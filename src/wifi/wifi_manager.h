#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <WiFi.h>

class WiFiManager {
public:
    WiFiManager(AsyncWebServer *server, AsyncWebSocket *ws);

    void begin();

    void update();

    void setupApi();

private:
    AsyncWebServer *server;
    AsyncWebSocket *ws;

    Preferences prefs;

    String connectedSSID;

    bool wifiConnected = false;
    bool wifiConnecting = false;

    bool scanInProgress = false;

    unsigned long connectionStartedAt = 0;

    static constexpr int MAX_SAVED_WIFI_NETWORKS = 8;
    static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;

    void broadcastStatus();

    void addStatusFields(JsonDocument &doc);

    void addSavedNetworks(JsonArray networks);

    bool isStationConnected();

    void syncConnectionState();

    int savedNetworkCount();

    String savedNetworkKey(const char *prefix, int index);

    String getSavedSSID(int index);

    String getSavedPassword(int index);

    int findSavedNetwork(const String &ssid);

    bool getSavedPassword(const String &ssid, String &password);

    bool deleteSavedNetwork(const String &ssid);

    void migrateSavedWifiCredentials();

    void saveWifiCredentials(const String &ssid, const String &password);

    void startWifiConnection(const String &ssid, const String &password, bool saveCredentials);

    void startWifiScan();

    void handleWifiScan();

    void setupWebSocket();

    void clearWifiCredentials();

    void ensureAccessPoint();
};
