// =====================================================
// GreenHouse ESP32 Firmware (Refactored)
// PlatformIO / Arduino Framework
// =====================================================
// Features:
// - WiFi provisioning via AP mode (config portal)
// - Server sync (config fetch + state upload)
// - Autonomous greenhouse control logic
// - Sensor management (soil, water, SHT4x, BH1750)
// - OLED status display
// - Actuator scheduling (pump, fan, humidifier, light)
// =====================================================

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <Adafruit_SHT4x.h>
#include <BH1750.h>
#include <Adafruit_SSD1306.h>

#include "esp_task_wdt.h"
#include "bitmaps/ok.h"
#include "bitmaps/no_water.h"

// =====================================================
// FOR debugging without sensors (uncomment to enable)
// =====================================================
#define DEBUG 1
#define EXHIBITION_MODE 0

#if DEBUG
#define LOGI(x) Serial.println(String("[INFO] ") + x)
#define LOGW(x) Serial.println(String("[WARN] ") + x)
#define LOGE(x) Serial.println(String("[ERROR] ") + x)

#define LOGF(...) Serial.printf(__VA_ARGS__)
#else
#define LOGI(x)
#define LOGW(x)
#define LOGE(x)
#define LOGF(...)
#endif

// =====================================================
// EEPROM
// =====================================================
#define EEPROM_SIZE 512

// =====================================================
// PIN MAPPING (HARDWARE LAYER)
// =====================================================
#define SDA_PIN 21
#define SCL_PIN 22

#define SOIL_POWER 23
#define SOIL_ADC 34

#define WATER_POWER 19
#define WATER_ADC 35

#define PUMP_PIN 14
#define FAN_PIN 27
#define HUM_PIN 26
#define LIGHT_PIN 25

#define BOOT_BUTTON 0

#define SETTINGS_MAGIC 0xA1B2C3D4

enum SystemMode
{
  MODE_STARTUP,
  MODE_CONNECTING_WIFI,
  MODE_AP_CONFIG,
  MODE_ONLINE,
  MODE_OFFLINE
};

SystemMode systemMode = MODE_STARTUP;

// =====================================================
// SETTINGS (stored in EEPROM)
// =====================================================
struct Settings
{
  uint32_t magic; // защита EEPROM
  char ssid[32];
  char password[64];
  char serverIp[16];
  int serverPort;
  char deviceName[32];
  bool configured;
};

Settings settings;

// =====================================================
// RUNTIME CONFIG (from server)
// =====================================================
struct Config
{
  uint16_t lightThresholdLux = 300;
  uint32_t lightForceIntervalMin = 180;
  uint32_t lightForceDurationSec = 300;

  uint16_t soilDryThreshold = 2100;
  uint16_t soilWetThreshold = 1800;

  float emaAlpha = 0.12;

  uint32_t pumpRunSec = 5;
  uint32_t pumpRecoverySec = 10;

  uint32_t fanIntervalMin = 30;
  uint32_t fanRunSec = 20;

  uint32_t humIntervalMin = 45;
  uint32_t humRunSec = 15;
};

Config getDefaultConfig();

Config cfg = getDefaultConfig();

Config getDefaultConfig()
{
  Config c;

  c.lightThresholdLux = 300;
  c.lightForceIntervalMin = 180;
  c.lightForceDurationSec = 300;

  c.soilDryThreshold = 2100;
  c.soilWetThreshold = 1800;

  c.emaAlpha = 0.12;

  c.pumpRunSec = 5;
  c.pumpRecoverySec = 10;

  c.fanIntervalMin = 30;
  c.fanRunSec = 20;

  c.humIntervalMin = 45;
  c.humRunSec = 15;

  return c;
}

// =====================================================
// WEB SERVER
// =====================================================
AsyncWebServer server(80);

// =====================================================
// SYSTEM STATE
// =====================================================
bool configMode = false;
unsigned long configStart = 0;
const unsigned long CONFIG_TIMEOUT = 300000;
static uint32_t lastAlive = 0;
static uint32_t lastDebug = 0;
int lastScanCount = 0;
unsigned long lastScanTime = 0;
String scanCache = "{\"networks\":[]}";
uint32_t tScan = 0;
bool scanning = false;
static bool serverStarted = false;
bool luxValid = false;
uint32_t tLightCooldown = 0;
uint8_t syncFails = 0;

// sensor values
float temperature = 0, humidity = 0, lux = 0;
int soilValue = 0, waterValue = 0;
float soilFiltered = 0;
bool soilInit = false;
bool bh1750_enabled = false;
bool sht_enabled = false;

