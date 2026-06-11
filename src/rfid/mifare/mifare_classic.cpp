#include "mifare_classic.h"
#include "rfid/ndef/ndef_manager.h"

namespace {
constexpr uint8_t DEFAULT_KEY_BYTES[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr uint8_t NDEF_KEY_BYTES[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
constexpr uint8_t MAD_KEY_BYTES[6] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};

MFRC522::MIFARE_Key makeKey(const uint8_t keyBytes[6]) {
    MFRC522::MIFARE_Key key;

    for (uint8_t i = 0; i < 6; i++) {
        key.keyByte[i] = keyBytes[i];
    }

    return key;
}

bool isMifareClassic(MFRC522::PICC_Type type) {
    return type == MFRC522::PICC_TYPE_MIFARE_MINI || type == MFRC522::PICC_TYPE_MIFARE_1K ||
           type == MFRC522::PICC_TYPE_MIFARE_4K;
}

uint8_t sectorCountForType(MFRC522::PICC_Type type) {
    if (type == MFRC522::PICC_TYPE_MIFARE_MINI) {
        return 5;
    }

    if (type == MFRC522::PICC_TYPE_MIFARE_1K) {
        return 16;
    }

    if (type == MFRC522::PICC_TYPE_MIFARE_4K) {
        return 40;
    }

    return 0;
}

uint8_t firstBlockForSector(uint8_t sector) {
    if (sector < 32) {
        return sector * 4;
    }

    return 128 + ((sector - 32) * 16);
}

uint8_t trailerBlockForSector(uint8_t sector) {
    if (sector < 32) {
        return firstBlockForSector(sector) + 3;
    }

    return firstBlockForSector(sector) + 15;
}

bool resetMifareSession(RFIDCard &card) {
    card.stopCrypto();
    delay(5);

    if (card.wakeAndSelect()) {
        return true;
    }

    card.resetReader();
    delay(10);
    return card.wakeAndSelect();
}

bool authenticateKnownKey(RFIDCard &card, uint8_t block) {
    MFRC522::MIFARE_Key keys[] = {
        makeKey(DEFAULT_KEY_BYTES),
        makeKey(NDEF_KEY_BYTES),
        makeKey(MAD_KEY_BYTES),
    };

    for (MFRC522::MIFARE_Key &key : keys) {
        if (!resetMifareSession(card)) {
            continue;
        }

        if (card.authenticate(block, &key, false)) {
            return true;
        }

        if (!resetMifareSession(card)) {
            continue;
        }

        if (card.authenticateKeyB(block, &key, false)) {
            return true;
        }
    }

    return false;
}
} // namespace

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

bool MifareClassic::convertNdefToClassic(RFIDCard &card, String &outData, String &outError) {
    if (!isMifareClassic(card.getType())) {
        outError = "Card is not MIFARE Classic";
        return false;
    }

    String ndefData = "";

    if (!NDEFManager::read(card, ndefData, outError)) {
        return false;
    }

    if (ndefData.length() == 0) {
        outError = "NDEF message is empty";
        return false;
    }

    size_t offset = 0;
    bool wroteAnyBlock = false;
    uint8_t sectorCount = sectorCountForType(card.getType());

    for (uint8_t sector = 1; sector < sectorCount; sector++) {
        uint8_t firstBlock = firstBlockForSector(sector);
        uint8_t trailerBlock = trailerBlockForSector(sector);

        if (!authenticateKnownKey(card, firstBlock)) {
            continue;
        }

        for (uint8_t block = firstBlock; block < trailerBlock; block++) {
            uint8_t buffer[16];
            memset(buffer, 0, sizeof(buffer));

            for (uint8_t i = 0; i < 16; i++) {
                if (offset < ndefData.length()) {
                    buffer[i] = (uint8_t)ndefData[offset++];
                }
            }

            if (!card.writeBlockRaw(block, buffer)) {
                card.stopCrypto();
                if (offset < ndefData.length()) {
                    outError = "Failed to write Classic block " + String(block);
                    return false;
                }

                break;
            }

            wroteAnyBlock = true;
        }

        card.stopCrypto();
    }

    if (!wroteAnyBlock) {
        outError = "No writable Classic data blocks found with known keys";
        return false;
    }

    outError = "NDEF message is larger than writable Classic space";

    if (offset >= ndefData.length()) {
        outData = ndefData;
        outError = "";
        return true;
    }

    return false;
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
