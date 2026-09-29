/* ==========================================================
 *  Thai Oil Price Display  -  ENGLISH MODE
 *  ESP32 + ILI9341 320x240 TFT (Landscape)
 *  API : https://api.chnwt.dev/thai-oil-api/latest
 *
 *  BOOT button (GPIO0) = toggle PTT <-> Bangchak v 1.0
 * ==========================================================*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>

// ================= CONFIG =================
#define WIFI_SSID   "e27asy"
#define WIFI_PASS   "0896071707"

#define API_URL     "https://api.chnwt.dev/thai-oil-api/latest"
#define BTN_PIN     0                       // BOOT button
#define REFRESH_MS  (60UL * 60UL * 1000UL)  // refresh every 1 hour

const long GMT_OFFSET_SEC = 7 * 3600;       // Thailand UTC+7

// ================= COLORS =================
#define C_BG     0x0000
#define C_HEAD   0x018C
#define C_CARD   0x2124
#define C_LINE   0x4A69
#define C_LABEL  0xBDF7
#define C_PRICE  0x07E0
#define C_ERR    0xF800
#define C_ACCENT 0xFD20

// ================= GLOBALS =================
TFT_eSPI    tft  = TFT_eSPI();
TFT_eSprite card = TFT_eSprite(&tft);   // sprite per card (flicker-free)

const int CARD_W = 152, CARD_H = 56;
const int GRID_X = 4,   GRID_Y = 40;
const int GAP_X  = 8,   GAP_Y  = 4;

struct Fuel {
  const char* apiKey;     // key inside JSON object
  const char* en;         // label shown on screen
  float       price;
};

Fuel fuels[] = {
  { "gasohol_95",  "GSH 95",    -1 },
  { "gasohol_91",  "GSH 91",    -1 },
  { "gasohol_e20", "GSH E20",   -1 },
  { "gasohol_e85", "GSH E85",   -1 },
  { "diesel",      "DIESEL",    -1 },
  { "gasoline_95", "BENZIN 95", -1 }
};
const int N_FUEL = sizeof(fuels) / sizeof(fuels[0]);

String apiDate    = "-";
int    stationIdx = 0;                       // 0=ptt 1=bcp
const char* STATIONS[] = { "ptt", "bcp" };
const char* ST_NAME[]  = { "PTT", "BANGCHAK" };

unsigned long lastFetch = 0;

// ==========================================================
//  UI
// ==========================================================
void drawHeader() {
  tft.fillRect(0, 0, 320, 34, C_HEAD);
  tft.drawFastHLine(0, 34, 320, C_ACCENT);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, C_HEAD);
  tft.drawString("THAI OIL PRICE", 8, 17, 4);

  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(C_ACCENT, C_HEAD);
  tft.fillRect(180, 2, 136, 30, C_HEAD);
  tft.drawString(ST_NAME[stationIdx], 312, 17, 4);
}

void drawCard(int i) {
  int col = i % 2, row = i / 2;
  int x = GRID_X + col * (CARD_W + GAP_X);
  int y = GRID_Y + row * (CARD_H + GAP_Y);

  card.fillSprite(C_CARD);
  card.drawRoundRect(0, 0, CARD_W, CARD_H, 6, C_LINE);

  // --- fuel name ---
  card.setTextDatum(TL_DATUM);
  card.setTextColor(C_LABEL, C_CARD);
  card.drawString(fuels[i].en, 8, 5, 2);

  // --- price ---
  card.setTextDatum(BR_DATUM);
  if (fuels[i].price > 0) {
    card.setTextColor(C_PRICE, C_CARD);
    card.drawFloat(fuels[i].price, 2, CARD_W - 22, CARD_H - 4, 4);
    card.setTextColor(C_LABEL, C_CARD);
    card.drawString("B", CARD_W - 6, CARD_H - 6, 2);
  } else {
    card.setTextColor(C_ERR, C_CARD);
    card.drawString("--.--", CARD_W - 8, CARD_H - 4, 4);
  }

  card.pushSprite(x, y);
}

void drawAllCards() {
  for (int i = 0; i < N_FUEL; i++) drawCard(i);
}

void drawStatus(const String& msg, uint16_t color) {
  tft.fillRect(0, 222, 150, 18, C_BG);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, C_BG);
  tft.drawString(msg, 6, 231, 2);
}

void drawClock() {
  struct tm ti;
  if (!getLocalTime(&ti, 5)) return;
  char buf[24];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &ti);

  tft.fillRect(150, 222, 170, 18, C_BG);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(C_LABEL, C_BG);
  tft.drawString(buf, 314, 231, 2);
}

// ==========================================================
//  API
// ==========================================================
bool fetchOilPrice() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  HTTPClient https;
  if (!https.begin(client, API_URL)) return false;
  https.addHeader("User-Agent", "ESP32-OilDisplay/1.0");
  https.setTimeout(15000);

  int code = https.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[HTTP] error %d\n", code);
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[JSON] %s\n", err.c_str());
    return false;
  }

  JsonObject resp = doc["response"];
  if (resp.isNull()) { Serial.println("[API] no response object"); return false; }

  apiDate = resp["date"] | "-";

  // stations.<name> is an OBJECT, not an array
  JsonObject st = resp["stations"][STATIONS[stationIdx]];
  if (st.isNull()) { Serial.println("[API] station not found"); return false; }

  // clear old values first
  for (int i = 0; i < N_FUEL; i++) fuels[i].price = -1.0f;

  Serial.printf("\n=== %s | %s ===\n", ST_NAME[stationIdx], apiDate.c_str());

  for (JsonPair kv : st) {
    const char* key   = kv.key().c_str();
    const char* price = kv.value()["price"] | "";
    Serial.printf("  %-16s %s\n", key, price);

    for (int i = 0; i < N_FUEL; i++) {
      if (strcmp(key, fuels[i].apiKey) == 0) {
        fuels[i].price = atof(price);   // price arrives as a string
        break;
      }
    }
  }
  return true;
}

void refresh(const char* waitMsg) {
  drawStatus(waitMsg, C_ACCENT);
  if (fetchOilPrice()) {
    drawAllCards();
    drawStatus("OK " + apiDate.substring(0, 12), C_PRICE);
  } else {
    drawAllCards();
    drawStatus("Fetch failed", C_ERR);
  }
  lastFetch = millis();
}

// ==========================================================
//  SETUP
// ==========================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(BTN_PIN, INPUT_PULLUP);

  tft.init();
  tft.setRotation(3);              // landscape 320x240
  tft.fillScreen(C_BG);

  card.setColorDepth(16);
  card.createSprite(CARD_W, CARD_H);

  drawHeader();
  drawAllCards();

  // ---- WiFi ----
  drawStatus("Connecting WiFi...", C_ACCENT);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry++ < 40) { delay(500); Serial.print('.'); }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("IP: " + WiFi.localIP().toString());
    drawStatus("WiFi OK", C_PRICE);
  } else {
    drawStatus("WiFi FAILED", C_ERR);
  }

  configTime(GMT_OFFSET_SEC, 0, "pool.ntp.org", "time.nist.gov");

  refresh("Fetching...");
}

// ==========================================================
//  LOOP
// ==========================================================
void loop() {
  static unsigned long tClock = 0;
  static unsigned long tBtn   = 0;

  if (millis() - tClock > 1000) { drawClock(); tClock = millis(); }

  // toggle station
  if (digitalRead(BTN_PIN) == LOW && millis() - tBtn > 400) {
    tBtn = millis();
    stationIdx = (stationIdx + 1) % 2;
    for (int i = 0; i < N_FUEL; i++) fuels[i].price = -1.0f;
    drawHeader();
    drawAllCards();
    refresh("Switching...");
  }

  if (millis() - lastFetch > REFRESH_MS) refresh("Refreshing...");

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(3000);
  }
}