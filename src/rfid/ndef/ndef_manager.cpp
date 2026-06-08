#include "ndef_manager.h"

#include <string.h>

namespace {
constexpr size_t MAX_NDEF_STORAGE = 1024;

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

bool authenticateNdefBlock(RFIDCard &card, uint8_t block) {
    MFRC522::MIFARE_Key keys[] = {
        makeKey(NDEF_KEY_BYTES),
        makeKey(DEFAULT_KEY_BYTES),
        makeKey(MAD_KEY_BYTES),
    };

    for (MFRC522::MIFARE_Key &key : keys) {
        card.stopCrypto();

        if (card.authenticate(block, &key, false)) {
            return true;
        }

        card.stopCrypto();
        card.selectCurrent();

        if (card.authenticateKeyB(block, &key, false)) {
            return true;
        }

        card.stopCrypto();
        card.selectCurrent();
    }

    return false;
}

size_t readMifareClassicStorage(RFIDCard &card, uint8_t *storage, size_t storageSize) {
    size_t offset = 0;
    uint8_t sectorCount = sectorCountForType(card.getType());

    for (uint8_t sector = 1; sector < sectorCount; sector++) {
        uint8_t firstBlock = firstBlockForSector(sector);
        uint8_t trailerBlock = trailerBlockForSector(sector);

        if (!authenticateNdefBlock(card, firstBlock)) {
            card.stopCrypto();
            card.selectCurrent();
            continue;
        }

        for (uint8_t block = firstBlock; block < trailerBlock; block++) {
            if (offset + 16 > storageSize) {
                card.stopCrypto();
                return offset;
            }

            if (card.readBlockRaw(block, storage + offset)) {
                offset += 16;
            } else {
                break;
            }
        }

        card.stopCrypto();
        card.selectCurrent();
    }

    return offset;
}

size_t readType2Storage(RFIDCard &card, uint8_t *storage, size_t storageSize) {
    size_t offset = 0;
    uint8_t ccBuffer[18];
    uint16_t dataBytes = 240;

    if (card.readBlockRaw(3, ccBuffer) && ccBuffer[0] == 0xE1) {
        dataBytes = (uint16_t)ccBuffer[2] * 8;
    }

    uint16_t firstPage = 4;
    uint16_t pageCount = (dataBytes + 3) / 4;
    uint16_t lastPage = firstPage + pageCount;

    for (uint16_t page = firstPage; page < lastPage && offset + 16 <= storageSize; page += 4) {
        if (!card.readBlockRaw(page, storage + offset)) {
            break;
        }

        offset += 16;
    }

    return offset;
}

bool readTlvLength(const uint8_t *storage, size_t storageLen, size_t *index, size_t *length) {
    if (*index >= storageLen) {
        return false;
    }

    *length = storage[(*index)++];

    if (*length == 0xFF) {
        if (*index + 2 > storageLen) {
            return false;
        }

        *length = ((size_t)storage[*index] << 8) | storage[*index + 1];
        *index += 2;
    }

    return true;
}

bool findNdefMessage(const uint8_t *storage, size_t storageLen, const uint8_t **message, size_t *messageLen,
                     String &outError) {
    for (size_t start = 0; start < storageLen; start++) {
        size_t index = start;
        uint8_t type = storage[index++];

        if (type == 0x00) {
            continue;
        }

        if (type == 0xFE) {
            continue;
        }

        size_t len = 0;
        if (!readTlvLength(storage, storageLen, &index, &len)) {
            continue;
        }

        if (index + len > storageLen) {
            continue;
        }

        if (type == 0x03) {
            *message = storage + index;
            *messageLen = len;
            return true;
        }
    }

    outError = "No NDEF message found";
    return false;
}

String bytesToString(const uint8_t *data, size_t len) {
    String out = "";

    for (size_t i = 0; i < len; i++) {
        if (data[i] == 0x00) {
            continue;
        }

        out += (char)data[i];
    }

    return out;
}

String decodeTextRecord(const uint8_t *payload, size_t payloadLength) {
    if (payloadLength < 1) {
        return "";
    }

    bool utf16 = payload[0] & 0x80;
    uint8_t languageLength = payload[0] & 0x3F;

    if (payloadLength <= languageLength + 1) {
        return "";
    }

    const uint8_t *text = payload + languageLength + 1;
    size_t textLength = payloadLength - languageLength - 1;

    if (utf16) {
        return bytesToString(text, textLength);
    }

    return bytesToString(text, textLength);
}

String decodeUriRecord(const uint8_t *payload, size_t payloadLength) {
    if (payloadLength < 1) {
        return "";
    }

    static const char *prefixes[] = {
        "",           "http://www.",  "https://www.", "http://",      "https://",     "tel:",
        "mailto:",    "ftp://anonymous:anonymous@",   "ftp://ftp.",   "ftps://",      "sftp://",
        "smb://",     "nfs://",       "ftp://",       "dav://",       "news:",        "telnet://",
        "imap:",      "rtsp://",      "urn:",         "pop:",         "sip:",         "sips:",
        "tftp:",      "btspp://",     "btl2cap://",   "btgoep://",    "tcpobex://",   "irdaobex://",
        "file://",    "urn:epc:id:",  "urn:epc:tag:", "urn:epc:pat:", "urn:epc:raw:", "urn:epc:",
        "urn:nfc:",
    };

    String out = "";
    uint8_t prefixIndex = payload[0];

    if (prefixIndex < sizeof(prefixes) / sizeof(prefixes[0])) {
        out += prefixes[prefixIndex];
    }

    out += bytesToString(payload + 1, payloadLength - 1);
    return out;
}

String bytesToHex(const uint8_t *data, size_t len) {
    String out = "NDEF:";

    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) {
            out += "0";
        }

