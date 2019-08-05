#pragma once

#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include "utils.h"
#include "TemperatureService.h"



class WebService {
    public:
        WiFiManager*        wifiManager;
        ESP8266WebServer*   server;
    
    public:
        // Default Constructor 
        WebService(WiFiManager*); 
        void init();
        void loop();

    private:
        bool handleFileRead(String path);
};