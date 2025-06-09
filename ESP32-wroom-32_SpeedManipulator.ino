#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

float realSpeed = 0.0f;
float fakeSpeed = 0.0f;

void calcTask(void *pvParameters) {
    for (;;) {
        // Update speed values (replace with sensor logic)
        realSpeed += 0.1f;  // dummy update
        fakeSpeed = realSpeed * 1.5f;

        char json[64];
        snprintf(json, sizeof(json), "{\"realSpeed\":%.2f,\"fakeSpeed\":%.2f}", realSpeed, fakeSpeed);
        ws.printfAll("%s", json);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.println("WebSocket client connected");
            break;
        case WS_EVT_DISCONNECT:
            Serial.println("WebSocket client disconnected");
            break;
        case WS_EVT_DATA:
            data[len] = 0; // ensure null-terminated
            if (strcmp((char *)data, "getSpeed") == 0) {
                char json[64];
                snprintf(json, sizeof(json), "{\"realSpeed\":%.2f,\"fakeSpeed\":%.2f}", realSpeed, fakeSpeed);
                client->printf("%s", json);
            }
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();

    xTaskCreatePinnedToCore(calcTask, "calcTask", 4096, nullptr, 1, nullptr, 1);
}

void loop() {
    // nothing needed - tasks and async server handle everything
}

