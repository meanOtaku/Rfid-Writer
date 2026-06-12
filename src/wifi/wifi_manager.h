#pragma once

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

    int reconnectAttempts = 0;
    unsigned long lastReconnectAttempt = 0;

    static constexpr int MAX_RECONNECT_ATTEMPTS = 3;
    static constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;

    void broadcastStatus();

    bool isStationConnected();

    void syncConnectionState();

    void reconnectStoredWifi();

    void saveWifiCredentials(const String &ssid, const String &password);

    void startWifiConnection(const String &ssid, const String &password);

    void connectStoredWifi();

    void startWifiScan();

    void handleWifiScan();

    void setupWebSocket();

    void clearWifiCredentials();

    void ensureAccessPoint();
};
