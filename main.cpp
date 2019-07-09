#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <WiFiManager.h>
#include <EasyButton.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>

#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#include "TemperatureService.h"

#define APP_VERSION 5

#define MQTT_HOST "192.168.1.2"  // MQTT host (e.g. m21.cloudmqtt.com)
#define MQTT_PORT 11883          // MQTT port (e.g. 18076)
#define MQTT_USER "change-me"    // Ingored if brocker allows guest connection
#define MQTT_PASS "change-me"    // Ingored if brocker allows guest connection

#define DEVICE_ID       "electrolux_ac"  // Used for MQTT topics
#define WIFI_HOSTNAME   "esp-ir-coolix"  // Name of WiFi network

const char* gConfigFile = "/config.json";

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).

IRsend irsend(kIrLed);      // Set the GPIO to be used to sending the message.

WiFiManager wifiManager;                                    // WiFi Manager
WiFiClient client;                                          // WiFi Client
Adafruit_MQTT_Client mqtt(&client, MQTT_HOST, MQTT_PORT);   // MQTT client


// Setup MQTT topics
String input_topic  = String() + "mqtt2coolix/" + DEVICE_ID + "/set";
String output_topic = String() + "mqtt2coolix/" + DEVICE_ID;
Adafruit_MQTT_Subscribe mqtt_subscribe      = Adafruit_MQTT_Subscribe   (&mqtt, input_topic.c_str(), MQTT_QOS_1);
Adafruit_MQTT_Publish   mqtt_publish        = Adafruit_MQTT_Publish     (&mqtt, output_topic.c_str());


EasyButton button(0);                   // 0 - Flash button
IRCoolixAC ac(kIrLed);                  // Air conditioner
TemperatureService temperatureService;  // Temperature measurement service


void publishState()
{
    Serial.println("publishState()");
    const int JSON_SIZE = 1024;

    DynamicJsonDocument doc(JSON_SIZE);

    doc["temp"]   = ac.getTemp();
    doc["mode"]   = ac.getMode();
    doc["fan"]    = ac.getFan();
    doc["power"]  = ac.getPower();
    // doc["memory"] = system_get_free_heap_size();
    doc["uptime"] = millis() / 1000;
    doc["temp_out"] = TemperatureService::instance->getTemperature(0);
    doc["version"]  = APP_VERSION;

    char jsonStr[JSON_SIZE * 2];

    serializeJsonPretty(doc, jsonStr);

    // Publish state to output topic
    mqtt_publish.publish(jsonStr);
    Serial.print("MQTT published: ");
    Serial.println(jsonStr);
}



void setup()
{

    delay(10);
    irsend.begin();
    Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
    
    ac.begin();
    
    // Load config
    loadConfig(gConfigFile, [](DynamicJsonDocument json){
        ac.setTemp(json["temp"]);
        ac.setMode(json["mode"]);
        ac.setFan(json["fan"]);
        ac.setPower(json["power"]);
        
        Serial.println("AC state recovered from json.");
    });

    String apName = String(WIFI_HOSTNAME) + "-v" + APP_VERSION;
    WiFi.hostname(apName);
    wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.autoConnect(apName.c_str(), "12341234"); // IMPORTANT! Blocks execution. Waits until connected

    // Wait for WIFI connection

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(10);
        Serial.print(".");
    }

    Serial.print("\nConnected to ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());


    // Setup MQTT subscription for set_state topic.
    mqtt.subscribe(&mqtt_subscribe);

    // Send IR impulses on received MQTT message in the 'set' topic
    mqtt_subscribe.setCallback([](char *str, uint16_t len){

        char buf [len + 1];
        buf[len] = 0;
        strncpy(buf, str, len);

        Serial.println(String("Got mqtt message: ") + buf);

        const int JSON_SIZE = 1024;

        DynamicJsonDocument root(JSON_SIZE);
        DeserializationError error = deserializeJson(root, buf);
        
        if(error) return;

        if(root.containsKey("temp"))
            ac.setTemp(root["temp"]);           // 17..30  

        if(root.containsKey("mode"))
            ac.setMode(root["mode"]);           kCoolixCool; // 0 - cool, 1 - dry, 2 - auto, 3 - heat, 4 - fan

        if(root.containsKey("fan"))
            ac.setFan(root["fan"]);             kCoolixFanAuto; // 0 - auto0,  1 - max, 2 - med, 4 - min, 5 - auto

        if(root.containsKey("power"))
            ac.setPower(root["power"]);         // true or false
        

        Serial.println("Send COOLIX..");

        bool send = true;
        
        if(root.containsKey("send"))
            send = root["send"];

        if(send)
            ac.send(1);

        Serial.println(ac.toString());

        // Publish AC state to MQTT
        publishState();

        // Saving state to persistent storage (will be restored on reboot)

        DynamicJsonDocument json(JSON_SIZE);
        json["temp"]   = ac.getTemp();
        json["mode"]   = ac.getMode();
        json["fan"]    = ac.getFan();
        json["power"]  = ac.getPower();
        saveConfig(gConfigFile, json);

    });


    button.onPressed([](){
        Serial.println("Flash button pressed!");
        Serial.println(ac.toString());
        ac.send(1);         // send IR
        publishState();     // publish to mqtt
    });

    temperatureService.init(D5);

    ArduinoOTA.begin();

}


long    lastPublishTime = 0;
long    publishInterval = 60*1000;

void loop()
{
    ArduinoOTA.handle();
    button.read();
    temperatureService.loop();

    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect()
    MQTT_connect(&mqtt);
    
    
    // wait X milliseconds for subscription messages
    mqtt.processPackets(10);

    
    // publish state every X seconds
    if((!lastPublishTime && TemperatureService::instance->temperatures[0] > 0) || millis() > lastPublishTime + publishInterval){
        // do the publish
        publishState();
        lastPublishTime = millis();
    }

    // WARNING! .ping() causes error "dropped a packet". Disabled. Waiting for library update
    // keep the connection alive
    // if(! mqtt.ping()) {
    //     mqtt.disconnect();
    // }


}





