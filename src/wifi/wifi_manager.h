#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>

class WiFiManager
{
public:
    WiFiManager(
        AsyncWebServer* server,
        AsyncWebSocket* ws);

    void begin();

    void update();

    void setupApi();

private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;

    Preferences prefs;

    String connectedSSID;

    bool wifiConnected = false;
    bool wifiConnecting = false;

    bool scanInProgress = false;

    void broadcastStatus();

    void saveWifiCredentials(
        const String& ssid,
        const String& password);

    void startWifiConnection(
        const String& ssid,
        const String& password);

    void connectStoredWifi();

    void startWifiScan();

    void handleWifiScan();

    void setupWebSocket();
};