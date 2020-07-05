#pragma once

#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "utils.h"
#include "Queue.h"

#define ONE_WIRE_MAX_DEV    15                              // The maximum number of devices



class TemperatureService {
    public:
        static TemperatureService* instance;
        static const char *    ADDRESS_OUT;
        static const char *    ADDRESS_IN;
        static const char *    ADDRESS_BOARD;

        // One wire pin, connected to the sensors
        int                 pin;
        
        // Interval in milliseconds of the data reading
        int                 interval            = 1000;     // 1 second by default
        long                lastUpdateTime      = 0;

        // Is the first measurement done
        bool                ready               = false;    

        // Array of device addresses
        DeviceAddress       addresses[ONE_WIRE_MAX_DEV];     // An array device temperature sensors

        // Array of measured values
        float               temperatures[ONE_WIRE_MAX_DEV]; // Saving the last measurement of temperature
        
        // Array of Queues
        Queue<10>           queues [ONE_WIRE_MAX_DEV];

        OneWire*            oneWire;
        DallasTemperature*  DS18B20;


    public:
        TemperatureService();
        void    init(int _pin);
        void    loop();
        int     getDeviceCount();
        int     getDeviceIndex(const char * sensorAddress);
        float   getTemperature(int deviceIndex);
        float   getTemperatureByAddress(const char * sensorAddress);
};


