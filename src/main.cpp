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
uint16_t co2Limit = 1000;
float temperatureSCD = 0.0f;
float humiditySCD = 0.0f;
float temperatureBME = 25.0f;
float humidityBME = 50.0f;
float pressureBME = 1014.0f;
float altitude = 950.0f;
float pressureMSL = 1018.0f;
const char *ssid = "";// Enter SSID here
const char *password = "";// Enter Password here
AsyncWebServer server(80);

String processor(const String &var)
{
    if (var == "temperature")
    {
        return String(temperatureSCD);
    }
    else if (var == "humidity")
    {
        return String(humiditySCD);
    }
    else if (var == "pressure")
    {
        return String(pressureBME);
    }
    else if (var == "altitude")
    {
        return String(altitude);
    }
    else if (var == "co2")
    {
        return String(co2);
    }
    else if (var == "warnCo2")
    {
        if (co2 >= co2Limit)
        {
            return "Ventilation required!";
        }
    }
    return String();
}

void onRequest(AsyncWebServerRequest *request)
{
    request->send(SPIFFS, "/air.html", String(), false, processor);
}

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
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
    server.on("/", HTTP_GET, onRequest);
    server.onNotFound(notFound);
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
    temperatureBME = bme.readTemperature();
    humidityBME = bme.readHumidity();
    pressureBME = bme.readPressure() / 100.0F;
    altitude = bme.readAltitude(pressureMSL);
    bool isDataReady = false;
    if ((scd4x.getDataReadyFlag(isDataReady) == 0 && isDataReady == true && scd4x.readMeasurement(co2, temperatureSCD, humiditySCD) == 0 && co2 != 0) == false)
    {
        Serial.println("SCD40 data not ready!");
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

    if (co2 >= co2Limit)
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