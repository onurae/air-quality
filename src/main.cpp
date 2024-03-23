/*******************************************************************************************
 *                                                                                         *
 *    air-quality                                                                          *
 *                                                                                         *
 *    Copyright (c) 2024 Onur AKIN <https://github.com/onurae>                             *
 *    Licensed under the MIT License.                                                      *
 *                                                                                         *
 ******************************************************************************************/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SensirionI2CScd4x.h>
#include "SPIFFS.h"

Adafruit_BME280 bme;
SensirionI2CScd4x scd4x;
uint16_t co2 = 0;
float temperatureSCD = 0.0f;
float humiditySCD = 0.0f;
float temperatureBME = 25.0f;
float humidityBME = 50.0f;
float pressureBME = 1014.0f;
float altitude = 950.0f;
float pressureMSL = 1018.0f;
const char *ssid = "";     // Enter SSID here
const char *password = ""; // Enter Password here
AsyncWebServer server(80);
std::deque<float> tDeque; // For history data.
std::deque<float> hDeque;
std::deque<float> pDeque;
std::deque<float> cDeque;
const int maxSize = 1 * 60 * 24; // 24h, 1min interval.
int k = 0;                       // 1min counter.
std::vector<float> tBuf;         // For 1min average data.
std::vector<float> hBuf;
std::vector<float> pBuf;
std::vector<float> cBuf;
float tAve = 0; // Average data.
float hAve = 0;
float pAve = 0;
float cAve = 0;

String PrepareChartData(const std::deque<float> &deq)
{
    String str = "";
    for (int i = 0; i < deq.size(); i++)
    {
        str = i == 0 ? String(deq[0]) : str + "," + String(deq[i]);
    }
    return str;
}

void setup()
{
    // Serial port
    Serial.begin(115200);

    // Leds
    pinMode(5, OUTPUT);
    pinMode(7, OUTPUT);
    digitalWrite(5, LOW);
    digitalWrite(7, LOW);
    delay(5000);
    Serial.println("air quality!");

    // Sensors
    Wire.setPins(8, 9);
    bme.begin(0x76);
    scd4x.begin(Wire);
    uint16_t error = 0;
    error += scd4x.stopPeriodicMeasurement();                                 // Stop potentially previously started measurement
    error += scd4x.setAmbientPressure(uint16_t(bme.readPressure() / 100.0F)); // Set pressure, overrides pressure compensation based on a previously set sensor altitude.
    error += scd4x.setAutomaticSelfCalibration(0);                            // Turn off asc.
    error += scd4x.startPeriodicMeasurement();                                // Start periodic measurement.
    if (error != 0)
    {
        Serial.println("SCD40 error!");
        while (true)
        {
            delay(1000);
        }
    }

    // Wi-fi
    Serial.println("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);           // Connect to your local wi-fi network.
    while (WiFi.status() != WL_CONNECTED) // Check wi-fi is connected to wi-fi network.
    {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected..!");
    Serial.print("Got IP: ");
    Serial.println(WiFi.localIP());

    // mDNS: "air.local"
    if (!MDNS.begin("air"))
    {
        Serial.println("Error setting up MDNS responder!");
        while (true)
        {
            delay(1000);
        }
    }
    Serial.println("mDNS responder started.");

    // Server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/air.html"); });
    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(404, "text/plain", "Not found"); });
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", (String(temperatureSCD) + "," + String(humiditySCD) + "," + String(pressureBME) + "," + String(co2)).c_str()); });
    server.on("/data-average", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", (String(tAve) + "," + String(hAve) + "," + String(pAve) + "," + String(cAve)).c_str()); });
    server.on("/chart-temperature", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", PrepareChartData(tDeque).c_str()); });
    server.on("/chart-humidity", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", PrepareChartData(hDeque).c_str()); });
    server.on("/chart-pressure", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", PrepareChartData(pDeque).c_str()); });
    server.on("/chart-co2", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", PrepareChartData(cDeque).c_str()); });
    server.begin();
    Serial.println("HTTP server started.");

    // SPIFFS
    if (SPIFFS.begin(true) == false)
    {
        Serial.println("SPIFFS error!");
        while (true)
        {
            delay(1000);
        }
    }
}

void loop()
{
    k++;
    temperatureBME = bme.readTemperature();
    humidityBME = bme.readHumidity();
    pressureBME = bme.readPressure() / 100.0F;
    altitude = bme.readAltitude(pressureMSL);
    scd4x.setAmbientPressure(uint16_t(bme.readPressure() / 100.0F));
    bool isDataReady = false;
    if ((scd4x.getDataReadyFlag(isDataReady) == 0 && isDataReady == true && scd4x.readMeasurement(co2, temperatureSCD, humiditySCD) == 0 && co2 != 0) == false)
    {
        Serial.println("SCD40 data not ready!");
    }
    else
    {
        tBuf.push_back(temperatureSCD);
        hBuf.push_back(humiditySCD);
        pBuf.push_back(pressureBME);
        cBuf.push_back(co2);
        if (k >= 12) // 1min average.
        {
            float tSum = 0;
            float hSum = 0;
            float pSum = 0;
            float cSum = 0;
            for (auto &element : tBuf)
                tSum += element;
            for (auto &element : hBuf)
                hSum += element;
            for (auto &element : pBuf)
                pSum += element;
            for (auto &element : cBuf)
                cSum += element;
            tAve = tSum / tBuf.size();
            hAve = hSum / hBuf.size();
            pAve = pSum / pBuf.size();
            cAve = cSum / cBuf.size();
            k = 0;
            tBuf.clear();
            hBuf.clear();
            pBuf.clear();
            cBuf.clear();

            if (tDeque.size() == maxSize)
            {
                Serial.println("Deque is full!");
                tDeque.pop_front();
                hDeque.pop_front();
                pDeque.pop_front();
                cDeque.pop_front();
                tDeque.push_back(tAve);
                hDeque.push_back(hAve);
                pDeque.push_back(pAve);
                cDeque.push_back(cAve);
            }
            else
            {
                tDeque.push_back(tAve);
                hDeque.push_back(hAve);
                pDeque.push_back(pAve);
                cDeque.push_back(cAve);
            }
        }
    }
    Serial.print("Co2:");
    Serial.print(co2);
    Serial.print(",");
    Serial.print("TemperatureSCD:");
    Serial.print(temperatureSCD);
    Serial.print(",");
    Serial.print("HumiditySCD:");
    Serial.print(humiditySCD);
    Serial.print(",");
    Serial.print("TemperatureBME:");
    Serial.print(temperatureBME);
    Serial.print(",");
    Serial.print("HumidityBME:");
    Serial.print(humidityBME);
    Serial.print(",");
    Serial.print("PressureBME:");
    Serial.println(pressureBME);
    delay(5000);

    if ((WiFi.status() != WL_CONNECTED))
    {
        Serial.println("Reconnecting to WiFi...");
        WiFi.disconnect();
        WiFi.reconnect();
    }

    if (co2 >= 1000)
    {
        digitalWrite(5, HIGH);
        digitalWrite(7, LOW);
    }
    else
    {
        digitalWrite(5, LOW);
        digitalWrite(7, HIGH);
    }
}