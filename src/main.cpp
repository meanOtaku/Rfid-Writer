#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "led/status_led.h"
#include "rfid/rfid_manager.h"
#include "wifi/wifi_manager.h"

constexpr uint8_t YELLOW_LED_PIN = 25;
constexpr uint8_t RED_LED_PIN = 26;
constexpr uint8_t GREEN_LED_PIN = 27;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

WiFiManager wifi(&server, &ws);
StatusLED statusLED(YELLOW_LED_PIN, RED_LED_PIN, GREEN_LED_PIN);

RFIDManager rfid(5,  // SS
                 22, // RST
                 &server,
                 &ws,
                 &statusLED);

void setup() {
    Serial.begin(115200);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");

        while (true) {
            delay(1000);
        }
    }

    wifi.begin();

    statusLED.begin();

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
