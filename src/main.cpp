#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <AsyncElegantOTA.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Hitachi.h>
#include <ArduinoJson.h>

#include <SinricPro.h>
#include <SinricProWindowAC.h>

const uint16_t kIrLed = 4; // Using GPIO5.
IRHitachiAc1 ac(kIrLed);   // Set the GPIO to be used to sending the message

const char *wifiSSID = "...";
const char *wifiPassword = "...";
const char *host = "esp32ac";

const char *sinricAppKey = "...";
const char *sinricAppSecret = "...";
const char *sinricDeviceID = "...";

AsyncWebServer server(80);

struct ACSettings
{
  bool power = false;
  uint8_t mode = kHitachiAc1Cool;
  uint8_t fan = kHitachiAc1FanAuto;
  uint8_t temp = 25;
  bool swing = false;
};

ACSettings acSettings;

void sendACCommand();
void setupSinricPro();

void setup()
{
  Serial.begin(115200);

  // Wifi Setup
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  Serial.print("Connecting to WiFi");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to ");
  Serial.println(wifiSSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // use mdns for host name resolution
  if (!MDNS.begin(host))
  { // http://<host>.local
    Serial.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }

  if (!SPIFFS.begin(true))
  {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");

  ac.begin();
  ac.calibrate();
  ac.setModel(R_LT0541_HTA_A);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", "text/html", false); });

  AsyncStaticWebHandler staticHandler = server.serveStatic("/", SPIFFS, "/");
  staticHandler.setDefaultFile("index.html");

  server.on(
      "/state", HTTP_GET,
      [](AsyncWebServerRequest *request)
      {
        DynamicJsonDocument root(1024);
        root["power"] = acSettings.power;
        root["swing"] = acSettings.swing;
        root["temp"] = acSettings.temp;

        switch (acSettings.mode)
        {
        case kHitachiAc1Auto:
          root["mode"] = 0;
          break;
        case kHitachiAc1Cool:
          root["mode"] = 1;
          break;
        case kHitachiAc1Dry:
          root["mode"] = 2;
          break;
        case kHitachiAc1Fan:
          root["mode"] = 4;
          break;
        }

        switch (acSettings.fan)
        {
        case kHitachiAc1FanAuto:
          root["fan"] = 0;
          break;
        case kHitachiAc1FanLow:
          root["fan"] = 1;
          break;
        case kHitachiAc1FanMed:
          root["fan"] = 2;
          break;
        case kHitachiAc1FanHigh:
          root["fan"] = 3;
          break;
        }

        String output;
        serializeJson(root, output);
        request->send(200, "application/json", output);
      });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler(
      "/state",
      [](AsyncWebServerRequest *request, JsonVariant &json)
      {
        JsonObject jsonObj = json.as<JsonObject>();

        if (jsonObj.containsKey("power") && jsonObj["power"].is<bool>())
          acSettings.power = jsonObj["power"];
        if (jsonObj.containsKey("swing") && jsonObj["swing"].is<bool>())
          acSettings.swing = jsonObj["swing"];

        if (jsonObj.containsKey("mode") && jsonObj["mode"].is<uint8_t>())
        {
          switch (jsonObj["mode"].as<uint8_t>())
          {
          case 0:
            acSettings.mode = kHitachiAc1Auto;
            break;
          case 1:
            acSettings.mode = kHitachiAc1Cool;
            break;
          case 2:
            acSettings.mode = kHitachiAc1Dry;
            break;
          case 4:
            acSettings.mode = kHitachiAc1Fan;
            break;
          }
        }

        if (jsonObj.containsKey("fan") && jsonObj["fan"].is<uint8_t>())
        {
          switch (jsonObj["fan"].as<uint8_t>())
          {
          case 0:
            acSettings.fan = kHitachiAc1FanAuto;
            break;
          case 1:
            acSettings.fan = kHitachiAc1FanLow;
            break;
          case 2:
            acSettings.fan = kHitachiAc1FanMed;
            break;
          case 3:
            acSettings.fan = kHitachiAc1FanHigh;
            break;
          }
        }

        if (jsonObj.containsKey("temp") && jsonObj["temp"].is<uint8_t>())
        {
          int temp = jsonObj["temp"].as<int>();
          temp = max(16, min(32, temp));
          acSettings.temp = (uint8_t)temp;
        }

        sendACCommand();

        request->send(200, "text/plain", "OK");
      });
  server.addHandler(handler);

  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->send(404, "text/plain", "Not found"); });

  // Start ElegantOTA
  AsyncElegantOTA.begin(&server);

  server.begin();
  Serial.println("HTTP server started");

  setupSinricPro();
}

