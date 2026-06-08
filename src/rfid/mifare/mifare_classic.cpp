#include "mifare_classic.h"

bool MifareClassic::readRawBlock(RFIDCard &card, uint8_t blockNum, String &outData, String &outError) {
    uint8_t buffer[18];

    if (!card.readBlock(blockNum, buffer)) {
        outError = "Failed to read block " + String(blockNum);
        return false;
    }

    outData = "";
    for (int i = 0; i < 16; i++) {
        if (buffer[i] >= 32 && buffer[i] <= 126) {
            outData += (char)buffer[i];
        }
    }

    return true;
}

bool MifareClassic::writeRawBlock(RFIDCard &card, uint8_t blockNum, const String &data, String &outError) {
    if ((blockNum + 1) % 4 == 0) {
        outError = "Refusing to write sector trailer block " + String(blockNum);
        return false;
    }

    uint8_t buffer[16];
    memset(buffer, 0, sizeof(buffer));

    size_t len = data.length();
    if (len > 16) {
        len = 16;
    }
    memcpy(buffer, data.c_str(), len);

    if (!card.writeBlock(blockNum, buffer)) {
        outError = "Failed to write block " + String(blockNum);
        return false;
    }

    return true;
}
