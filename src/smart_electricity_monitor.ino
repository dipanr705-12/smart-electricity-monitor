/*
 * ============================================================
 *  SMART ELECTRICITY MONITORING SYSTEM
 * ============================================================
 *  Hardware : ESP32 + PZEM-004T v3.0 + 0.96" OLED (SSD1306)
 *  Protocol : MQTT over Wi-Fi
 *  Dashboard: Node-RED / MQTT Explorer / Home Assistant
 * ============================================================
 *  Libraries required (install via Arduino Library Manager):
 *    - PZEM004Tv30   by olehs      (search "PZEM-004T")
 *    - Adafruit SSD1306             (search "SSD1306")
 *    - Adafruit GFX Library         (search "Adafruit GFX")
 *    - PubSubClient  by knolleary   (search "PubSubClient")
 *    - ArduinoJson   by bblanchon   (search "ArduinoJson")
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PZEM004Tv30.h>
#include <Preferences.h>  // NVS – persist cumulative energy across resets

// ─────────────────────────────────────────────
//  USER CONFIGURATION  ← edit these
// ─────────────────────────────────────────────
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

#define MQTT_BROKER     "192.168.1.100"   // your broker IP or hostname
#define MQTT_PORT       1883
#define MQTT_USER       ""                // leave blank if no auth
#define MQTT_PASS       ""
#define MQTT_CLIENT_ID  "esp32-energy-meter"

// MQTT Topics
#define TOPIC_DATA      "home/energy/data"
#define TOPIC_ALERT     "home/energy/alert"
#define TOPIC_STATUS    "home/energy/status"
#define TOPIC_RESET     "home/energy/reset"   // subscribe – reset energy counter

// Alert threshold
#define POWER_ALERT_W   2000.0f   // alert when instantaneous power > 2000 W
#define ENERGY_ALERT_KWH 10.0f   // alert when session energy > 10 kWh

// Intervals (ms)
#define SAMPLE_INTERVAL_MS   2000
#define MQTT_PUBLISH_MS      5000
#define DISPLAY_CYCLE_MS     3000

// ─────────────────────────────────────────────
//  PIN MAPPING
// ─────────────────────────────────────────────
// PZEM-004T v3 uses UART – connect via HardwareSerial
//   PZEM TX → ESP32 GPIO16 (RX2)
//   PZEM RX → ESP32 GPIO17 (TX2)  (through 5V↔3.3V level shifter)
#define PZEM_RX_PIN  16
#define PZEM_TX_PIN  17

// OLED on I²C
//   SDA → GPIO21,  SCL → GPIO22  (ESP32 default I²C)
#define OLED_WIDTH   128
#define OLED_HEIGHT   64
#define OLED_ADDR    0x3C

// Built-in LED for status blink
#define STATUS_LED   2

// ─────────────────────────────────────────────
//  GLOBALS
// ─────────────────────────────────────────────
HardwareSerial pzemSerial(2);               // UART2
PZEM004Tv30    pzem(&pzemSerial, PZEM_RX_PIN, PZEM_TX_PIN);
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

WiFiClient     wifiClient;
PubSubClient   mqtt(wifiClient);
Preferences    prefs;

// Live readings
struct Readings {
  float voltage     = 0;   // V
  float current     = 0;   // A
  float power       = 0;   // W
  float energy      = 0;   // kWh (from PZEM)
  float frequency   = 0;   // Hz
  float powerFactor = 0;
  bool  valid       = false;
};

Readings latest;

// Cumulative energy (persisted in NVS)
float sessionEnergyKWh = 0.0f;

// Timers
unsigned long lastSample  = 0;
unsigned long lastPublish = 0;
unsigned long lastDisplay = 0;

uint8_t displayPage = 0;   // rotate display pages

bool powerAlertSent  = false;
bool energyAlertSent = false;

// ─────────────────────────────────────────────
//  FUNCTION PROTOTYPES
// ─────────────────────────────────────────────
void connectWiFi();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int len);
void readPZEM();
void publishData();
void publishAlert(const char* message);
void updateDisplay();
void drawPage0();   // Voltage / Current
void drawPage1();   // Power / Energy
void drawPage2();   // Frequency / PF / Status
void blinkLED(int times, int delayMs);

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Electricity Monitor ===");

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  // NVS – restore session energy
  prefs.begin("energy", false);
  sessionEnergyKWh = prefs.getFloat("session_kwh", 0.0f);
  Serial.printf("Restored session energy: %.4f kWh\n", sessionEnergyKWh);

  // OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed!");
  } else {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(10, 20);
    oled.println("  Energy Monitor");
    oled.setCursor(10, 36);
    oled.println("  Initialising...");
    oled.display();
  }

  // Wi-Fi
  connectWiFi();

  // MQTT
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
  connectMQTT();

  // PZEM serial (already configured by library constructor)
  Serial.println("PZEM-004T ready.");

  blinkLED(3, 150);
}

// ─────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────
void loop() {
  // Keep MQTT alive
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  unsigned long now = millis();

  // Sample PZEM
  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = now;
    readPZEM();
  }

  // Publish to MQTT
  if (now - lastPublish >= MQTT_PUBLISH_MS) {
    lastPublish = now;
    if (latest.valid) publishData();
  }

  // Rotate OLED pages
  if (now - lastDisplay >= DISPLAY_CYCLE_MS) {
    lastDisplay = now;
    displayPage = (displayPage + 1) % 3;
    updateDisplay();
  }
}

// ─────────────────────────────────────────────
//  WI-FI
// ─────────────────────────────────────────────
void connectWiFi() {
  Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
    blinkLED(2, 100);
  } else {
    Serial.println("\nWiFi failed – running offline.");
  }
}

// ─────────────────────────────────────────────
//  MQTT
// ─────────────────────────────────────────────
void connectMQTT() {
  int attempts = 0;
  while (!mqtt.connected() && attempts < 5) {
    Serial.print("MQTT connecting...");
    bool ok = (strlen(MQTT_USER) > 0)
      ? mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS,
                     TOPIC_STATUS, 1, true, "{\"online\":false}")
      : mqtt.connect(MQTT_CLIENT_ID, nullptr, nullptr,
                     TOPIC_STATUS, 1, true, "{\"online\":false}");
    if (ok) {
      Serial.println(" connected.");
      mqtt.subscribe(TOPIC_RESET);
      // Publish online status (retained)
      mqtt.publish(TOPIC_STATUS, "{\"online\":true}", true);
    } else {
      Serial.printf(" failed (rc=%d). Retry in 3 s.\n", mqtt.state());
      delay(3000);
    }
    attempts++;
  }
}

// MQTT incoming message handler
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("MQTT [%s]: %s\n", topic, msg.c_str());

  // Reset energy counter command
  if (String(topic) == TOPIC_RESET && msg == "RESET") {
    pzem.resetEnergy();
    sessionEnergyKWh = 0.0f;
    prefs.putFloat("session_kwh", 0.0f);
    powerAlertSent  = false;
    energyAlertSent = false;
    Serial.println("Energy counter reset.");
    mqtt.publish(TOPIC_ALERT, "{\"type\":\"info\",\"msg\":\"Energy counter reset\"}");
  }
}

// ─────────────────────────────────────────────
//  PZEM-004T READING
// ─────────────────────────────────────────────
void readPZEM() {
  float v  = pzem.voltage();
  float i  = pzem.current();
  float p  = pzem.power();
  float e  = pzem.energy();   // kWh on PZEM internal counter
  float hz = pzem.frequency();
  float pf = pzem.pf();

  if (isnan(v) || isnan(i) || isnan(p)) {
    Serial.println("PZEM read error – check wiring.");
    latest.valid = false;
    return;
  }

  latest.voltage     = v;
  latest.current     = i;
  latest.power       = p;
  latest.energy      = e;
  latest.frequency   = hz;
  latest.powerFactor = pf;
  latest.valid       = true;

  // Accumulate session energy into NVS (save every 10 readings ~20 s)
  static uint8_t saveCount = 0;
  sessionEnergyKWh = e;   // PZEM holds cumulative; just mirror it
  if (++saveCount >= 10) {
    saveCount = 0;
    prefs.putFloat("session_kwh", sessionEnergyKWh);
  }

  Serial.printf("[PZEM] V:%.1fV  I:%.3fA  P:%.1fW  E:%.4fkWh  Hz:%.1f  PF:%.2f\n",
                v, i, p, e, hz, pf);

  // ── Threshold Alerts ──────────────────────
  if (p > POWER_ALERT_W && !powerAlertSent) {
    powerAlertSent = true;
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"power\",\"msg\":\"Power exceeded %.0f W\",\"value\":%.1f}",
             POWER_ALERT_W, p);
    publishAlert(buf);
  } else if (p <= POWER_ALERT_W) {
    powerAlertSent = false;
  }

  if (e > ENERGY_ALERT_KWH && !energyAlertSent) {
    energyAlertSent = true;
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"energy\",\"msg\":\"Energy exceeded %.1f kWh\",\"value\":%.4f}",
             ENERGY_ALERT_KWH, e);
    publishAlert(buf);
  }
}

// ─────────────────────────────────────────────
//  MQTT PUBLISH
// ─────────────────────────────────────────────
void publishData() {
  StaticJsonDocument<256> doc;
  doc["voltage"]      = round(latest.voltage * 10) / 10.0;
  doc["current"]      = round(latest.current * 1000) / 1000.0;
  doc["power"]        = round(latest.power * 10) / 10.0;
  doc["energy_kwh"]   = round(latest.energy * 10000) / 10000.0;
  doc["frequency"]    = round(latest.frequency * 10) / 10.0;
  doc["power_factor"] = round(latest.powerFactor * 100) / 100.0;
  doc["timestamp"]    = millis() / 1000;

  // Estimated cost: assume ₹8/kWh (India) – adjust as needed
  doc["est_cost_inr"] = round(latest.energy * 8.0 * 100) / 100.0;

  char buf[256];
  serializeJson(doc, buf);
  mqtt.publish(TOPIC_DATA, buf, false);
  Serial.printf("Published: %s\n", buf);
}

void publishAlert(const char* json) {
  mqtt.publish(TOPIC_ALERT, json, false);
  Serial.printf("ALERT sent: %s\n", json);
  blinkLED(5, 80);
}

// ─────────────────────────────────────────────
//  OLED DISPLAY
// ─────────────────────────────────────────────
void updateDisplay() {
  oled.clearDisplay();
  if (!latest.valid) {
    oled.setCursor(10, 28);
    oled.setTextSize(1);
    oled.println("No PZEM signal...");
    oled.display();
    return;
  }
  switch (displayPage) {
    case 0: drawPage0(); break;
    case 1: drawPage1(); break;
    case 2: drawPage2(); break;
  }
  oled.display();
}

// Page 0 – Voltage & Current
void drawPage0() {
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("-- Voltage & Current --");

  oled.setTextSize(2);
  oled.setCursor(0, 14);
  oled.printf("%.1fV", latest.voltage);

  oled.setTextSize(2);
  oled.setCursor(0, 38);
  oled.printf("%.3fA", latest.current);
}

// Page 1 – Power & Energy
void drawPage1() {
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("-- Power & Energy --");

  oled.setTextSize(2);
  oled.setCursor(0, 14);
  oled.printf("%.1fW", latest.power);

  oled.setTextSize(1);
  oled.setCursor(0, 38);
  oled.printf("Energy: %.4f kWh", latest.energy);
  oled.setCursor(0, 52);
  oled.printf("Cost: Rs%.2f", latest.energy * 8.0f);
}

// Page 2 – Frequency, PF, Wi-Fi
void drawPage2() {
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("-- Line Quality --");

  oled.setCursor(0, 14);
  oled.printf("Freq : %.1f Hz", latest.frequency);
  oled.setCursor(0, 26);
  oled.printf("PF   : %.2f", latest.powerFactor);
  oled.setCursor(0, 38);
  oled.printf("WiFi : %s",
              WiFi.status() == WL_CONNECTED ? "OK" : "OFF");
  oled.setCursor(0, 50);
  if (WiFi.status() == WL_CONNECTED)
    oled.printf("IP: %s", WiFi.localIP().toString().c_str());
}

// ─────────────────────────────────────────────
//  UTILITY
// ─────────────────────────────────────────────
void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(delayMs);
    digitalWrite(STATUS_LED, LOW);
    delay(delayMs);
  }
}