void loop()
{
  SinricPro.handle();
}

void sendACCommand()
{
  if (acSettings.power)
  {
    ac.on();
    ac.setFan(acSettings.fan);
    ac.setMode(acSettings.mode);
    ac.setTemp(acSettings.temp);
    ac.setSwingV(acSettings.swing);
    ac.setSwingH(acSettings.swing);
  }
  else
    ac.off();
  ac.send();
}

void setupSinricPro()
{
  SinricProWindowAC &myAcUnit = SinricPro[sinricDeviceID];

  myAcUnit.onPowerState(
      [](const String &deviceId, bool &state) -> bool
      {
        Serial.printf("Device %s power state changed to %s\r\n", deviceId.c_str(), state ? "on" : "off");
        acSettings.power = state;
        sendACCommand();
        return true;
      });

  myAcUnit.onTargetTemperature(
      [](const String &deviceId, float &temperature) -> bool
      {
        if (temperature < 16 || temperature > 32)
          return false;
        Serial.printf("Device %s target temperature changed to %f\r\n", deviceId.c_str(), temperature);
        acSettings.temp = (uint8_t)temperature;
        sendACCommand();
        return true;
      });

  myAcUnit.onAdjustTargetTemperature(
      [](const String &deviceId, float &temperatureDelta) -> bool
      {
        if (acSettings.temp + temperatureDelta < 16 || acSettings.temp + temperatureDelta > 32)
          return false;
        acSettings.temp += (uint8_t)temperatureDelta;
        Serial.printf("Device %s target temperature changed to %f\r\n", deviceId.c_str(), acSettings.temp);
        sendACCommand();
        return true;
      });

  myAcUnit.onThermostatMode(
      [](const String &deviceId, String &mode) -> bool
      {
        Serial.printf("Device %s thermostat mode changed to %s\r\n", deviceId.c_str(), mode.c_str());
        acSettings.power = true;
        if (mode == "OFF")
          acSettings.power = false;
        else if (mode == "AUTO")
          acSettings.mode = kHitachiAc1Auto;
        else if (mode == "COOL")
          acSettings.mode = kHitachiAc1Cool;
        else if (mode == "ECO")
          acSettings.mode = kHitachiAc1Dry;
        else if (mode == "HEAT")
          acSettings.mode = kHitachiAc1Fan;
        else
          return false;
        sendACCommand();
        return true;
      });
  myAcUnit.onRangeValue(
      [](const String &deviceId, int &fanValue) -> bool
      {
        Serial.printf("Device %s fan speed changed to %d\r\n", deviceId.c_str(), fanValue);
        switch (fanValue)
        {
        case 0:
          acSettings.fan = kHitachiAc1FanAuto;
          break;
        case 1:
          acSettings.fan = kHitachiAc1FanLow;
          break;
        case 2:
          acSettings.fan = kHitachiAc1FanMed;
          break;
        case 3:
          acSettings.fan = kHitachiAc1FanHigh;
          break;
        default:
          return false;
        }
        sendACCommand();
        return true;
      });
  myAcUnit.onAdjustRangeValue(
      [](const String &deviceId, int &fanValueDelta) -> bool
      {
        Serial.printf("Device %s fan speed changed to %d\r\n", deviceId.c_str(), acSettings.fan + fanValueDelta);
        switch (acSettings.fan + fanValueDelta)
        {
        case 0:
          acSettings.fan = kHitachiAc1FanAuto;
          break;
        case 1:
          acSettings.fan = kHitachiAc1FanLow;
          break;
        case 2:
          acSettings.fan = kHitachiAc1FanMed;
          break;
        case 3:
          acSettings.fan = kHitachiAc1FanHigh;
          break;
        default:
          return false;
        }
        sendACCommand();
        return true;
      });

  // setup SinricPro
  SinricPro.onConnected([]()
                        { Serial.printf("Connected to SinricPro\r\n"); });
  SinricPro.onDisconnected([]()
                           { Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(sinricAppKey, sinricAppSecret);
}