// timers
uint32_t tSensors = 0;
uint32_t tDisplay = 0;
uint32_t tWiFiCheck = 0;
uint32_t tLightForce = 0;
uint32_t tFan = 0;
uint32_t tHum = 0;
uint32_t tPumpLock = 0;
uint32_t tServerSync = 0;
uint16_t waterEmptyThreshold = 300;
// =====================================================
// DEVICES
// =====================================================
Adafruit_SHT4x sht4;
BH1750 lightSensor;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// =====================================================
// ACTUATORS
// =====================================================
struct Actuator
{
  bool active = false;
  uint32_t start = 0;
  uint32_t duration = 0;
};

Actuator pump, fan, hum, lightAct;

// OLED display mode control
uint32_t tOledMode = 0;
bool oledShowSensors = true;

// =====================================================
// FORWARD DECLARATIONS
// =====================================================
void handleRoot(AsyncWebServerRequest *request);
void handleSave(AsyncWebServerRequest *request);
void handleScan(AsyncWebServerRequest *request);
void saveSettings();

// =====================================================
// STORAGE LAYER (EEPROM)
// =====================================================

// Load persistent WiFi/server configuration
void loadSettings()
{
  EEPROM.get(0, settings);

  if (settings.magic != SETTINGS_MAGIC)
  {
    Serial.println("EEPROM invalid → resetting settings");

    memset(&settings, 0, sizeof(settings));
    settings.magic = SETTINGS_MAGIC;
    settings.configured = false;

    saveSettings();
  }
}

// Save persistent configuration to flash
void saveSettings()
{
  settings.magic = SETTINGS_MAGIC;
  EEPROM.put(0, settings);
  EEPROM.commit();
  delay(50);
}

// =====================================================
// WIFI LAYER
// =====================================================
void updateWiFiScan()
{
  if (millis() - tScan < 15000)
    return;

  tScan = millis();

  int status = WiFi.scanComplete();

  if (status == WIFI_SCAN_RUNNING)
    return;

  if (status >= 0)
  {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.createNestedArray("networks");

    for (int i = 0; i < status; i++)
    {
      JsonObject o = arr.createNestedObject();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
    }

    scanCache = "";
    serializeJson(doc, scanCache);

    WiFi.scanDelete();
  }

  scanning = true;
  WiFi.scanNetworks(true);
}

void feedWDT()
{
  esp_task_wdt_reset();
  delay(1);
}

// Connect to WiFi using stored credentials
bool connectWiFi()
{
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.ssid, settings.password);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
  {
    esp_task_wdt_reset();
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    LOGI("WiFi connected");
    LOGI(WiFi.localIP().toString());
    return true;
  }

  LOGW("WiFi connect failed");
  return false;
}

// =====================================================
// SERVER COMMUNICATION
// =====================================================

