                    Home WiFi
                        |
                 STA Connection
                        |
                 +-------------+
                 |   ESP32     |
                 |             |
                 | Web Server  |
                 | WebSocket   |
                 | RC522 RFID  |
                 | LittleFS    |
                 +-------------+
                        ^
                        |
                 AP Connection
                        |
             Phone / Laptop Browser


             http://192.168.4.1

             RFID_WRITER/
│
├── platformio.ini
│
├── src/
│   ├── main.cpp
│   ├── wifi_manager.cpp
│   ├── wifi_manager.h
│   ├── rfid_manager.cpp
│   └── rfid_manager.h
│
├── include/
│
├── data/
│   ├── index.html
│   ├── app.js
│   ├── style.css
│   └── logo.png
│
└── test/