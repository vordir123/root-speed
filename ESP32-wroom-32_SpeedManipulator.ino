#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>

/* ---- Configuration pins ---- */
#define REED_PIN    4    // GPIO for reed switch
#define PWM_PIN     21   // PWM output to vehicle cluster
#define STATUS_LED  2
#define UART_BAUD   115200
#define SPEED_MAX   120.0f

/* ---- Globals ---- */
Preferences prefs;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
HardwareSerial &canSerial = Serial1; // connection to Teensy

struct Config {
  float wheelCirc;     // in meters
  uint8_t magnetCount;
  float scale;
  float offset;
  uint32_t pwmFreq;
  uint32_t pwmDuty;
  char wifiSsid[32];
  char wifiPass[32];
} cfg;

volatile unsigned long lastPulse = 0;
volatile unsigned long pulseInterval = 0;

/* Speed samples for graph */
const int HISTORY_LEN = 60;
float historyReal[HISTORY_LEN];
float historyFake[HISTORY_LEN];
int histIdx = 0;

/* Calibration */
bool calibrating = false;
unsigned long calibrateStart = 0;

/* ---- Utility ---- */
void logEvent(const String &msg){
  Serial.println(msg);
}

void IRAM_ATTR reedISR(){
  unsigned long now = micros();
  pulseInterval = now - lastPulse;
  lastPulse = now;
}

void loadConfig(){
  cfg.wheelCirc   = prefs.getFloat("wheel", 2.17f);
  cfg.magnetCount = prefs.getUChar("magnet", 1);
  cfg.scale       = prefs.getFloat("scale", 1.0f);
  cfg.offset      = prefs.getFloat("offset", -10.0f);
  cfg.pwmFreq     = prefs.getUInt("pwmf", 50);
  cfg.pwmDuty     = prefs.getUInt("pwmd", 128);
  prefs.getString("ssid", cfg.wifiSsid, sizeof(cfg.wifiSsid));
  prefs.getString("pass", cfg.wifiPass, sizeof(cfg.wifiPass));
}

void saveConfig(){
  prefs.putFloat("wheel", cfg.wheelCirc);
  prefs.putUChar("magnet", cfg.magnetCount);
  prefs.putFloat("scale", cfg.scale);
  prefs.putFloat("offset", cfg.offset);
  prefs.putUInt("pwmf", cfg.pwmFreq);
  prefs.putUInt("pwmd", cfg.pwmDuty);
  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("pass", cfg.wifiPass);
}

/* ---- Real-time calculation task ---- */
float realSpeed = 0.0f;
float fakeSpeed = 0.0f;

void calcTask(void *param){
  const TickType_t delay100 = pdMS_TO_TICKS(100);
  for(;;){
    unsigned long interval = pulseInterval;
    if(interval == 0 || (micros() - lastPulse) > 1000000){
      realSpeed = 0.0f;              // timeout
    } else {
      float revPerSec = 1000000.0f / interval / cfg.magnetCount;
      realSpeed = revPerSec * cfg.wheelCirc * 3.6f; // km/h
    }
    if(calibrating){
      fakeSpeed = 15.0f; // constant during calibration
    } else {
      fakeSpeed = realSpeed * cfg.scale + cfg.offset;
      if(fakeSpeed < 0) fakeSpeed = 0;
    }
    if(fakeSpeed > SPEED_MAX) fakeSpeed = SPEED_MAX;
    float freq = (fakeSpeed/3.6f)/(cfg.wheelCirc)*cfg.magnetCount;
    if(freq < 1) freq = 0;
    ledcWriteTone(0, freq);
    ledcWrite(0, cfg.pwmDuty);
    historyReal[histIdx] = realSpeed;
    historyFake[histIdx] = fakeSpeed;
    histIdx = (histIdx + 1) % HISTORY_LEN;
    vTaskDelay(delay100);
  }
}

/* ---- WebSocket handling ---- */
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client,
               AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    logEvent("WS client connected");
  }
}

void wsTask(void *param){
  const TickType_t delay200 = pdMS_TO_TICKS(200);
  for(;;){
    if(ws.count() > 0){
      String msg = "{";
      msg += "\"real\":" + String(realSpeed,1) + ",";
      msg += "\"fake\":" + String(fakeSpeed,1) + ",";
      msg += "\"histR\":[";
      for(int i=0;i<HISTORY_LEN;i++){
        int idx=(histIdx+i)%HISTORY_LEN;
        msg += String(historyReal[idx],1);
        if(i<HISTORY_LEN-1) msg += ",";
      }
      msg += "],\"histF\":[";
      for(int i=0;i<HISTORY_LEN;i++){
        int idx=(histIdx+i)%HISTORY_LEN;
        msg += String(historyFake[idx],1);
        if(i<HISTORY_LEN-1) msg += ",";
      }
      msg += "],";
      msg += "\"rssi\":" + String(WiFi.RSSI()) + ",";
      msg += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
      msg += "\"uptime\":" + String(millis()/1000);
      msg += "}";
      ws.textAll(msg);
    }
    vTaskDelay(delay200);
  }
}