        out += String(data[i], HEX);
    }

    out.toUpperCase();
    return out;
}

bool decodeNdefMessage(const uint8_t *message, size_t messageLen, String &outData, String &outError);

bool decodeOneRecord(const uint8_t *message, size_t messageLen, size_t *recordIndex, String &recordData,
                     String &outError) {
    size_t index = *recordIndex;

    if (index + 2 > messageLen) {
        outError = "NDEF message is too short";
        return false;
    }

    uint8_t header = message[index++];
    bool messageEnd = header & 0x40;
    bool chunked = header & 0x20;
    bool shortRecord = header & 0x10;
    bool hasId = header & 0x08;
    uint8_t tnf = header & 0x07;
    uint8_t typeLength = message[index++];
    uint32_t payloadLength = 0;

    if (shortRecord) {
        if (index >= messageLen) {
            outError = "NDEF short record is incomplete";
            return false;
        }

        payloadLength = message[index++];
    } else {
        if (index + 4 > messageLen) {
            outError = "NDEF long record is incomplete";
            return false;
        }

        payloadLength = ((uint32_t)message[index] << 24) | ((uint32_t)message[index + 1] << 16) |
                        ((uint32_t)message[index + 2] << 8) | message[index + 3];
        index += 4;
    }

    uint8_t idLength = 0;

    if (hasId) {
        if (index >= messageLen) {
            outError = "NDEF record ID is incomplete";
            return false;
        }

        idLength = message[index++];
    }

    if (index + typeLength + idLength + payloadLength > messageLen) {
        outError = "NDEF record length is invalid";
        return false;
    }

    const uint8_t *type = message + index;
    index += typeLength;

    if (hasId) {
        index += idLength;
    }

    const uint8_t *payload = message + index;
    *recordIndex = index + payloadLength;

    if (chunked) {
        recordData = "";
        return !messageEnd;
    }

    if (tnf == 0x01 && typeLength == 1 && type[0] == 'T') {
        recordData = decodeTextRecord(payload, payloadLength);
    } else if (tnf == 0x01 && typeLength == 1 && type[0] == 'U') {
        recordData = decodeUriRecord(payload, payloadLength);
    } else if (tnf == 0x01 && typeLength == 2 && type[0] == 'S' && type[1] == 'p') {
        String nestedError = "";
        decodeNdefMessage(payload, payloadLength, recordData, nestedError);
    } else if (tnf == 0x02 && typeLength == 10 && memcmp(type, "text/plain", 10) == 0) {
        recordData = bytesToString(payload, payloadLength);
    } else if (tnf == 0x03) {
        recordData = bytesToString(type, typeLength);
    } else {
        recordData = "";
    }

    return true;
}

bool decodeNdefMessage(const uint8_t *message, size_t messageLen, String &outData, String &outError) {
    size_t index = 0;
    String fallback = "";

    while (index < messageLen) {
        String recordData = "";
        size_t previousIndex = index;

        if (!decodeOneRecord(message, messageLen, &index, recordData, outError)) {
            return false;
        }

        if (index <= previousIndex) {
            break;
        }

        if (recordData.length() > 0) {
            if (outData.length() > 0) {
                outData += "\n";
            }

            outData += recordData;
        }

        if (fallback.length() == 0) {
            fallback = bytesToHex(message + previousIndex, index - previousIndex);
        }

        if (message[previousIndex] & 0x40) {
            break;
        }
    }

    if (outData.length() == 0) {
        if (fallback.length() > 0) {
            outData = fallback;
            return true;
        }

        outError = "NDEF record is empty or unsupported";
        return false;
    }

    return true;
}
} // namespace

bool NDEFManager::read(RFIDCard &card, String &outData, String &outError) {
    uint8_t storage[MAX_NDEF_STORAGE];
    memset(storage, 0, sizeof(storage));

    MFRC522::PICC_Type cardType = card.getType();
    size_t storageLen = 0;

    if (cardType == MFRC522::PICC_TYPE_MIFARE_UL) {
        storageLen = readType2Storage(card, storage, sizeof(storage));
    } else if (isMifareClassic(cardType)) {
        storageLen = readMifareClassicStorage(card, storage, sizeof(storage));
    } else {
        outError = "Unsupported card type for NDEF read";
        return false;
    }

    if (storageLen == 0) {
        outError = "No readable NDEF storage found";
        return false;
    }

    const uint8_t *message = nullptr;
    size_t messageLen = 0;

    if (!findNdefMessage(storage, storageLen, &message, &messageLen, outError)) {
        return false;
    }

    return decodeNdefMessage(message, messageLen, outData, outError);
}

bool NDEFManager::write(RFIDCard &card, const String &data, String &outError) {
    // NDEF placeholder implementation.
    // In a complete implementation, this would:
    // 1. Construct an NDEF Text or URI record.
    // 2. Write the NDEF TLV wrapper.
    // 3. Store the message using the detected tag storage type.

    Serial.print("NDEFManager::write stub called with data: ");
    Serial.println(data);

    return true;
}

bool NDEFManager::format(RFIDCard &card, String &outError) {
    // NDEF format placeholder.
    // In a complete implementation, this would initialize the tag storage
    // for NDEF according to its concrete card type.

    Serial.println("NDEFManager::format stub called");
    return true;
}
