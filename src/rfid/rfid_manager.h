#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <MFRC522.h>
#include <SPI.h>

#include "rfid/card/rfid_card.h"

class RFIDManager {
public:
    RFIDManager(uint8_t ssPin, uint8_t rstPin, AsyncWebSocket *websocket);

    void begin();

    void update();

    void setMode(const String &mode);

    void setFormat(const String &format);

    void setBlock(int block);

    void setWriteData(const String &data);

private:
    RFIDCard card;

    AsyncWebSocket *ws;

    String lastUID = "";

    String mode = "read";

    String formatMode = "raw";

    int blockNumber = 4;

    String writeData;

    void sendUID(const String &uid);
};