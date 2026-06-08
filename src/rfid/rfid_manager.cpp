#include "rfid_manager.h"
#include <ArduinoJson.h>
#include "rfid/mifare/mifare_classic.h"

RFIDManager::RFIDManager(uint8_t ssPin, uint8_t rstPin, AsyncWebServer *srv, AsyncWebSocket *websocket)
    : card(ssPin, rstPin) {
    server = srv;
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

void RFIDManager::triggerFormat() { pendingFormat = true; }

void RFIDManager::setupApi() {
    server->on(
        "/api/rfid/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;

            auto err = deserializeJson(doc, data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");

                return;
            }

            setMode(doc["mode"] | "read");
            setFormat(doc["format"] | "raw");
            setBlock(doc["block"] | 4);
            setWriteData(doc["data"] | "");

            request->send(200, "application/json", "{\"success\":true}");
        });

    server->on("/api/rfid/format", HTTP_POST, [this](AsyncWebServerRequest *request) {
        triggerFormat();

        request->send(200, "application/json", "{\"success\":true}");
    });
}

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

    if (pendingFormat) {
        bool ok = MifareClassic::formatNDEF(card, errorMsg);
        pendingFormat = false;

        JsonDocument doc;

        doc["type"] = "rfid_format";
        doc["uid"] = uid;
        doc["success"] = ok;
        if (!ok) {
            doc["error"] = errorMsg;
        }

        String msg;

        serializeJson(doc, msg);

        ws->textAll(msg);
    } else if (mode == "read") {
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
