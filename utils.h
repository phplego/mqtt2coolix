#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>
#include <Adafruit_MQTT_Client.h>

String getContentType(String filename);
String getAddressToString(DeviceAddress deviceAddress);
bool mountSpiffs(void); 
bool loadConfig(const char * filename, void onLoadCallback(DynamicJsonDocument));
bool saveConfig(const char * filename, DynamicJsonDocument json);
void MQTT_connect(Adafruit_MQTT_Client * mqtt);