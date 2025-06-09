#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

/* Configuration */
#define REED_PIN 4          // GPIO for reed switch
#define PWM_PIN  21         // PWM output
#define STATUS_LED 2
#define UART_BAUD 115200

/* Globals */
volatile unsigned long lastPulse = 0;
volatile unsigned long pulseInterval = 0;

Preferences prefs;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

float wheelCirc = 2.17f;    // meters
uint8_t magnetCount = 1;
float scale = 1.0f;
float offset = -10.0f;
uint32_t pwmFreq = 50;      // Hz
uint8_t pwmChannel = 0;
uint8_t pwmResolution = 8;
uint32_t pwmDuty = 128;     // pulse width

/* Real-time variables */
float realSpeed = 0.0f;
float fakeSpeed = 0.0f;
unsigned long lastCalc = 0;

/* Logging */
void logEvent(const String &msg){
  Serial.println(msg);
}

/* Interrupt handler */
void IRAM_ATTR reedISR(){
  unsigned long now = micros();
  pulseInterval = now - lastPulse;
  lastPulse = now;
}

/* Speed calculation task */
void calcTask(void *param){
  for(;;){
    if(pulseInterval > 0){
      float revPerSec = 1000000.0f / pulseInterval;
      revPerSec /= magnetCount;
      realSpeed = revPerSec * wheelCirc * 3.6f; // m/s to km/h
    }
    fakeSpeed = realSpeed * scale + offset;
    fakeSpeed = max(0.0f, fakeSpeed);
    ledcWrite(pwmChannel, (int)pwmDuty);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/* WebSocket events */
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    logEvent("WS client connected");
  } else if(type == WS_EVT_DISCONNECT){
    logEvent("WS client disconnected");
  }
}

/* Serve dashboard */
void setupServer(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(SPIFFS, "/dashboard.html", "text/html");
  });
  server.serveStatic("/", SPIFFS, "/");
  server.addHandler(&ws);
  ws.onEvent(onWsEvent);
  server.begin();
}

void setup(){
  Serial.begin(UART_BAUD);
  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  prefs.begin("config", false);
  wheelCirc = prefs.getFloat("wheel", wheelCirc);
  magnetCount = prefs.getUChar("magnet", magnetCount);
  scale = prefs.getFloat("scale", scale);
  offset = prefs.getFloat("offset", offset);
  pwmFreq = prefs.getUInt("pwmf", pwmFreq);
  pwmDuty = prefs.getUInt("pwmd", pwmDuty);

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(PWM_PIN, pwmChannel);
  ledcWrite(pwmChannel, 0);

  WiFi.mode(WIFI_STA);
  WiFi.begin("your_ssid", "your_password");
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
  }

  ArduinoOTA.begin();
  if(!SPIFFS.begin(true)){
    logEvent("SPIFFS mount failed");
  }
  setupServer();
  attachInterrupt(digitalPinToInterrupt(REED_PIN), reedISR, FALLING);
  xTaskCreatePinnedToCore(calcTask, "calc", 4096, NULL, 1, NULL, 1);
}

void loop(){
  ArduinoOTA.handle();
  ws.cleanupClients();
  vTaskDelay(pdMS_TO_TICKS(10));
}


