#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <MFRC522.h>
#include <SPI.h>

#include "led/status_led.h"
#include "rfid/card/rfid_card.h"

class RFIDManager {
public:
    RFIDManager(uint8_t ssPin, uint8_t rstPin, AsyncWebServer *server, AsyncWebSocket *websocket,
                StatusLED *statusLED = nullptr);

    void begin();

    void update();

    void setupApi();

private:
    RFIDCard card;

    AsyncWebServer *server;
    AsyncWebSocket *ws;
    StatusLED *statusLED;

    String lastUID = "";

    String mode = "read";

    String formatMode = "raw";

    bool pendingFormat = false;

    bool waitingForTagRemoval = false;

    uint8_t tagMissingChecks = 0;

    int blockNumber = 4;

    String writeData;

    void setMode(const String &mode);

    void setFormat(const String &format);

    void setBlock(int block);

    void setWriteData(const String &data);

    void triggerFormat();

    void sendUID(const String &uid);

    void sendRemoved();
};
