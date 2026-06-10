#include "auto_reader.h"

#include "rfid/mifare/mifare_classic.h"
#include "rfid/ndef/ndef_manager.h"

namespace {
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

bool isTrailerBlock(uint16_t block) {
    for (uint8_t sector = 0; sector < 40; sector++) {
        if (block == trailerBlockForSector(sector)) {
            return true;
        }
    }

    return false;
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

bool readClassicBlocksOneByOne(RFIDCard &card, String &outData, String &outError) {
    uint16_t blockCount = blockCountForType(card.getType());
    bool readAnyBlock = false;
    bool resetFailed = false;

    outData += "\n\nMIFARE Classic blocks";

    for (uint16_t block = 1; block < blockCount; block++) {
        if (isTrailerBlock(block)) {
            continue;
        }

        String blockData = "";
        String blockError = "";

        outData += "\nBlock ";
        outData += String(block);
        outData += ": ";

        if (!resetCardSession(card)) {
            outData += "card reset failed";
            resetFailed = true;
            continue;
        }

        if (MifareClassic::readRawBlock(card, (uint8_t)block, blockData, blockError)) {
            outData += blockData.length() > 0 ? blockData : "(empty)";
            readAnyBlock = true;
        } else {
            outData += "unreadable";
        }

        card.stopCrypto();
    }

    if (!readAnyBlock) {
        outData += "\n\nNo readable MIFARE Classic data blocks found with default key.";
        outError = resetFailed ? "Could not reselect card after NDEF check"
                               : "No readable MIFARE Classic blocks found with default key";
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
        String ndefError = "";
        bool ndefOk = NDEFManager::read(card, ndefData, ndefError);

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
        bool blockOk = readClassicBlocksOneByOne(card, result.data, classicError);

        if (blockOk) {
            return true;
        }

        outError = classicError.length() > 0 ? classicError : "No readable MIFARE Classic block data found";
        return false;
    }

    outError = "Unsupported card type for automatic read";
    return false;
}
