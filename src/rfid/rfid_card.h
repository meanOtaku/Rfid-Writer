#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

class RFIDCard
{
public:
    RFIDCard(
        uint8_t ssPin,
        uint8_t rstPin);

    void begin();

    bool cardPresent();

    String getUID();

    bool authenticate(
        uint8_t block);

    bool readBlock(
        uint8_t block,
        uint8_t *buffer);

    bool writeBlock(
        uint8_t block,
        const uint8_t *buffer);

    void halt();

private:
    MFRC522 rfid;

    MFRC522::MIFARE_Key key;
};