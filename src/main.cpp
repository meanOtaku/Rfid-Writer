#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "rfid/rfid_manager.h"
#include "wifi/wifi_manager.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

WiFiManager wifi(&server, &ws);

RFIDManager rfid(5,  // SS
                 22, // RST
                 &ws);

void setupRFIDApi() {
    server.on(
        "/api/rfid/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;

            auto err = deserializeJson(doc, data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");

                return;
            }

            rfid.setMode(doc["mode"] | "read");

            rfid.setFormat(doc["format"] | "raw");

            rfid.setBlock(doc["block"] | 4);

            rfid.setWriteData(doc["data"] | "");

            request->send(200, "application/json", "{\"success\":true}");
        });

    server.on(
        "/api/rfid/format", HTTP_POST, [](AsyncWebServerRequest *request) {
            rfid.triggerFormat();
            request->send(200, "application/json", "{\"success\":true}");
        });
}

void setup() {
    Serial.begin(115200);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");

        while (true) {
            delay(1000);
        }
    }

    wifi.begin();

    rfid.begin();

    wifi.setupApi();

    setupRFIDApi();

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.begin();

    Serial.println();
    Serial.println("System Ready");
}

void loop() {
    ws.cleanupClients();

    wifi.update();

    rfid.update();
}