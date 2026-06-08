#include "rfid_manager.h"
#include <ArduinoJson.h>

RFIDManager::RFIDManager(uint8_t ssPin, uint8_t rstPin, AsyncWebSocket *websocket) : card(ssPin, rstPin) {
    ws = websocket;
}

void RFIDManager::begin() {
    Serial.println("RFID Ready");
    card.begin();
}

void RFIDManager::setMode(const String &m) { mode = m; }

void RFIDManager::setBlock(int block) { blockNumber = block; }

void RFIDManager::setWriteData(const String &data) { writeData = data; }

void RFIDManager::sendUID(const String &uid) {
    JsonDocument doc;

    doc["type"] = "rfid";
    doc["uid"] = uid;

    String msg;

    serializeJson(doc, msg);

    ws->textAll(msg);
}

void RFIDManager::update() {
    if (!card.cardPresent()) {
        return;
    }

    String uid = card.getUID();

    uid.toUpperCase();

    if (uid == lastUID) {
        card.halt();
        return;
    }

    lastUID = uid;

    sendUID(uid);

    if (mode == "read") {
        String data = readBlock();

        JsonDocument doc;

        doc["type"] = "rfid_read";
        doc["uid"] = uid;
        doc["data"] = data;

        String msg;

        serializeJson(doc, msg);

        ws->textAll(msg);
    } else {
        bool ok = writeBlock();

        JsonDocument doc;

        doc["type"] = "rfid_write";
        doc["uid"] = uid;
        doc["success"] = ok;

        String msg;

        serializeJson(doc, msg);

        ws->textAll(msg);
    }

    card.halt();

    delay(300);

    lastUID = "";
}

String RFIDManager::readBlock() {
    uint8_t buffer[18];

    if (!card.readBlock(blockNumber, buffer)) {
        return "";
    }

    String text = "";

    for (int i = 0; i < 16; i++) {
        if (buffer[i] >= 32 && buffer[i] <= 126) {
            text += (char)buffer[i];
        }
    }

    return text;
}

bool RFIDManager::writeBlock() {
    if ((blockNumber + 1) % 4 == 0) {
        Serial.println("Refusing to write sector trailer");

        return false;
    }

    uint8_t buffer[16];

    memset(buffer, 0, sizeof(buffer));

    size_t len = writeData.length();

    if (len > 16) {
        len = 16;
    }

    memcpy(buffer, writeData.c_str(), len);

    return card.writeBlock(blockNumber, buffer);
}