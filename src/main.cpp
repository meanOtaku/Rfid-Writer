#include <SPI.h>
#include <MFRC522.h>

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>

#define AP_SSID "RFID_Manager"
#define AP_PASS "12345678"

#define SS_PIN 5
#define RST_PIN 22

MFRC522 mfrc522(SS_PIN, RST_PIN);

Preferences prefs;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String connectedSSID = "";
bool wifiConnected = false;

bool scanInProgress = false;
String cachedNetworks = "{\"networks\":[]}";

bool wifiConnecting = false;

void broadcastStatus()
{
  JsonDocument doc;

  doc["connected"] = wifiConnected;
  doc["ssid"] = connectedSSID;

  if (wifiConnected)
  {
    doc["ip"] = WiFi.localIP().toString();
  }

  String msg;
  serializeJson(doc, msg);

  ws.textAll(msg);
}

void saveWifiCredentials(
    const String &ssid,
    const String &password)
{
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
}

void startWifiConnection(
    const String &ssid,
    const String &password)
{
  Serial.printf(
      "Connecting to %s\n",
      ssid.c_str());

  saveWifiCredentials(
      ssid,
      password);

  connectedSSID = ssid;
  wifiConnecting = true;

  WiFi.begin(
      ssid.c_str(),
      password.c_str());
}

void connectStoredWifi()
{
  if (!prefs.isKey("ssid"))
  {
    Serial.println(
        "No saved WiFi credentials");
    return;
  }

  String ssid =
      prefs.getString("ssid");

  String pass =
      prefs.getString("pass");

  startWifiConnection(
      ssid,
      pass);
}

void startWifiScan()
{
  if (scanInProgress)
    return;

  Serial.println(
      "Starting WiFi scan");

  scanInProgress = true;

  WiFi.scanDelete();

  WiFi.scanNetworks(
      true,  // async
      true); // show hidden
}

void setupWebSocket()
{
  ws.onEvent(
      [](AsyncWebSocket *server,
         AsyncWebSocketClient *client,
         AwsEventType type,
         void *arg,
         uint8_t *data,
         size_t len)
      {
        if (type == WS_EVT_CONNECT)
        {
          Serial.printf(
              "Client %u connected\n",
              client->id());

          JsonDocument doc;

          doc["connected"] =
              wifiConnected;

          doc["ssid"] =
              connectedSSID;

          if (wifiConnected)
          {
            doc["ip"] =
                WiFi.localIP()
                    .toString();
          }

          String msg;

          serializeJson(
              doc,
              msg);

          client->text(msg);
        }
      });

  server.addHandler(&ws);
}

void setupApi()
{
  server.on(
      "/api/status",
      HTTP_GET,
      [](AsyncWebServerRequest *request)
      {
        JsonDocument doc;

        doc["connected"] =
            wifiConnected;

        doc["ssid"] =
            connectedSSID;

        doc["ap_ip"] =
            WiFi.softAPIP()
                .toString();

        if (wifiConnected)
        {
          doc["sta_ip"] =
              WiFi.localIP()
                  .toString();
        }

        String out;

        serializeJson(
            doc,
            out);

        request->send(
            200,
            "application/json",
            out);
      });
  server.on("/api/scan/start",
            HTTP_GET,
            [](AsyncWebServerRequest *request)
            {
              startWifiScan();

              request->send(
                  200,
                  "application/json",
                  "{\"status\":\"started\"}");
            });

  server.on("/api/scan/results",
            HTTP_GET,
            [](AsyncWebServerRequest *request)
            {
              request->send(
                  200,
                  "application/json",
                  cachedNetworks);
            });

  server.on(
      "/api/connect",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      nullptr,
      [](AsyncWebServerRequest *request,
         uint8_t *data,
         size_t len,
         size_t index,
         size_t total)
      {
        JsonDocument doc;

        auto err =
            deserializeJson(
                doc,
                data,
                len);

        if (err)
        {
          request->send(
              400,
              "application/json",
              "{\"error\":\"invalid json\"}");

          return;
        }

        String ssid =
            doc["ssid"] | "";

        String pass =
            doc["password"] | "";

        startWifiConnection(
            ssid,
            pass);

        request->send(
            200,
            "application/json",
            "{\"status\":\"connecting\"}");
      });

  server.serveStatic(
            "/",
            LittleFS,
            "/")
      .setDefaultFile(
          "index.html");

  server.begin();
}

void setup()
{
  Serial.begin(115200);

  if (!LittleFS.begin(true))
  {
    Serial.println(
        "LittleFS mount failed");

    while (true)
    {
      delay(1000);
    }
  }

  prefs.begin(
      "wifi",
      false);

  WiFi.mode(
      WIFI_AP_STA);

  WiFi.softAP(
      AP_SSID,
      AP_PASS);

  SPI.begin(18, 19, 23, 5);

  mfrc522.PCD_Init();

  Serial.println("RC522 Initialized");

  Serial.println();
  Serial.println(
      "Access Point Started");

  Serial.print(
      "AP IP: ");

  Serial.println(
      WiFi.softAPIP());

  WiFi.onEvent(
      [](WiFiEvent_t event)
      {
        switch (event)
        {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:

          Serial.println(
              "STA Connected");

          break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:

          wifiConnected =
              true;

          wifiConnecting =
              false;

          Serial.print(
              "STA IP: ");

          Serial.println(
              WiFi.localIP());

          broadcastStatus();

          break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:

          wifiConnected =
              false;

          wifiConnecting =
              false;

          Serial.println(
              "STA Disconnected");

          broadcastStatus();

          break;

        default:
          break;
        }
      });

  connectStoredWifi();

  setupWebSocket();
  setupApi();
}

void loop()
{
  ws.cleanupClients();

  if (scanInProgress)
  {
    int count =
        WiFi.scanComplete();

    if (count >= 0)
    {
      Serial.printf(
          "Found %d networks\n",
          count);

      JsonDocument doc;

      JsonArray arr =
          doc["networks"]
              .to<JsonArray>();

      for (int i = 0;
           i < count;
           i++)
      {
        JsonObject net =
            arr.add<JsonObject>();

        net["ssid"] =
            WiFi.SSID(i);

        net["rssi"] =
            WiFi.RSSI(i);

        net["secure"] =
            WiFi.encryptionType(i) !=
            WIFI_AUTH_OPEN;
      }

      serializeJson(
          doc,
          cachedNetworks);

      WiFi.scanDelete();

      scanInProgress =
          false;
    }
  }

  if (!mfrc522.PICC_IsNewCardPresent())
    return;

  if (!mfrc522.PICC_ReadCardSerial())
    return;

  Serial.print("UID: ");

  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    if (mfrc522.uid.uidByte[i] < 0x10)
      Serial.print("0");

    Serial.print(
        mfrc522.uid.uidByte[i],
        HEX);

    Serial.print(" ");
  }

  Serial.println();

  MFRC522::PICC_Type type =
      mfrc522.PICC_GetType(
          mfrc522.uid.sak);

  Serial.print("Type: ");
  Serial.println(
      mfrc522.PICC_GetTypeName(type));

  mfrc522.PICC_HaltA();
}