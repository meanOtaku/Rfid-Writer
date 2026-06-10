#include "rfid_manager.h"
#include <ArduinoJson.h>
#include "rfid/auto/auto_reader.h"
#include "rfid/mifare/mifare_classic.h"
#include "rfid/ndef/ndef_manager.h"

RFIDManager::RFIDManager(uint8_t ssPin, uint8_t rstPin, AsyncWebServer *srv, AsyncWebSocket *websocket,
                         StatusLED *led)
    : card(ssPin, rstPin) {
    server = srv;
    ws = websocket;
    statusLED = led;
}

void RFIDManager::begin() {
    Serial.println("RFID Ready");
    card.begin();

    if (statusLED) {
        statusLED->set(LEDStatus::Idle);
    }
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
    doc["cardType"] = card.getTypeName();

    String msg;

    serializeJson(doc, msg);

    ws->textAll(msg);
}

void RFIDManager::sendRemoved() {
    JsonDocument doc;

    doc["type"] = "rfid_removed";

    String msg;

    serializeJson(doc, msg);

    ws->textAll(msg);
}

void RFIDManager::update() {
    if (waitingForTagRemoval) {
        if (!card.cardPresent()) {
            tagMissingChecks++;

            if (tagMissingChecks >= 3) {
                waitingForTagRemoval = false;
                tagMissingChecks = 0;
                lastUID = "";
                sendRemoved();
                if (statusLED) {
                    statusLED->set(LEDStatus::Idle);
                }
            }
        } else {
            tagMissingChecks = 0;
            card.stopCrypto();
        }

        return;
    }

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

    String cardType = card.getTypeName();

    sendUID(uid);

    String errorMsg = "";

    if (pendingFormat) {
        bool ok = NDEFManager::format(card, errorMsg);
        pendingFormat = false;

        if (statusLED) {
            statusLED->set(ok ? LEDStatus::Success : LEDStatus::Error);
        }

        JsonDocument doc;

        doc["type"] = "rfid_format";
        doc["uid"] = uid;
        doc["cardType"] = cardType;
        doc["success"] = ok;
        if (!ok) {
            doc["error"] = errorMsg;
        }

        String msg;

        serializeJson(doc, msg);

        ws->textAll(msg);
    } else if (mode == "read") {
        String data = "";
        String readFormat = formatMode;
        bool ok = false;

        if (formatMode == "auto") {
            AutoReadResult result;

            ok = AutoReader::read(card, result, errorMsg);
            readFormat = result.format;
            data = result.data;

            if (readFormat.length() == 0) {
                readFormat = "auto";
            }
        } else if (formatMode == "ndef") {
            ok = NDEFManager::read(card, data, errorMsg);
        } else {
            ok = MifareClassic::readRawBlock(card, blockNumber, data, errorMsg);
            readFormat = "raw";
        }

        if (statusLED) {
            statusLED->set(ok ? LEDStatus::Success : LEDStatus::Error);
        }

        JsonDocument doc;

        doc["type"] = "rfid_read";
        doc["uid"] = uid;
        doc["cardType"] = cardType;
        doc["format"] = readFormat;
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
            ok = NDEFManager::write(card, writeData, errorMsg);
        } else {
            ok = MifareClassic::writeRawBlock(card, blockNumber, writeData, errorMsg);
        }

        if (statusLED) {
            statusLED->set(ok ? LEDStatus::Success : LEDStatus::Error);
        }

        JsonDocument doc;

        doc["type"] = "rfid_write";
        doc["uid"] = uid;
        doc["cardType"] = cardType;
        doc["data"] = writeData;
        doc["success"] = ok;
        if (!ok) {
            doc["error"] = errorMsg;
        }

        String msg;

        serializeJson(doc, msg);

        ws->textAll(msg);
    }

    card.stopCrypto();

    delay(100);

    tagMissingChecks = 0;
    waitingForTagRemoval = true;
}
