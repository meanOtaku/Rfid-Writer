#include "ndef_manager.h"

#include <string.h>

namespace {
constexpr size_t MAX_NDEF_STORAGE = 1024;
constexpr size_t MAX_NDEF_MESSAGE = 900;
constexpr size_t MAX_NDEF_TLV = 920;

constexpr uint8_t DEFAULT_KEY_BYTES[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr uint8_t NDEF_KEY_BYTES[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
constexpr uint8_t MAD_KEY_BYTES[6] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};

enum class TlvSearchStatus {
    NotFound,
    NeedMoreData,
    Found,
};

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

bool authenticateNdefBlock(RFIDCard &card, uint8_t block) {
    MFRC522::MIFARE_Key keys[] = {
        makeKey(NDEF_KEY_BYTES),
        makeKey(DEFAULT_KEY_BYTES),
        makeKey(MAD_KEY_BYTES),
    };

    for (MFRC522::MIFARE_Key &key : keys) {
        resetMifareSession(card);

        if (card.authenticate(block, &key, false)) {
            return true;
        }

        resetMifareSession(card);

        if (card.authenticateKeyB(block, &key, false)) {
            return true;
        }

        resetMifareSession(card);
    }

    return false;
}

bool readTlvLength(const uint8_t *storage, size_t storageLen, size_t *index, size_t *length, bool *needMore) {
    *needMore = false;

    if (*index >= storageLen) {
        *needMore = true;
        return false;
    }

    *length = storage[(*index)++];

    if (*length == 0xFF) {
        if (*index + 2 > storageLen) {
            *needMore = true;
            return false;
        }

        *length = ((size_t)storage[*index] << 8) | storage[*index + 1];
        *index += 2;
    }

    return true;
}

TlvSearchStatus findCompleteNdefMessage(const uint8_t *storage, size_t storageLen, const uint8_t **message,
                                        size_t *messageLen) {
    bool sawIncompleteNdef = false;

    for (size_t start = 0; start < storageLen; start++) {
        size_t index = start;
        uint8_t type = storage[index++];

        if (type == 0x00 || type == 0xFE) {
            continue;
        }

        size_t len = 0;
        bool needMore = false;

        if (!readTlvLength(storage, storageLen, &index, &len, &needMore)) {
            if (type == 0x03 && needMore) {
                sawIncompleteNdef = true;
            }

            continue;
        }

        if (type != 0x03) {
            continue;
        }

        if (index + len > storageLen) {
            sawIncompleteNdef = true;
            continue;
        }

        *message = storage + index;
        *messageLen = len;
        return TlvSearchStatus::Found;
    }

    return sawIncompleteNdef ? TlvSearchStatus::NeedMoreData : TlvSearchStatus::NotFound;
}

size_t readMifareClassicStorage(RFIDCard &card, uint8_t *storage, size_t storageSize) {
    size_t offset = 0;
    uint8_t sectorCount = sectorCountForType(card.getType());

    for (uint8_t sector = 1; sector < sectorCount; sector++) {
        uint8_t firstBlock = firstBlockForSector(sector);
        uint8_t trailerBlock = trailerBlockForSector(sector);

        if (!authenticateNdefBlock(card, firstBlock)) {
            resetMifareSession(card);
            continue;
        }

        for (uint8_t block = firstBlock; block < trailerBlock; block++) {
            if (offset + 16 > storageSize) {
                card.stopCrypto();
                return offset;
            }

            if (card.readBlockRaw(block, storage + offset)) {
                offset += 16;

                const uint8_t *message = nullptr;
                size_t messageLen = 0;

                if (findCompleteNdefMessage(storage, offset, &message, &messageLen) == TlvSearchStatus::Found) {
                    card.stopCrypto();
                    return offset;
                }
            } else {
                break;
            }
        }

        resetMifareSession(card);

        const uint8_t *message = nullptr;
        size_t messageLen = 0;
        TlvSearchStatus status = findCompleteNdefMessage(storage, offset, &message, &messageLen);

        if (status == TlvSearchStatus::Found) {
            return offset;
        }
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

        const uint8_t *message = nullptr;
        size_t messageLen = 0;

        if (findCompleteNdefMessage(storage, offset, &message, &messageLen) == TlvSearchStatus::Found) {
            return offset;
        }
    }

    return offset;
}

bool readType2Capacity(RFIDCard &card, uint16_t *dataBytes, String &outError) {
    uint8_t ccBuffer[18];

    if (!card.readBlockRaw(3, ccBuffer)) {
        outError = "Failed to read Type 2 capability data";
        return false;
    }

    if (ccBuffer[0] != 0xE1) {
        outError = "Tag is not formatted as an NDEF Type 2 tag";
        return false;
    }

    if ((ccBuffer[3] & 0x0F) == 0x0F) {
        outError = "NDEF tag is read-only";
        return false;
    }

    *dataBytes = (uint16_t)ccBuffer[2] * 8;

    if (*dataBytes == 0) {
        outError = "Type 2 NDEF capacity is zero";
        return false;
    }

    return true;
}

bool buildTextRecord(const String &text, uint8_t *message, size_t messageSize, size_t *messageLen, String &outError) {
    size_t textLen = text.length();
    size_t payloadLen = textLen + 3;

    if (payloadLen > 255) {
        outError = "NDEF text is too long for this writer";
        return false;
    }

    size_t needed = 4 + payloadLen;

    if (needed > messageSize) {
        outError = "NDEF message is too large";
        return false;
    }

    size_t index = 0;
    message[index++] = 0xD1;
    message[index++] = 0x01;
    message[index++] = (uint8_t)payloadLen;
    message[index++] = 'T';
    message[index++] = 0x02;
    message[index++] = 'e';
    message[index++] = 'n';

    for (size_t i = 0; i < textLen; i++) {
        message[index++] = (uint8_t)text[i];
    }

    *messageLen = index;
    return true;
}

bool buildNdefTlv(const String &text, uint8_t *tlv, size_t tlvSize, size_t *tlvLen, String &outError) {
    uint8_t message[MAX_NDEF_MESSAGE];
    size_t messageLen = 0;

    if (!buildTextRecord(text, message, sizeof(message), &messageLen, outError)) {
        return false;
    }

    size_t lengthBytes = messageLen < 0xFF ? 1 : 3;
    size_t needed = 1 + lengthBytes + messageLen + 1;

    if (needed > tlvSize) {
        outError = "NDEF payload is too large";
        return false;
    }

    size_t index = 0;
    tlv[index++] = 0x03;

    if (messageLen < 0xFF) {
        tlv[index++] = (uint8_t)messageLen;
    } else {
        tlv[index++] = 0xFF;
        tlv[index++] = (uint8_t)(messageLen >> 8);
        tlv[index++] = (uint8_t)(messageLen & 0xFF);
    }

    memcpy(tlv + index, message, messageLen);
    index += messageLen;
    tlv[index++] = 0xFE;

    *tlvLen = index;
    return true;
}

bool writeType2Storage(RFIDCard &card, const uint8_t *tlv, size_t tlvLen, String &outError) {
    uint16_t dataBytes = 0;

    if (!readType2Capacity(card, &dataBytes, outError)) {
        return false;
    }

    if (tlvLen > dataBytes) {
        outError = "NDEF message is larger than Type 2 tag capacity";
        return false;
    }

    uint8_t pageBuffer[4];
    uint16_t pageCount = (dataBytes + 3) / 4;
    uint16_t requiredPages = (tlvLen + 3) / 4;

    for (uint16_t pageOffset = 0; pageOffset < requiredPages; pageOffset++) {
        memset(pageBuffer, 0, sizeof(pageBuffer));

        for (uint8_t i = 0; i < 4; i++) {
            size_t sourceIndex = (pageOffset * 4) + i;

            if (sourceIndex < tlvLen) {
                pageBuffer[i] = tlv[sourceIndex];
            }
        }

        if (!card.writePageRaw(4 + pageOffset, pageBuffer)) {
            outError = "Failed to write Type 2 NDEF page";
            return false;
        }
    }

    if (requiredPages < pageCount) {
        memset(pageBuffer, 0, sizeof(pageBuffer));

        if (!card.writePageRaw(4 + requiredPages, pageBuffer)) {
            outError = "Failed to clear old Type 2 NDEF data";
            return false;
        }
    }

    return true;
}

bool writeMifareClassicStorage(RFIDCard &card, const uint8_t *tlv, size_t tlvLen, String &outError) {
    size_t offset = 0;
    uint8_t sectorCount = sectorCountForType(card.getType());

    for (uint8_t sector = 1; sector < sectorCount; sector++) {
        uint8_t firstBlock = firstBlockForSector(sector);
        uint8_t trailerBlock = trailerBlockForSector(sector);

        if (!authenticateNdefBlock(card, firstBlock)) {
            resetMifareSession(card);
            continue;
        }

        for (uint8_t block = firstBlock; block < trailerBlock; block++) {
            uint8_t blockBuffer[16];
            memset(blockBuffer, 0, sizeof(blockBuffer));

            for (uint8_t i = 0; i < 16; i++) {
                if (offset < tlvLen) {
                    blockBuffer[i] = tlv[offset++];
                }
            }

            if (!card.writeBlockRaw(block, blockBuffer)) {
                card.stopCrypto();
                outError = "Failed to write MIFARE Classic NDEF block";
                return false;
            }

            if (offset >= tlvLen) {
                card.stopCrypto();
                return true;
            }
        }

        resetMifareSession(card);
    }

    outError = "NDEF message is larger than writable MIFARE Classic space";
    return false;
}

bool findNdefMessage(const uint8_t *storage, size_t storageLen, const uint8_t **message, size_t *messageLen,
                     String &outError) {
    TlvSearchStatus status = findCompleteNdefMessage(storage, storageLen, message, messageLen);

    if (status == TlvSearchStatus::Found) {
        return true;
    }

    outError = status == TlvSearchStatus::NeedMoreData ? "NDEF message is incomplete" : "No NDEF message found";
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
    uint8_t tlv[MAX_NDEF_TLV];
    size_t tlvLen = 0;

    if (!buildNdefTlv(data, tlv, sizeof(tlv), &tlvLen, outError)) {
        return false;
    }

    MFRC522::PICC_Type cardType = card.getType();

    if (cardType == MFRC522::PICC_TYPE_MIFARE_UL) {
        return writeType2Storage(card, tlv, tlvLen, outError);
    }

    if (isMifareClassic(cardType)) {
        return writeMifareClassicStorage(card, tlv, tlvLen, outError);
    }

    outError = "Unsupported card type for NDEF write";
    return false;
}

bool NDEFManager::format(RFIDCard &card, String &outError) {
    // NDEF format placeholder.
    // In a complete implementation, this would initialize the tag storage
    // for NDEF according to its concrete card type.

    Serial.println("NDEFManager::format stub called");
    return true;
}
