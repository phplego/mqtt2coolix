#include "TemperatureService.h"

const char *    TemperatureService::ADDRESS_OUT            = "28ee3577911302da";
const char *    TemperatureService::ADDRESS_IN             = "282c4445920e0245";

TemperatureService* TemperatureService::instance = NULL;

TemperatureService::TemperatureService()
{
    instance = this;
}

void TemperatureService::init(int _pin)
{
    pin     = _pin;
    oneWire = new OneWire(pin);
    DS18B20 = new DallasTemperature(oneWire);

    DS18B20->begin();

    // Loop through each device, save its address
    Serial.print("DS18B20 device count: ");
    Serial.println(DS18B20->getDeviceCount());
    for (int i = 0; i < DS18B20->getDeviceCount(); i++) {
        // save device's address
        DS18B20->getAddress(addresses[i], i);
        Serial.println(String("device: ") + getAddressToString(addresses[i]));
    }

    this->DS18B20->setWaitForConversion(true);  // Wait for measurement
    this->DS18B20->requestTemperatures();       // Initiate the temperature measurement
}


float TemperatureService::getTemperature(int deviceIndex)
{
    if(deviceIndex >= this->DS18B20->getDeviceCount() || deviceIndex < 0)
    {
        return 0.0;
    }

    return temperatures[deviceIndex];
}


float TemperatureService::getTemperatureByAddress(const char * sensorAddress)
{
    int index = this->getDeviceIndex(sensorAddress);
    return this->getTemperature(index);
}

int TemperatureService::getDeviceIndex(const char * sensorAddress)
{
    for(int i = 0; i < DS18B20->getDeviceCount(); i++){
        if (strcmp(getAddressToString(addresses[i]).c_str(), sensorAddress) == 0){
            return i;
        }
    }
    return 0;
}

void TemperatureService::loop()
{
    long now = millis();


    if ( now - lastUpdateTime > interval ) { // Take a measurement at a fixed time (tempMeasInterval = 1000ms, 1s)

        for (int i = 0; i < DS18B20->getDeviceCount(); i++) {
            temperatures[i] = this->DS18B20->getTempC( addresses[i] ); // Measuring temperature in Celsius and save the measured value to the array
            Serial.print(String() +  i + ") " + getAddressToString(addresses[i]) + " = " + temperatures[i] + " ºC \t");
        }
        Serial.println(String(" dev count: ") + DS18B20->getDeviceCount()) ;

        // const int JSON_SIZE = 300;


        // // send temperatures as json to web socket
        // StaticJsonBuffer<JSON_SIZE> jsonBuffer;
        // JsonObject& jsonRoot = jsonBuffer.createObject();
        // JsonObject& jsonTemperatures = jsonRoot.createNestedObject("temperatures");

        // for (int i = 0; i < DS18B20->getDeviceCount(); i++) {
        //     jsonTemperatures[getAddressToString(addresses[i])] = temperatures[i];
        // }

        // char jsonStr[JSON_SIZE];
        // jsonRoot.prettyPrintTo(jsonStr, JSON_SIZE);
        //WebSocketService::instance->webSocket->broadcastTXT(jsonStr);        

        // request next measurement
        this->DS18B20->setWaitForConversion(false); //No waiting for measurement
        this->DS18B20->requestTemperatures();       //Initiate the temperature measurement
        
        lastUpdateTime = millis();                  //Remember the last time measurement
    }
}