/* ---- CAN message forward task ---- */
void canTask(void *param){
  String line;
  for(;;){
    while(canSerial.available()){
      char c = canSerial.read();
      if(c=='\n'){
        ws.textAll("{\"can\":\"" + line + "\"}");
        line = "";
      }else if(c!='\r'){
        line += c;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

/* ---- HTTP API ---- */
void setupServer(){
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *req){
    String json = "{";
    json += "\"wheel\":" + String(cfg.wheelCirc,2) + ",";
    json += "\"magnet\":" + String(cfg.magnetCount) + ",";
    json += "\"scale\":" + String(cfg.scale,2) + ",";
    json += "\"offset\":" + String(cfg.offset,2) + ",";
    json += "\"pwmf\":" + String(cfg.pwmFreq) + ",";
    json += "\"pwmd\":" + String(cfg.pwmDuty) + ",";
    json += "\"ssid\":\"" + String(cfg.wifiSsid) + "\",";
    json += "\"pass\":\"" + String(cfg.wifiPass) + "\"";
    json += "}";
    req->send(200, "application/json", json);
  });

  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *req){
    if(req->hasParam("wheel", true)) cfg.wheelCirc = req->getParam("wheel", true)->value().toFloat();
    if(req->hasParam("magnet", true)) cfg.magnetCount = req->getParam("magnet", true)->value().toInt();
    if(req->hasParam("scale", true)) cfg.scale = req->getParam("scale", true)->value().toFloat();
    if(req->hasParam("offset", true)) cfg.offset = req->getParam("offset", true)->value().toFloat();
    if(req->hasParam("pwmf", true)) cfg.pwmFreq = req->getParam("pwmf", true)->value().toInt();
    if(req->hasParam("pwmd", true)) cfg.pwmDuty = req->getParam("pwmd", true)->value().toInt();
    if(req->hasParam("ssid", true)) strlcpy(cfg.wifiSsid, req->getParam("ssid", true)->value().c_str(), sizeof(cfg.wifiSsid));
    if(req->hasParam("pass", true)) strlcpy(cfg.wifiPass, req->getParam("pass", true)->value().c_str(), sizeof(cfg.wifiPass));
    saveConfig();
    ledcSetup(0, cfg.pwmFreq, 8);
    ledcAttachPin(PWM_PIN,0);
    if(strlen(cfg.wifiSsid)>0){
      WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
    }
    req->send(200,"text/plain","OK");
  });

  server.on("/calibrate", HTTP_POST, [](AsyncWebServerRequest *req){
    calibrating = true;
    calibrateStart = millis();
    req->send(200,"text/plain","started");
  });

  server.on("/calibrateDone", HTTP_POST, [](AsyncWebServerRequest *req){
    if(req->hasParam("tacho", true)){
      float reading = req->getParam("tacho", true)->value().toFloat();
      cfg.offset = 15.0f - reading;
      saveConfig();
    }
    calibrating = false;
    req->send(200,"text/plain","ok");
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(SPIFFS, "/dashboard.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/");
  server.addHandler(&ws);
  ws.onEvent(onWsEvent);
  server.begin();
}

/* ---- Setup & Loop ---- */
void setup(){
  Serial.begin(UART_BAUD);
  canSerial.begin(UART_BAUD);
  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);

  prefs.begin("config", false);
  loadConfig();

  ledcSetup(0, cfg.pwmFreq, 8);
  ledcAttachPin(PWM_PIN, 0);
  ledcWrite(0, 0);

  WiFi.mode(WIFI_STA);
  if(strlen(cfg.wifiSsid)>0){
    WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
    unsigned long start = millis();
    while(WiFi.status() != WL_CONNECTED && millis()-start < 20000){
      delay(500);
    }
  }

  if(!SPIFFS.begin(true)){
    logEvent("SPIFFS mount failed");
  }
  ArduinoOTA.begin();

  attachInterrupt(digitalPinToInterrupt(REED_PIN), reedISR, FALLING);

  setupServer();
  xTaskCreatePinnedToCore(calcTask, "calc", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(wsTask, "ws", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(canTask, "can", 4096, NULL, 1, NULL, 1);
}

void loop(){
  ArduinoOTA.handle();
  ws.cleanupClients();
  if(calibrating && millis() - calibrateStart > 30000){
    calibrating = false;
  }
  delay(10);
}

