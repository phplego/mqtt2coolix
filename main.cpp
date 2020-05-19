#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <WiFiManager.h>
#include <EasyButton.h>

#include <IRremoteESP8266.h>
#include <ir_Coolix.h>

#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#include "TemperatureService.h"
#include "WebService.h"
#include "ChangesDetector.h"
#include "Globals.h"


#define MQTT_HOST "192.168.1.157"   // MQTT host (e.g. m21.cloudmqtt.com)
#define MQTT_PORT 11883             // MQTT port (e.g. 18076)
#define MQTT_USER "change-me"       // Ingored if brocker allows guest connection
#define MQTT_PASS "change-me"       // Ingored if brocker allows guest connection

#define DEVICE_ID       "electrolux_ac"  // Used for MQTT topics

#define PIN_TSENSR 0 // Pin connected to DS18B20 temperature sensors
#define PIN_IR_LED 2 // Pin connected to IR-diode

const char* gConfigFile = "/config.json";

const char *    Globals::appVersion     = "1.96";
bool            Globals::mqttEnabled    = true;


WiFiManager wifiManager;                                    // WiFi Manager
WiFiClient client;                                          // WiFi Client
Adafruit_MQTT_Client mqtt(&client, MQTT_HOST, MQTT_PORT);   // MQTT client


// Setup MQTT topics
String set_topic    = String() + "mqtt2coolix/" + DEVICE_ID + "/set";
String cmd_topic    = String() + "mqtt2coolix/" + DEVICE_ID + "/cmd";
String output_topic = String() + "mqtt2coolix/" + DEVICE_ID;
Adafruit_MQTT_Subscribe mqtt_sub_set        = Adafruit_MQTT_Subscribe   (&mqtt, set_topic.c_str(), MQTT_QOS_1);
Adafruit_MQTT_Subscribe mqtt_sub_cmd        = Adafruit_MQTT_Subscribe   (&mqtt, cmd_topic.c_str(), MQTT_QOS_1);
Adafruit_MQTT_Publish   mqtt_publish        = Adafruit_MQTT_Publish     (&mqtt, output_topic.c_str());


EasyButton          button(0);                // 0 - Flash button
IRCoolixAC          ac(PIN_IR_LED);           // Air conditioner
WebService          webService(&wifiManager); // HTTP service
TemperatureService  temperatureService;       // Temperature measurement service
ChangesDetector<10> changesDetector;


long    lastPublishTime             = 0;
long    publishInterval             = 60*1000;


void publishState()
{
    Serial.println("publishState()");

    String jsonStr1 = "";

    jsonStr1 += "{";
    jsonStr1 += "\"temp\": " + String(ac.getTemp()) + ", ";
    jsonStr1 += "\"mode\": " + String(ac.getMode()) + ", ";
    jsonStr1 += "\"fan\": " + String(ac.getFan()) + ", ";
    jsonStr1 += "\"power\": " + String(ac.getPower()) + ", ";
    //jsonStr1 += "\"memory\": " + String(system_get_free_heap_size()) + ", ";
    jsonStr1 += "\"uptime\": " + String(millis() / 1000) + ", ";
    jsonStr1 += "\"temp_out\": " + String(TemperatureService::instance->getTemperatureByAddress(TemperatureService::ADDRESS_OUT)) + ", ";
    jsonStr1 += "\"temp_in\": " + String(TemperatureService::instance->getTemperatureByAddress(TemperatureService::ADDRESS_IN)) + ", ";
    jsonStr1 += "\"version\": \"" + String(Globals::appVersion) + "\"";
    jsonStr1 += "}";

    // Ensure mqtt connection
    MQTT_connect(&mqtt);

    // Publish state to output topic
    mqtt_publish.publish(jsonStr1.c_str());

    // Remember publish time
    lastPublishTime = millis();

    // Remember published temperatures (to detect changes)
    changesDetector.remember();

    Serial.print("MQTT published: ");
    Serial.println(jsonStr1);
}





