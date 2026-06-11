#include "wifi_manager.h"

#define AP_SSID "RFID_Manager"
#define AP_PASS "12345678"

WiFiManager::WiFiManager(AsyncWebServer *srv, AsyncWebSocket *websocket) {
    server = srv;
    ws = websocket;
}

void WiFiManager::begin() {
    prefs.begin("wifi", false);

    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);

    ensureAccessPoint();

    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info) {
        switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("STA Connected");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            ensureAccessPoint();

            wifiConnected = true;
            wifiConnecting = false;

            reconnectAttempts = 0;

            Serial.print("STA IP: ");
            Serial.println(WiFi.localIP());

            broadcastStatus();
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:

            if (scanInProgress) {
                Serial.println("Ignoring disconnect during scan");

                break;
            }

            wifiConnected = false;
            wifiConnecting = false;

            Serial.printf("STA Disconnected. Reason=%d\n", info.wifi_sta_disconnected.reason);

            if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
                reconnectAttempts++;

                Serial.printf("Reconnect %d/%d\n", reconnectAttempts, MAX_RECONNECT_ATTEMPTS);

                String ssid = prefs.getString("ssid", "");

                String pass = prefs.getString("pass", "");

                WiFi.begin(ssid.c_str(), pass.c_str());
            } else {
                Serial.println("Max reconnect attempts reached");

                clearWifiCredentials();

                WiFi.disconnect(false, false);
                WiFi.mode(WIFI_AP_STA);
                ensureAccessPoint();
            }

            broadcastStatus();

            break;

        default:
            break;
        }
    });

    connectStoredWifi();

    setupWebSocket();
}

void WiFiManager::update() { handleWifiScan(); }

void WiFiManager::broadcastStatus() {
    JsonDocument doc;

    doc["connected"] = wifiConnected;

    doc["ssid"] = connectedSSID;

    doc["ap_ip"] = WiFi.softAPIP().toString();

    if (wifiConnected) {
        doc["ip"] = WiFi.localIP().toString();
        doc["sta_ip"] = WiFi.localIP().toString();
    }

    String msg;

    serializeJson(doc, msg);

    ws->textAll(msg);
}

void WiFiManager::saveWifiCredentials(const String &ssid, const String &password) {
    prefs.putString("ssid", ssid);

    prefs.putString("pass", password);
}

void WiFiManager::startWifiConnection(const String &ssid, const String &password) {
    Serial.printf("Connecting to %s\n", ssid.c_str());

    WiFi.mode(WIFI_AP_STA);
    ensureAccessPoint();

    saveWifiCredentials(ssid, password);

    connectedSSID = ssid;

    wifiConnecting = true;

    WiFi.begin(ssid.c_str(), password.c_str());
}

void WiFiManager::connectStoredWifi() {
    if (!prefs.isKey("ssid")) {
        Serial.println("No saved WiFi credentials");

        return;
    }

    String ssid = prefs.getString("ssid");

    String pass = prefs.getString("pass");

    startWifiConnection(ssid, pass);
}

void WiFiManager::startWifiScan() {
    if (scanInProgress) {
        return;
    }

    WiFi.mode(WIFI_AP_STA);
    ensureAccessPoint();

    Serial.println("Starting WiFi scan");

    scanInProgress = true;

    WiFi.scanDelete();

    WiFi.scanNetworks(true, true);
}

void WiFiManager::handleWifiScan() {
    if (!scanInProgress) {
        return;
    }

    int count = WiFi.scanComplete();

    if (count < 0) {
        return;
    }

    Serial.printf("Found %d networks\n", count);

    String json = "{\"type\":\"scan_results\",\"networks\":[";

    bool first = true;

    for (int i = 0; i < count; i++) {
        String ssid = WiFi.SSID(i);

        if (ssid.length() == 0) {
            continue;
        }

        ssid.replace("\\", "\\\\");
        ssid.replace("\"", "\\\"");

        if (!first) {
            json += ",";
        }

        first = false;

        json += "{";
        json += "\"ssid\":\"" + ssid + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"secure\":";
        json += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "true" : "false";
        json += "}";
    }

    json += "]}";

    Serial.println(json);

    ws->textAll(json);

    WiFi.scanDelete();

    scanInProgress = false;
}

void WiFiManager::setupWebSocket() {
    ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg,
                       uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.printf("Client %u connected\n", client->id());

            JsonDocument doc;

            doc["connected"] = wifiConnected;

            doc["ssid"] = connectedSSID;

            if (wifiConnected) {
                doc["ip"] = WiFi.localIP().toString();
                doc["sta_ip"] = WiFi.localIP().toString();
            }

            doc["ap_ip"] = WiFi.softAPIP().toString();

            String msg;

            serializeJson(doc, msg);

            client->text(msg);
        }
    });

    server->addHandler(ws);
}

void WiFiManager::setupApi() {
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;

        doc["connected"] = wifiConnected;

        doc["ssid"] = connectedSSID;

        doc["ap_ip"] = WiFi.softAPIP().toString();

        if (wifiConnected) {
            doc["sta_ip"] = WiFi.localIP().toString();
        }

        String out;

        serializeJson(doc, out);

        request->send(200, "application/json", out);
    });

    server->on("/api/scan/start", HTTP_GET, [this](AsyncWebServerRequest *request) {
        startWifiScan();

        request->send(200, "application/json", "{\"status\":\"started\"}");
    });

    server->on(
        "/api/connect", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;

            auto err = deserializeJson(doc, data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");

                return;
            }

            String ssid = doc["ssid"] | "";

            String pass = doc["password"] | "";

            reconnectAttempts = 0;
            startWifiConnection(ssid, pass);

            request->send(200, "application/json", "{\"status\":\"connecting\"}");
        });
}

void WiFiManager::clearWifiCredentials() {
    prefs.remove("ssid");
    prefs.remove("pass");

    connectedSSID = "";

    reconnectAttempts = 0;

    Serial.println("WiFi credentials cleared");
}

void WiFiManager::ensureAccessPoint() {
    if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_AP_STA);
    }

    if (WiFi.softAPIP().toString() == "0.0.0.0") {
        WiFi.softAP(AP_SSID, AP_PASS);

        Serial.println();
        Serial.println("Access Point Started");

        Serial.print("AP IP: ");

        Serial.println(WiFi.softAPIP());
    }
}
