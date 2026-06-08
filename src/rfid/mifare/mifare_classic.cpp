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

bool MifareClassic::readNDEF(RFIDCard &card, String &outData, String &outError) {
    // NDEF Placeholder implementation.
    // In a complete implementation, this would:
    // 1. Check if card contains NDEF structure.
    // 2. Authenticate using NDEF Key (e.g., A0A1A2A3A4A5).
    // 3. Locate NDEF Message TLV (0x03).
    // 4. Read the NDEF payload (Text, URI, etc.).
    
    Serial.println("MifareClassic::readNDEF stub called");
    
    // Returning a dummy string to demonstrate NDEF reading capability:
    outData = "Hello from NDEF! (Stub)";
    return true;
}

bool MifareClassic::writeNDEF(RFIDCard &card, const String &data, String &outError) {
    // NDEF Placeholder implementation.
    // In a complete implementation, this would:
    // 1. Construct NDEF Text or URI record.
    // 2. Write the NDEF TLV wrapper.
    // 3. Store across the data blocks in NDEF-enabled sectors.
    
    Serial.print("MifareClassic::writeNDEF stub called with data: ");
    Serial.println(data);
    
    return true;
}

bool MifareClassic::formatNDEF(RFIDCard &card, String &outError) {
    // NDEF Format Placeholder.
    // In a complete implementation, this would:
    // 1. Write Mifare Application Directory (MAD) in sector 0.
    // 2. Set Sector Trailer keys of NDEF sectors to A0A1A2A3A4A5.
    // 3. Initialize NDEF TLV block (e.g. empty NDEF container).
    
    Serial.println("MifareClassic::formatNDEF stub called");
    return true;
}
