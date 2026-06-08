#include "rfid_card.h"

RFIDCard::RFIDCard(uint8_t ssPin, uint8_t rstPin) : rfid(ssPin, rstPin) {}

void RFIDCard::begin() {
    SPI.begin(18, // SCK
              19, // MISO
              23  // MOSI
    );

    rfid.PCD_Init();

    Serial.print("MFRC522 Version: 0x");
    Serial.println(rfid.PCD_ReadRegister(MFRC522::VersionReg), HEX);

    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

    Serial.println("RFID Ready");
}

bool RFIDCard::cardPresent() {
    if (!rfid.PICC_IsNewCardPresent()) {
        return false;
    }

    if (!rfid.PICC_ReadCardSerial()) {
        return false;
    }

    return true;
}

String RFIDCard::getUID() {
    String uid = "";

    for (byte i = 0; i < rfid.uid.size; i++) {
        if (i) {
            uid += ":";
        }

        if (rfid.uid.uidByte[i] < 0x10) {
            uid += "0";
        }

        uid += String(rfid.uid.uidByte[i], HEX);
    }

    uid.toUpperCase();

    return uid;
}

bool RFIDCard::authenticate(uint8_t block, MFRC522::MIFARE_Key *customKey, bool logFailure) {
    MFRC522::StatusCode status;
    MFRC522::MIFARE_Key *authKey = customKey ? customKey : &key;

    status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, authKey, &(rfid.uid));

    if (status != MFRC522::STATUS_OK) {
        if (!logFailure) {
            return false;
        }

        Serial.print("Auth failed block ");

        Serial.print(block);

        Serial.print(": ");

        Serial.println(rfid.GetStatusCodeName(status));

        return false;
    }

    return true;
}

bool RFIDCard::authenticateKeyB(uint8_t block, MFRC522::MIFARE_Key *customKey, bool logFailure) {
    MFRC522::StatusCode status;
    MFRC522::MIFARE_Key *authKey = customKey ? customKey : &key;

    status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, block, authKey, &(rfid.uid));

    if (status != MFRC522::STATUS_OK) {
        if (!logFailure) {
            return false;
        }

        Serial.print("Auth B failed block ");

        Serial.print(block);

        Serial.print(": ");

        Serial.println(rfid.GetStatusCodeName(status));

        return false;
    }

    return true;
}

bool RFIDCard::readBlockRaw(uint8_t block, uint8_t *buffer) {
    byte size = 18;

    MFRC522::StatusCode status;

    status = rfid.MIFARE_Read(block, buffer, &size);

    if (status != MFRC522::STATUS_OK) {
        Serial.print("Read failed block ");

        Serial.print(block);

        Serial.print(": ");

        Serial.println(rfid.GetStatusCodeName(status));

        return false;
    }

    return true;
}

bool RFIDCard::writeBlockRaw(uint8_t block, const uint8_t *buffer) {
    MFRC522::StatusCode status;

    status = rfid.MIFARE_Write(block, (uint8_t *)buffer, 16);

    if (status != MFRC522::STATUS_OK) {
        Serial.print("Write failed block ");

        Serial.print(block);

        Serial.print(": ");

        Serial.println(rfid.GetStatusCodeName(status));

        return false;
    }

    Serial.print("Write success block ");

    Serial.println(block);

    return true;
}

bool RFIDCard::writePageRaw(uint8_t page, const uint8_t *buffer) {
    MFRC522::StatusCode status;

    status = rfid.MIFARE_Ultralight_Write(page, (uint8_t *)buffer, 4);

    if (status != MFRC522::STATUS_OK) {
        Serial.print("Write failed page ");

        Serial.print(page);

        Serial.print(": ");

        Serial.println(rfid.GetStatusCodeName(status));

        return false;
    }

    Serial.print("Write success page ");

    Serial.println(page);

    return true;
}

bool RFIDCard::selectCurrent() {
    return rfid.PICC_Select(&(rfid.uid)) == MFRC522::STATUS_OK;
}

bool RFIDCard::readBlock(uint8_t block, uint8_t *buffer) {
    if (!authenticate(block)) {
        return false;
    }

    bool success = readBlockRaw(block, buffer);

    stopCrypto();

    return success;
}

bool RFIDCard::writeBlock(uint8_t block, const uint8_t *buffer) {
    if ((block + 1) % 4 == 0) {
        Serial.println("Refusing to write sector trailer");

        return false;
    }

    if (!authenticate(block)) {
        return false;
    }

    bool success = writeBlockRaw(block, buffer);

    stopCrypto();

    return success;
}

void RFIDCard::stopCrypto() {
    rfid.PCD_StopCrypto1();
}

void RFIDCard::halt() {
    rfid.PICC_HaltA();

    stopCrypto();
}

MFRC522::PICC_Type RFIDCard::getType() {
    return rfid.PICC_GetType(rfid.uid.sak);
}

String RFIDCard::getTypeName() {
    return String(MFRC522::PICC_GetTypeName(getType()));
}
