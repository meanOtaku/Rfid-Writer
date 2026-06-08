#pragma once

#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>

class RFIDCard {
public:
    RFIDCard(uint8_t ssPin, uint8_t rstPin);

    void begin();

    bool cardPresent();

    String getUID();

    bool authenticate(uint8_t block, MFRC522::MIFARE_Key *customKey = nullptr, bool logFailure = true);

    bool authenticateKeyB(uint8_t block, MFRC522::MIFARE_Key *customKey = nullptr, bool logFailure = true);

    bool readBlock(uint8_t block, uint8_t *buffer);

    bool writeBlock(uint8_t block, const uint8_t *buffer);

    bool readBlockRaw(uint8_t block, uint8_t *buffer);

    bool writeBlockRaw(uint8_t block, const uint8_t *buffer);

    bool writePageRaw(uint8_t page, const uint8_t *buffer);

    bool selectCurrent();

    void stopCrypto();

    void halt();

    MFRC522::PICC_Type getType();

    String getTypeName();

private:
    MFRC522 rfid;

    MFRC522::MIFARE_Key key;
};
