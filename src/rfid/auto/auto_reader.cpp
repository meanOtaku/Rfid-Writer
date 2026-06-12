#include "auto_reader.h"

#include "rfid/mifare/mifare_classic.h"
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

uint16_t blockCountForType(MFRC522::PICC_Type type) {
    if (type == MFRC522::PICC_TYPE_MIFARE_MINI) {
        return 20;
    }

    if (type == MFRC522::PICC_TYPE_MIFARE_1K) {
        return 64;
    }

    if (type == MFRC522::PICC_TYPE_MIFARE_4K) {
        return 256;
    }

    return 0;
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

uint16_t firstBlockForSector(uint8_t sector) {
    if (sector < 32) {
        return sector * 4;
    }

    return 128 + ((sector - 32) * 16);
}

uint16_t trailerBlockForSector(uint8_t sector) {
    if (sector < 32) {
        return firstBlockForSector(sector) + 3;
    }

    return firstBlockForSector(sector) + 15;
}

String bytesToText(const uint8_t *data, size_t len) {
    String out = "";

    for (size_t i = 0; i < len; i++) {
        if (data[i] >= 32 && data[i] <= 126) {
            out += (char)data[i];
        }
    }

    return out;
}

bool resetCardSession(RFIDCard &card) {
    card.stopCrypto();
    delay(5);

    if (card.wakeAndSelect()) {
        return true;
    }

    card.resetReader();
    delay(10);
    return card.wakeAndSelect();
}

bool authenticateBlockWithKnownKeys(RFIDCard &card, uint8_t block) {
    MFRC522::MIFARE_Key keys[] = {
        makeKey(DEFAULT_KEY_BYTES),
        makeKey(NDEF_KEY_BYTES),
        makeKey(MAD_KEY_BYTES),
    };

    for (MFRC522::MIFARE_Key &key : keys) {
        if (!resetCardSession(card)) {
            continue;
        }

        if (card.authenticate(block, &key, false)) {
            return true;
        }

        if (!resetCardSession(card)) {
            continue;
        }

        if (card.authenticateKeyB(block, &key, false)) {
            return true;
        }
    }

    return false;
}

bool blockHasData(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] != 0x00) {
            return true;
        }
    }

    return false;
}

bool blockLooksLikeNdefTlv(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] == 0x00) {
            continue;
        }

        return data[i] == 0x03;
    }

    return false;
}

bool classicLooksLikeNdef(RFIDCard &card) {
    uint8_t buffer[18];

    if (!authenticateBlockWithKnownKeys(card, 4)) {
        card.stopCrypto();
        return false;
    }

    bool looksLikeNdef = card.readBlockRaw(4, buffer) && blockLooksLikeNdefTlv(buffer, 16);

    card.stopCrypto();
    return looksLikeNdef;
}

bool readClassicBlocksBySector(RFIDCard &card, String &outData, String &outError) {
    uint8_t sectorCount = sectorCountForType(card.getType());
    bool readAnyBlock = false;
    bool foundData = false;
    bool resetFailed = false;

    outData += "\n\nMIFARE Classic data blocks";

    if (!resetCardSession(card)) {
        outError = "Could not select card before Classic scan";
        return false;
    }

    for (uint8_t sector = 0; sector < sectorCount; sector++) {
        uint16_t firstBlock = firstBlockForSector(sector);
        uint16_t trailerBlock = trailerBlockForSector(sector);

        if (firstBlock >= blockCountForType(card.getType())) {
            continue;
        }

        if (!authenticateBlockWithKnownKeys(card, (uint8_t)firstBlock)) {
            card.stopCrypto();

            if (!resetCardSession(card)) {
                outData += "\nSector ";
                outData += String(sector);
                outData += ": card reset failed";
                resetFailed = true;
            }

            continue;
        }

        for (uint16_t block = firstBlock; block < trailerBlock; block++) {
            if (block == 0) {
                continue;
            }

            uint8_t buffer[18];

            if (card.readBlockRaw((uint8_t)block, buffer)) {
                readAnyBlock = true;

                if (!blockHasData(buffer, 16)) {
                    continue;
                }

                String blockData = bytesToText(buffer, 16);

                outData += "\nBlock ";
                outData += String(block);
                outData += ": ";
                outData += blockData.length() > 0 ? blockData : "(empty)";
                foundData = true;
            } else {
                outData += "\nBlock ";
                outData += String(block);
                outData += ": ";
                outData += "unreadable";
                break;
            }
        }

        card.stopCrypto();
    }

    if (!readAnyBlock) {
        outData += "\n\nNo readable MIFARE Classic data blocks found with default key.";
        outError = resetFailed ? "Could not reselect card after NDEF check"
                               : "No readable MIFARE Classic blocks found with default key";
    }

    if (readAnyBlock && !foundData) {
        outData += "\n\nReadable blocks found, but no non-empty data blocks were found.";
    }

    return readAnyBlock;
}
} // namespace

bool AutoReader::read(RFIDCard &card, AutoReadResult &result, String &outError) {
    MFRC522::PICC_Type type = card.getType();

    if (type == MFRC522::PICC_TYPE_MIFARE_UL) {
        result.format = "ndef";
        return NDEFManager::read(card, result.data, outError);
    }

    if (isMifareClassic(type)) {
        String ndefData = "";
        String ndefError = "No NDEF marker found in block 4";
        bool shouldTryNdef = classicLooksLikeNdef(card);

        resetCardSession(card);

        bool ndefOk = shouldTryNdef && NDEFManager::read(card, ndefData, ndefError);

        card.stopCrypto();

        if (ndefOk) {
            result.format = "ndef";
            result.data = "NDEF data\n\n";
            result.data += ndefData.length() > 0 ? ndefData : "(empty)";
            return true;
        }

        resetCardSession(card);

        result.format = "mifare";
        result.data = "NDEF data\n\nNot found";

        if (ndefError.length() > 0) {
            result.data += ": ";
            result.data += ndefError;
        }

        String classicError = "";
        bool blockOk = readClassicBlocksBySector(card, result.data, classicError);

        if (blockOk) {
            return true;
        }

        outError = classicError.length() > 0 ? classicError : "No readable MIFARE Classic block data found";
        return false;
    }

    outError = "Unsupported card type for automatic read";
    return false;
}
