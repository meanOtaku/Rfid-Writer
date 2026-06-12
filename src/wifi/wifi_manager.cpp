#include "wifi_manager.h"

#define AP_SSID "RFID_Manager"
#define AP_PASS "12345678"

WiFiManager::WiFiManager(AsyncWebServer *srv, AsyncWebSocket *websocket) {
    server = srv;
    ws = websocket;
}

void WiFiManager::begin() {
    prefs.begin("wifi", false);
    migrateSavedWifiCredentials();

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
            connectedSSID = WiFi.SSID();

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
            Serial.println("WiFi disconnected; manual reconnect required");

            broadcastStatus();

            break;

        default:
            break;
        }
    });

    if (savedNetworkCount() == 0) {
        Serial.println("No saved WiFi credentials");
    } else {
        Serial.printf("%d saved WiFi network(s). Connect manually from Web UI.\n", savedNetworkCount());
    }

    setupWebSocket();
}

void WiFiManager::update() {
    handleWifiScan();
    syncConnectionState();

    if (wifiConnecting && !wifiConnected && millis() - connectionStartedAt >= WIFI_CONNECT_TIMEOUT_MS) {
        wifiConnecting = false;
        broadcastStatus();
    }
}

bool WiFiManager::isStationConnected() {
    return WiFi.status() == WL_CONNECTED && WiFi.localIP().toString() != "0.0.0.0";
}

void WiFiManager::syncConnectionState() {
    bool connected = isStationConnected();

    if (connected == wifiConnected) {
        return;
    }

    wifiConnected = connected;
    wifiConnecting = false;

    if (connected) {
        connectedSSID = WiFi.SSID();
    }

    ensureAccessPoint();
    broadcastStatus();
}

void WiFiManager::addStatusFields(JsonDocument &doc) {
    doc["connected"] = wifiConnected;

    doc["ssid"] = connectedSSID;

    doc["ap_ip"] = WiFi.softAPIP().toString();

    bool hasSavedCredentials = savedNetworkCount() > 0;

    doc["wifi_connecting"] = wifiConnecting;
    doc["wifi_retry_active"] = false;
    doc["wifi_retry_paused"] = false;
    doc["wifi_retry_attempt"] = 0;
    doc["wifi_retry_max"] = 0;
    doc["wifi_has_saved"] = hasSavedCredentials;
    doc["wifi_manual"] = true;

    if (wifiConnected) {
        doc["ip"] = WiFi.localIP().toString();
        doc["sta_ip"] = WiFi.localIP().toString();
    }
}

void WiFiManager::broadcastStatus() {
    JsonDocument doc;

    addStatusFields(doc);

    String msg;

    serializeJson(doc, msg);

    ws->textAll(msg);
}

void WiFiManager::addSavedNetworks(JsonArray networks) {
    for (int i = 0; i < savedNetworkCount(); i++) {
        String ssid = getSavedSSID(i);

        if (ssid.length() == 0) {
            continue;
        }

        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = ssid;
    }
}

int WiFiManager::savedNetworkCount() {
    int count = prefs.getInt("saved_count", 0);

    if (count < 0) {
        return 0;
    }

    if (count > MAX_SAVED_WIFI_NETWORKS) {
        return MAX_SAVED_WIFI_NETWORKS;
    }

    return count;
}

String WiFiManager::savedNetworkKey(const char *prefix, int index) {
    return String(prefix) + String(index);
}

String WiFiManager::getSavedSSID(int index) {
    return prefs.getString(savedNetworkKey("ssid", index).c_str(), "");
}

String WiFiManager::getSavedPassword(int index) {
    return prefs.getString(savedNetworkKey("pass", index).c_str(), "");
}

int WiFiManager::findSavedNetwork(const String &ssid) {
    for (int i = 0; i < savedNetworkCount(); i++) {
        if (getSavedSSID(i) == ssid) {
            return i;
        }
    }

    return -1;
}

bool WiFiManager::getSavedPassword(const String &ssid, String &password) {
    int index = findSavedNetwork(ssid);

    if (index < 0) {
        return false;
    }

    password = getSavedPassword(index);

    return true;
}

