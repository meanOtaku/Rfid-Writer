# ESP32 RFID Writer

ESP32-based RFID/NFC web tool for reading and writing MIFARE Classic blocks and NDEF text messages from a browser.

The ESP32 starts its own WiFi access point and also supports connecting to a home or office WiFi network. The access point stays enabled even when station WiFi is connected, so the tool remains reachable directly from a phone or laptop.

## Features

- Web UI served from LittleFS
- Always-on ESP32 access point
- Optional station WiFi connection and saved credentials
- WebSocket updates for card detection, reads, writes, and WiFi status
- MIFARE Classic raw block read/write tools
- NDEF read/write tools for phone-compatible text records
- NDEF read support for common phone-written records, including Text, URI, Smart Poster, and `text/plain`
- Layered RFID code split into card, MIFARE Classic, and NDEF modules

## Hardware

- ESP32 development board
- MFRC522 / RC522 RFID reader
- Yellow, red, and green status LEDs with suitable resistors
- MIFARE Classic or NDEF-compatible NFC tags

Default RC522 wiring used by the firmware:

| RC522 | ESP32 |
| --- | --- |
| SDA / SS | GPIO 5 |
| RST | GPIO 22 |
| SCK | GPIO 18 |
| MISO | GPIO 19 |
| MOSI | GPIO 23 |
| 3.3V | 3.3V |
| GND | GND |

Use 3.3V only. Do not power the RC522 from 5V.

Default status LED wiring:

| LED | ESP32 |
| --- | --- |
| Yellow, no card | GPIO 25 |
| Red, card/read/write error | GPIO 26 |
| Green, successful card read/write | GPIO 27 |

Each LED should be wired with a suitable current-limiting resistor.

## Web Access

Default access point:

- SSID: `RFID_Manager`
- Password: `12345678`
- Browser URL: `http://192.168.4.1`

After connecting the ESP32 to another WiFi network, the AP remains active. You can use either the AP address or the station IP shown in the WiFi page/serial monitor.

## Project Structure

```text
src/
├── main.cpp
├── wifi/
│   ├── wifi_manager.h
│   └── wifi_manager.cpp
└── rfid/
    ├── rfid_manager.h
    ├── rfid_manager.cpp
    ├── card/
    │   ├── rfid_card.h
    │   └── rfid_card.cpp
    ├── mifare/
    │   ├── mifare_classic.h
    │   └── mifare_classic.cpp
    └── ndef/
        ├── ndef_manager.h
        └── ndef_manager.cpp

data/
├── index.html
├── style.css
├── wifi/
│   ├── wifi.html
│   ├── wifi.js
│   └── style.css
└── rfid/
    ├── mifare/
    │   ├── mifare.html
    │   └── mifare.js
    └── ndef/
        ├── ndef.html
        └── ndef.js
```

## Build And Upload

This project uses PlatformIO.

Install dependencies and build:

```bash
pio run
```

Upload firmware:

```bash
pio run --target upload
```

Upload the LittleFS web UI:

```bash
pio run --target uploadfs
```

When web files in `data/` change, upload LittleFS again. When files in `src/` change, upload firmware again. If both changed, upload both.

Open serial monitor:

```bash
pio device monitor
```

Serial speed is `115200`.

## Using The Web UI

1. Power the ESP32.
2. Connect to the `RFID_Manager` WiFi access point.
3. Open `http://192.168.4.1`.
4. Use the home page to open WiFi, MIFARE Classic, or NDEF tools.

### NDEF Tools

Use this page for phone-compatible NFC text data.

- Select `Read NDEF` and tap a tag to read phone-written text.
- Select `Write NDEF`, enter text, and tap a writable NDEF-compatible tag.
- Switch back to `Read NDEF` and tap again to confirm the written data.

The writer creates a standard NFC Text record using language code `en`.

### MIFARE Classic Tools

Use this page for raw MIFARE Classic block access.

- Select `Read Block` to read a specific block.
- Select `Write Block` to write text into a specific data block.
- Sector trailer blocks are protected from normal writes.

Block `4` is the default first data block in sector 1.

## Tag Notes And Limitations

- Phone-readable NDEF tags are often Type 2 / NTAG tags, not MIFARE Classic.
- Some Android apps can format or lock tags in ways that change access keys or make areas read-only.
- If a MIFARE Classic sector key is unknown, this project cannot read or write that sector.
- NDEF write works only when the tag storage is writable and authentication succeeds.
- Writing NDEF replaces the beginning of the NDEF storage area and clears the next old page/block to prevent stale data from appearing after shorter writes.
- The MFRC522 reader supports common 13.56 MHz tags, but not every NFC tag type supported by phones.

## Development Notes

- `main.cpp` only wires together LittleFS, WiFi, web server, WebSocket, and RFID manager.
- RFID behavior is routed through `RFIDManager`.
- Low-level card operations live in `rfid/card`.
- Raw MIFARE Classic behavior lives in `rfid/mifare`.
- NDEF parsing and writing lives in `rfid/ndef`.
- Browser pages send mode/config updates through `/api/rfid/config`.
- Runtime card updates are sent through `/ws`.

## Troubleshooting

### Web page did not change

Upload the LittleFS filesystem:

```bash
pio run --target uploadfs
```

### Firmware behavior did not change

Upload firmware:

```bash
pio run --target upload
```

### NDEF read works but write fails

Check the Read Result/status message. Common causes:

- Tag is read-only or locked.
- Tag capacity is too small for the entered text.
- MIFARE Classic sector authentication failed.
- The tag type is not supported by MFRC522 or this firmware.

### MIFARE Classic write fails on an Android-formatted card

The Android app may have changed sector keys or access bits. If the sector key is unknown, the firmware cannot rewrite that sector.