void setup()
{
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

    String apName = String("esp-") + DEVICE_ID + "-v" + Globals::appVersion + "-" + ESP.getChipId();
    apName.replace('.', '_');
    WiFi.hostname(apName);

    wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
    //wifiManager.setConnectTimeout(20);
    wifiManager.setConfigPortalTimeout(60);
    wifiManager.autoConnect(apName.c_str(), "12341234"); // IMPORTANT! Blocks execution. Waits until connected

    // restart if not connected

    if (WiFi.status() != WL_CONNECTED)
    {
        ESP.restart();
    }

    Serial.print("\nConnected to ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());


    // Setup MQTT subscription for the 'set' topic.
    mqtt.subscribe(&mqtt_sub_set);

    // Send IR impulses on received MQTT message in the 'set' topic
    mqtt_sub_set.setCallback([](char *str, uint16_t len){

        char buf [len + 1];
        buf[len] = 0;
        strncpy(buf, str, len);

        Serial.println(String("Got mqtt SET message: ") + buf);


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

        // Saving state to persistent storage (because state will be restored on reboot)

        DynamicJsonDocument json(JSON_SIZE);
        json["temp"]   = ac.getTemp();
        json["mode"]   = ac.getMode();
        json["fan"]    = ac.getFan();
        json["power"]  = ac.getPower();
        saveConfig(gConfigFile, json);

    });

    // Setup MQTT subscription for the 'cmd' topic.
    mqtt.subscribe(&mqtt_sub_cmd);

    // Execute commands
    mqtt_sub_cmd.setCallback([](char *str, uint16_t len){

        char buf [len + 1];
        buf[len] = 0;
        strncpy(buf, str, len);
        
        Serial.println(String("Got mqtt CMD message: ") + buf);

        if(String("restart").equals(buf)){
            ESP.restart();
        }
        if(String("swing").equals(buf)){
            ac.setSwing();
            ac.send(1);
        }
        if(String("turbo").equals(buf)){
            ac.setTurbo();
            ac.send(1);
        }
    });

    // Onboard button pressed (for testing purposes)
    button.onPressed([](){
        Serial.println("Flash button pressed!");
        Serial.println(ac.toString());
        ac.send(1);         // send IR
        publishState();     // publish to mqtt
    });

    webService.init();
    //temperatureService.init(D5);
    temperatureService.init(PIN_TSENSR);

    // Provide values to ChangesDetecter
    changesDetector.setGetValuesCallback([](float buf[]){
        buf[0] = temperatureService.getTemperature(0);
        buf[1] = temperatureService.getTemperature(1);
    });

    // Publish state on changes detected
    changesDetector.setChangesDetectedCallback([](){
        publishState();
    });

    ArduinoOTA.begin();

}




void loop()
{
    if (!WiFi.isConnected())
    {
        Serial.println("No WiFi connection.. wait for 3 sec and skip loop");
        delay(3000);
        return;
    }

    ArduinoOTA.handle();
    button.read();
    webService.loop();
    temperatureService.loop();
    changesDetector.loop();

    // Somtimes it is required to disable MQTT listening without switching off the device 
    if(Globals::mqttEnabled)
    {
        // Ensure the connection to the MQTT server is alive (this will make the first
        // connection and automatically reconnect when disconnected).  See the MQTT_connect()
        MQTT_connect(&mqtt);
        
        
        // wait X milliseconds for subscription messages
        mqtt.processPackets(10);

        
        // publish state every publishInterval milliseconds
        if((!lastPublishTime && TemperatureService::instance->ready) || millis() > lastPublishTime + publishInterval)
        {
            // do the publish
            publishState();
        }
    }


    // WARNING! .ping() causes error "dropped a packet". Disabled. Waiting for library update
    // keep the connection alive
    // if(! mqtt.ping()) {
    //     mqtt.disconnect();
    // }


}