bool WiFiManager::deleteSavedNetwork(const String &ssid) {
    int index = findSavedNetwork(ssid);

    if (index < 0) {
        return false;
    }

    int count = savedNetworkCount();

    for (int i = index; i < count - 1; i++) {
        prefs.putString(savedNetworkKey("ssid", i).c_str(), getSavedSSID(i + 1));
        prefs.putString(savedNetworkKey("pass", i).c_str(), getSavedPassword(i + 1));
    }

    prefs.remove(savedNetworkKey("ssid", count - 1).c_str());
    prefs.remove(savedNetworkKey("pass", count - 1).c_str());
    prefs.putInt("saved_count", count - 1);

    if (connectedSSID == ssid) {
        WiFi.disconnect(false);
        wifiConnected = false;
        wifiConnecting = false;
        connectedSSID = "";
    }

    return true;
}

void WiFiManager::migrateSavedWifiCredentials() {
    if (!prefs.isKey("ssid") || savedNetworkCount() > 0) {
        return;
    }

    String ssid = prefs.getString("ssid", "");
    String password = prefs.getString("pass", "");

    if (ssid.length() == 0) {
        return;
    }

    prefs.putString("ssid0", ssid);
    prefs.putString("pass0", password);
    prefs.putInt("saved_count", 1);

    Serial.println("Migrated saved WiFi credential");
}

void WiFiManager::saveWifiCredentials(const String &ssid, const String &password) {
    if (ssid.length() == 0) {
        return;
    }

    int index = findSavedNetwork(ssid);

    if (index < 0) {
        int count = savedNetworkCount();

        if (count >= MAX_SAVED_WIFI_NETWORKS) {
            index = MAX_SAVED_WIFI_NETWORKS - 1;
        } else {
            index = count;
            prefs.putInt("saved_count", count + 1);
        }
    }

    prefs.putString(savedNetworkKey("ssid", index).c_str(), ssid);
    prefs.putString(savedNetworkKey("pass", index).c_str(), password);

    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
}

void WiFiManager::startWifiConnection(const String &ssid, const String &password, bool saveCredentials) {
    if (ssid.length() == 0) {
        return;
    }

    Serial.printf("Connecting to %s\n", ssid.c_str());

    WiFi.mode(WIFI_AP_STA);
    ensureAccessPoint();

    if (saveCredentials) {
        saveWifiCredentials(ssid, password);
    }

    connectedSSID = ssid;

    wifiConnecting = true;

    WiFi.begin(ssid.c_str(), password.c_str());
    connectionStartedAt = millis();
    broadcastStatus();
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

            addStatusFields(doc);

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

        addStatusFields(doc);

        String out;

        serializeJson(doc, out);

        request->send(200, "application/json", out);
    });

    server->on("/api/scan/start", HTTP_GET, [this](AsyncWebServerRequest *request) {
        startWifiScan();

        request->send(200, "application/json", "{\"status\":\"started\"}");
    });

    server->on("/api/wifi/saved", HTTP_GET, [this](AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonArray networks = doc["networks"].to<JsonArray>();

        addSavedNetworks(networks);

        String out;
        serializeJson(doc, out);

        request->send(200, "application/json", out);
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

            if (ssid.length() == 0) {
                request->send(400, "application/json", "{\"error\":\"missing ssid\"}");

                return;
            }

            startWifiConnection(ssid, pass, true);

            request->send(200, "application/json", "{\"status\":\"connecting\"}");
        });

    server->on(
        "/api/wifi/connect-saved", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;

            auto err = deserializeJson(doc, data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");

                return;
            }

            String ssid = doc["ssid"] | "";
            String password;

            if (!getSavedPassword(ssid, password)) {
                request->send(404, "application/json", "{\"error\":\"saved network not found\"}");

                return;
            }

            startWifiConnection(ssid, password, false);

            request->send(200, "application/json", "{\"status\":\"connecting\"}");
        });

    server->on(
        "/api/wifi/delete-saved", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;

            auto err = deserializeJson(doc, data, len);

            if (err || !doc["ssids"].is<JsonArray>()) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");

                return;
            }

            int deleted = 0;

            for (JsonVariant value : doc["ssids"].as<JsonArray>()) {
                String ssid = value.as<String>();

                if (deleteSavedNetwork(ssid)) {
                    deleted++;
                }
            }

            broadcastStatus();

            JsonDocument response;
            response["deleted"] = deleted;

            String out;
            serializeJson(response, out);

            request->send(200, "application/json", out);
        });
}

void WiFiManager::clearWifiCredentials() {
    for (int i = 0; i < savedNetworkCount(); i++) {
        prefs.remove(savedNetworkKey("ssid", i).c_str());
        prefs.remove(savedNetworkKey("pass", i).c_str());
    }

    prefs.remove("saved_count");
    prefs.remove("ssid");
    prefs.remove("pass");

    connectedSSID = "";

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