// Fetch runtime greenhouse config from backend server
bool fetchPlantConfig()
{
  WiFiClient client;
  client.setTimeout(3000);

  if (WiFi.status() != WL_CONNECTED)
    return false;

  HTTPClient http;
  http.setTimeout(3000);
  http.setReuse(false);

  String url = "http://" + String(settings.serverIp) + ":" +
               String(settings.serverPort) + "/plant-config";

  if (!http.begin(client, url))
    return false;

  unsigned long start = millis();

  int code = http.GET();

  if (millis() - start > 4000)
  {
    http.end();
    return false;
  }

  if (code != 200)
  {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<2048> doc;
  if (deserializeJson(doc, payload))
    return false;

  Config def = getDefaultConfig();

  // update config safely with fallback defaults
  cfg.lightThresholdLux = doc["light"]["threshold"] | def.lightThresholdLux;
  cfg.lightForceIntervalMin = doc["light"]["force_interval"] | def.lightForceIntervalMin;
  cfg.lightForceDurationSec = doc["light"]["force_duration"] | def.lightForceDurationSec;

  cfg.soilDryThreshold = doc["soil"]["dry"] | def.soilDryThreshold;
  cfg.soilWetThreshold = doc["soil"]["wet"] | def.soilWetThreshold;
  cfg.emaAlpha = doc["soil"]["ema"] | def.emaAlpha;

  cfg.pumpRunSec = doc["pump"]["run"] | def.pumpRunSec;
  cfg.pumpRecoverySec = doc["pump"]["cooldown"] | def.pumpRecoverySec;

  cfg.fanIntervalMin = doc["fan"]["interval"] | def.fanIntervalMin;
  cfg.fanRunSec = doc["fan"]["run"] | def.fanRunSec;

  cfg.humIntervalMin = doc["hum"]["interval"] | def.humIntervalMin;
  cfg.humRunSec = doc["hum"]["run"] | def.humRunSec;

  return true;
}

// Send current sensor + actuator state to backend
void sendStateToServer()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  esp_task_wdt_reset();

  WiFiClient client;
  client.setTimeout(3000);

  if (WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;

  String url = "http://" + String(settings.serverIp) + ":" +
               String(settings.serverPort) + "/state";

  http.setTimeout(3000);
  http.setReuse(false);
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> doc;

  doc["device"] = settings.deviceName;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["lux"] = lux;
  doc["soil"] = soilValue;
  doc["water"] = waterValue;

  doc["pump"] = pump.active;
  doc["fan"] = fan.active;
  doc["hum"] = hum.active;
  doc["light"] = lightAct.active;

  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis();

  String json;
  serializeJson(doc, json);

  int code = http.POST(json);

  Serial.println(code > 0 ? "STATE sent OK" : "STATE send error");

  http.end();
}

// =====================================================
// CONFIG PORTAL MODE (AP)
// =====================================================

// Start WiFi Access Point + async web server
void startConfigMode()
{
  configMode = true;
  configStart = millis();

  WiFi.mode(WIFI_AP);
  WiFi.softAP("Greenhouse_Config", "12345678");

  Serial.println(WiFi.softAPIP());

  if (!serverStarted)
  {
    server.on("/", HTTP_GET, handleRoot);      // UI page
    server.on("/scan", HTTP_GET, handleScan);  // WiFi scan
    server.on("/save", HTTP_POST, handleSave); // save credentials

    server.begin();
    serverStarted = true;
  }
}

void resetWiFiSettings()
{
  memset(&settings, 0, sizeof(settings));
  settings.magic = SETTINGS_MAGIC;
  settings.configured = false;

  saveSettings();

  LOGI("WiFi settings RESET");
  ESP.restart();
}

void checkBootReset()
{
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  uint32_t t0 = millis();

  while (digitalRead(BOOT_BUTTON) == LOW)
  {
    delay(50);

    if (millis() - t0 > 3000)
    {
      LOGW("BOOT long press → WiFi RESET");
      resetWiFiSettings();
    }
  }
}

// =====================================================
// ACTUATORS CONTROL
// =====================================================

// Start actuator for defined duration
void startActuator(Actuator &a, int pin, uint32_t sec)
{
  if (a.active)
    return;

  sec = constrain(sec, 1UL, 3600UL);

  digitalWrite(pin, HIGH);
  a.active = true;
  a.start = millis();
  a.duration = sec * 1000;
}

// Non-blocking actuator update
void updateActuator(Actuator &a, int pin)
{
  if (a.active && millis() - a.start >= a.duration)
  {
    digitalWrite(pin, LOW);
    a.active = false;
  }
}

// Emergency shutdown for safety
void emergencyShutdown()
{
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(HUM_PIN, LOW);
  digitalWrite(LIGHT_PIN, LOW);

  pump.active = false;
  fan.active = false;
  hum.active = false;
  lightAct.active = false;
}

// =====================================================
// SENSORS
// =====================================================

// Read soil moisture (with power gating + EMA filter)
int readSoil()
{
  esp_task_wdt_reset();

  digitalWrite(SOIL_POWER, HIGH);
  delay(20);

  int raw = 0;
  for (int i = 0; i < 5; i++)
  {
    raw += analogRead(SOIL_ADC);
  }
  raw /= 5;

  digitalWrite(SOIL_POWER, LOW);

  if (!soilInit)
  {
    soilFiltered = raw;
    soilInit = true;
  }
  else
  {
    soilFiltered = cfg.emaAlpha * raw + (1 - cfg.emaAlpha) * soilFiltered;

    if (isnan(soilFiltered) || soilFiltered == 0)
      soilFiltered = raw;
  }

  return (int)soilFiltered;
}

// Read water level sensor
int readWater()
{
  esp_task_wdt_reset();

  digitalWrite(WATER_POWER, HIGH);
  delay(5);

  int v = analogRead(WATER_ADC);

  digitalWrite(WATER_POWER, LOW);

  return v;
}

bool isValidADC(int v)
{
  return v > 0 && v < 4095;
}

bool safeSHT4Read(float &t, float &h)
{
  if (!sht_enabled)
    return false;

  esp_task_wdt_reset();

  sensors_event_t humidityEvent, tempEvent;

  if (!sht4.getEvent(&humidityEvent, &tempEvent))
    return false;

  t = tempEvent.temperature;
  h = humidityEvent.relative_humidity;

  return true;
}

float safeLuxRead()
{
  esp_task_wdt_reset();

  float l = 0;

  if (bh1750_enabled)
  {
    l = lightSensor.readLightLevel();
    luxValid = true;
  }
  else
  {
    l = 0;
  }

  if (l < 0 || l > 200000)
    return 0;

  return l;
}

// Update all sensor readings
void updateSensors()
{
  float t, h;

  if (safeSHT4Read(t, h))
  {
    temperature = t;
    humidity = h;
  }
  else
  {
    LOGW("SHT4x read failed");
    temperature = 0;
    humidity = 0;
  }

  esp_task_wdt_reset(); // 🔥 важно перед BH1750

  lux = safeLuxRead();

  soilValue = readSoil();

  if (!isValidADC(soilValue))
  {
    soilValue = cfg.soilWetThreshold; // безопасное значение
  }

  waterValue = readWater();

  LOGF("[SENS] T=%.1f H=%.1f Lux=%.0f Soil=%d Water=%d\n",
       temperature, humidity, lux, soilValue, waterValue);
}

// =====================================================
// CONTROL LOGIC (GREENHOUSE AI)
// =====================================================

bool hasWater()
{
  return waterValue > waterEmptyThreshold;
}

void controlLogic(uint32_t now)
{
  esp_task_wdt_reset();

  // periodic forced lighting cycle
  if (now - tLightForce > cfg.lightForceIntervalMin * 60000)
  {
    tLightForce = now;
    startActuator(lightAct, LIGHT_PIN, cfg.lightForceDurationSec);
  }

  if (!lightAct.active &&
      lux < cfg.lightThresholdLux &&
      now - tLightCooldown > 300000)
  {
    startActuator(lightAct,
                  LIGHT_PIN,
                  cfg.lightForceDurationSec);

    tLightCooldown = now;
  }

  // soil dryness trigger -> pump
  if (!pump.active &&
      hasWater() &&
      soilValue > cfg.soilDryThreshold &&
      now - tPumpLock > cfg.pumpRecoverySec * 1000)
  {

    startActuator(pump, PUMP_PIN, cfg.pumpRunSec);
    tPumpLock = now;
  }

  if (!hasWater() && pump.active)
  {
    digitalWrite(PUMP_PIN, LOW);
    pump.active = false;
    tPumpLock = now;
  }

  // periodic fan
  if (now - tFan > cfg.fanIntervalMin * 60000)
  {
    tFan = now;
    startActuator(fan, FAN_PIN, cfg.fanRunSec);
  }

  // periodic humidifier
  if (now - tHum > cfg.humIntervalMin * 60000)
  {
    tHum = now;
    startActuator(hum, HUM_PIN, cfg.humRunSec);
  }

  if (pump.active)
  {
    LOGI("Pump active");
  }
}

// =====================================================
// DISPLAY (OLED)
// =====================================================

void updateDisplay()
{
  esp_task_wdt_reset();

  display.clearDisplay();
  display.setCursor(0, 0);

  if (oledShowSensors)
  {
    // ---------------- SENSOR SCREEN ----------------
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.printf(
        "Air: %.1fC\nHum: %.1f%%\nLux: %.0f\nSoil: %d\nWater: %d\nPump:%d Fan:%d",
        temperature, humidity, lux,
        soilValue, waterValue,
        pump.active, fan.active);
  }
  else
  {
    // ---------------- IMAGE SCREEN ----------------

    bool noWater = (waterValue < waterEmptyThreshold); // threshold placeholder

    if (noWater)
    {
      // show NO WATER image
      display.drawBitmap(0, 0, img_ok, 128, 64, 1);
    }
    else
    {
      // show OK image
      display.drawBitmap(0, 0, img_no_water, 128, 64, 1);
    }
  }

  display.display();
}

// =====================================================
// WEB HANDLERS (CONFIG PORTAL)
// =====================================================

void handleRoot(AsyncWebServerRequest *request)
{

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>GreenHouse Setup</title>
<style>
body { font-family: Arial; background:#111; color:#eee; padding:20px; }
input, select { width:100%; padding:10px; margin:5px 0; }
button { padding:10px; width:100%; background:#4CAF50; color:white; border:none; }
.card { background:#222; padding:15px; border-radius:10px; }
</style>
</head>
<body>
<div class='card'>
<h2>GreenHouse WiFi Setup</h2>

<form action='/save' method='POST'>

<label>WiFi Network</label>
<select name='ssid' id='ssid'></select>

<label>Password</label>
<input name='password' type='password'>

<label>Server IP</label>
<input name='serverIp' placeholder='192.168.1.100'>

<label>Server Port</label>
<input name='serverPort' value='80'>

<label>Device Name</label>
<input name='deviceName' value='GreenHouse-01'>

<button type='submit'>Save & Reboot</button>
</form>
</div>

<script>
fetch('/scan')
.then(r => r.json())
.then(data => {
  let select = document.getElementById('ssid');
  data.networks.forEach(n => {
    let opt = document.createElement('option');
    opt.value = n.ssid;
    opt.text = n.ssid + ' (' + n.rssi + 'dBm)';
    select.appendChild(opt);
  });
});
</script>

</body>
</html>
)rawliteral";

  request->send(200, "text/html", html);
}

// Scan available WiFi networks
void handleScan(AsyncWebServerRequest *request)
{
  updateWiFiScan();
  request->send(200, "application/json", scanCache);
}

// Save WiFi credentials to EEPROM
void handleSave(AsyncWebServerRequest *request)
{

  if (!request->hasParam("ssid", true) ||
      !request->hasParam("password", true))
  {
    request->send(400, "text/plain", "Missing params");
    return;
  }

  strlcpy(settings.ssid, request->getParam("ssid", true)->value().c_str(), sizeof(settings.ssid));
  strlcpy(settings.password, request->getParam("password", true)->value().c_str(), sizeof(settings.password));
  strlcpy(settings.serverIp, request->getParam("serverIp", true)->value().c_str(), sizeof(settings.serverIp));

  IPAddress ip;
  if (!ip.fromString(settings.serverIp))
  {
    request->send(400, "text/plain", "Invalid IP");
    return;
  }

  settings.serverPort = request->hasParam("serverPort", true)
                            ? request->getParam("serverPort", true)->value().toInt()
                            : 80;

  if (request->hasParam("deviceName", true))
  {
    strlcpy(settings.deviceName,
            request->getParam("deviceName", true)->value().c_str(),
            sizeof(settings.deviceName));
  }
  else
  {
    strlcpy(settings.deviceName,
            "GreenHouse-01",
            sizeof(settings.deviceName));
  }

  settings.configured = true;

  saveSettings();
  request->send(200, "text/plain", "Saved. Rebooting...");

  xTaskCreate([](void *)
              {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP.restart(); }, "reboot", 2048, nullptr, 1, nullptr);
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);
  LOGI("========== GREENHOUSE START ==========");
  LOGI("Booting system...");

  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_ADC, ADC_11db);
  analogSetPinAttenuation(WATER_ADC, ADC_11db);

  EEPROM.begin(EEPROM_SIZE);
  checkBootReset();
  loadSettings();
  LOGI("EEPROM loaded");

  // GPIO setup
  pinMode(SOIL_POWER, OUTPUT);
  pinMode(WATER_POWER, OUTPUT);

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(HUM_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);

  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(HUM_PIN, LOW);
  digitalWrite(LIGHT_PIN, LOW);

  // I2C sensors
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setTimeOut(50);

  sht_enabled = sht4.begin(); // SHT4x temp/humidity init

  if (sht_enabled)
  {
    LOGI("SHT4x OK");
  }
  else
  {
    LOGW("SHT4x NOT FOUND");
  }

  bh1750_enabled = lightSensor.begin(); // BH1750 light sensor init

  if (bh1750_enabled)
  {
    lightSensor.configure(BH1750::CONTINUOUS_HIGH_RES_MODE);
    LOGI("BH1750 OK");
  }
  else
  {
    LOGW("BH1750 NOT FOUND → DISABLED");
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    LOGW("OLED NOT FOUND");
  }
  else
  {
    LOGI("OLED OK");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.cp437(true);
    display.println("OLED OK");
    display.display();
    delay(3000);
  }

  // watchdog (prevents freeze)
  esp_task_wdt_init(60, true);
  esp_task_wdt_add(NULL);

  // ==============================
  // WIFI STATE DECISION
  // ==============================
  systemMode = MODE_CONNECTING_WIFI;

  if (settings.configured)
  {
    LOGI("Checking WiFi...");
    if (connectWiFi())
    {
      systemMode = MODE_ONLINE;
      fetchPlantConfig();
    }
    else
    {
      LOGW("WiFi failed -> AP fallback");
      startConfigMode();
      systemMode = MODE_AP_CONFIG;
    }
  }
  else
  {
    WiFi.scanNetworks(true);
    startConfigMode();
    systemMode = MODE_AP_CONFIG;
  }

  if (systemMode == MODE_ONLINE)
    LOGI("MODE: ONLINE");
  if (systemMode == MODE_OFFLINE)
    LOGI("MODE: OFFLINE");
  if (systemMode == MODE_AP_CONFIG)
    LOGI("MODE: AP CONFIG");
}

void exhibitionRoutine(uint32_t now)
{
  static uint8_t stage = 0;
  static uint32_t tStage = 0;

  if (now - tStage < 10000)
    return;

  tStage = now;

  switch (stage)
  {
  case 0:
    startActuator(lightAct, LIGHT_PIN, 5);
    break;

  case 1:
    startActuator(fan, FAN_PIN, 5);
    break;

  case 2:
    startActuator(hum, HUM_PIN, 5);
    break;

  case 3:
    if (hasWater())
      startActuator(pump, PUMP_PIN, 2);
    break;
  }

  stage = (stage + 1) % 4;
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{
  if (Serial.available())
  {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "resetwifi")
    {
      resetWiFiSettings();
    }
  }

  uint32_t now = millis();

  feedWDT();

  if (!scanning && now - tScan > 15000)
  {
    updateWiFiScan();
  }

  if (now - lastAlive > 1000)
  {
    lastAlive = now;
    esp_task_wdt_reset();
  }

  if (now - lastDebug > 3000)
  {
    lastDebug = now;

    LOGF("[STATUS] mode=%d wifi=%d soil=%d water=%d lux=%.1f\n",
         systemMode,
         WiFi.status(),
         soilValue,
         waterValue,
         lux);
  }

  // ==============================
  // AP CONFIG MODE
  // ==============================
  if (systemMode == MODE_AP_CONFIG)
  {
    if (millis() - configStart > CONFIG_TIMEOUT)
    {
      ESP.restart();
    }

    if (now - tOledMode >= 20000)
    {
      tOledMode = now;
      oledShowSensors = !oledShowSensors;
    }

    // OLED continues to work in AP mode
    if (now - tDisplay > 1000)
    {
      tDisplay = now;
      updateDisplay();
    }

    esp_task_wdt_reset();
    return;
  }

  // ==============================
  // WIFI MONITOR
  // ==============================
  if (systemMode == MODE_OFFLINE)
  {
    static uint32_t tReconnect = 0;

    if (now - tReconnect > 30000)
    {
      tReconnect = now;

      LOGI("Trying reconnect WiFi");

      if (connectWiFi())
      {
        systemMode = MODE_ONLINE;
        fetchPlantConfig();
      }
    }
  }

  // ==============================
  // OFFLINE FALLBACK (ГАРАНТИЯ РАБОТЫ)
  // ==============================
  if (systemMode == MODE_OFFLINE)
  {
    // теплица работает автономно
  }

  // ==============================
  // SERVER SYNC ONLY ONLINE
  // ==============================
  if (systemMode == MODE_ONLINE)
  {
    if (now - tServerSync > 10000)
    {
      tServerSync = now;
      sendStateToServer();

      if (!fetchPlantConfig())
      {
        syncFails++;

        if (syncFails >= 5)
        {
          systemMode = MODE_OFFLINE;
        }
      }
      else
      {
        syncFails = 0;
      }
    }
  }

  // sensors + control loop
  if (now - tSensors > 2000)
  {
    tSensors = now;
    updateSensors(); // read all sensors

#if EXHIBITION_MODE
    exhibitionRoutine(now);
#else
    controlLogic(now);
#endif
  }

  // =====================================================
  // OLED MODE SWITCH (20s sensors / 20s image)
  // =====================================================
  if (now - tOledMode >= 20000)
  {
    tOledMode = now;
    oledShowSensors = !oledShowSensors;
  }

  if (now - tDisplay > 1000)
  {
    tDisplay = now;
    updateDisplay();
  }

  // actuator updates (non-blocking)
  updateActuator(pump, PUMP_PIN);
  updateActuator(fan, FAN_PIN);
  updateActuator(hum, HUM_PIN);
  updateActuator(lightAct, LIGHT_PIN);

  // watchdog reset
  esp_task_wdt_reset();
}
