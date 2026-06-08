#include "rfid_manager.h"
#include <ArduinoJson.h>
#include "rfid/mifare/mifare_classic.h"

RFIDManager::RFIDManager(uint8_t ssPin, uint8_t rstPin, AsyncWebSocket *websocket) : card(ssPin, rstPin) {
    ws = websocket;
}

void RFIDManager::begin() {
    Serial.println("RFID Ready");
    card.begin();
}

void RFIDManager::setMode(const String &m) { mode = m; }

void RFIDManager::setFormat(const String &f) { formatMode = f; }

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

    String errorMsg = "";

    if (mode == "read") {
        String data = "";
        bool ok = false;

        if (formatMode == "ndef") {
            ok = MifareClassic::readNDEF(card, data, errorMsg);
        } else {
            ok = MifareClassic::readRawBlock(card, blockNumber, data, errorMsg);
        }

        JsonDocument doc;

        doc["type"] = "rfid_read";
        doc["uid"] = uid;
        doc["data"] = data;
        doc["success"] = ok;
        if (!ok) {
            doc["error"] = errorMsg;
        }

        String msg;

        serializeJson(doc, msg);

        ws->textAll(msg);
    } else {
        bool ok = false;

        if (formatMode == "ndef") {
            ok = MifareClassic::writeNDEF(card, writeData, errorMsg);
        } else {
            ok = MifareClassic::writeRawBlock(card, blockNumber, writeData, errorMsg);
        }

        JsonDocument doc;

        doc["type"] = "rfid_write";
        doc["uid"] = uid;
        doc["success"] = ok;
        if (!ok) {
            doc["error"] = errorMsg;
        }

        String msg;

        serializeJson(doc, msg);

        ws->textAll(msg);
    }

    card.halt();

    delay(300);

    lastUID = "";
}