#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "rfid/rfid_manager.h"
#include "wifi/wifi_manager.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

WiFiManager wifi(&server, &ws);

RFIDManager rfid(5,  // SS
                 22, // RST
                 &server,
                 &ws);

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

    rfid.setupApi();

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
