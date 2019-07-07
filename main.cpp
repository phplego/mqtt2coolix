
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <WiFiManager.h>
#include <EasyButton.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>

#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#define APP_VERSION 2

#define MQTT_HOST "192.168.1.2"  // MQTT host (e.g. m21.cloudmqtt.com)
#define MQTT_PORT 11883          // MQTT port (e.g. 18076)
#define MQTT_USER "change-me"    // Ingored if brocker allows guest connection
#define MQTT_PASS "change-me"    // Ingored if brocker allows guest connection

#define DEVICE_ID "electrolux_ac"

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).

IRsend irsend(kIrLed);      // Set the GPIO to be used to sending the message.

// Example of data captured by IRrecvDumpV2.ino
// uint16_t rawData[67] = {9000, 4500, 650, 550, 650, 1650, 600, 550, 650, 550,
//                         600, 1650, 650, 550, 600, 1650, 650, 1650, 650, 1650,
//                         600, 550, 650, 1650, 650, 1650, 650, 550, 600, 1650,
//                         650, 1650, 650, 550, 650, 550, 650, 1650, 650, 550,
//                         650, 550, 650, 550, 600, 550, 650, 550, 650, 550,
//                         650, 1650, 600, 550, 650, 1650, 650, 1650, 650, 1650,
//                         650, 1650, 650, 1650, 650, 1650, 600};

//uint64_t data = 0xB2BFA0;

WiFiManager wifiManager;

WiFiClient client;

// MQTT client
Adafruit_MQTT_Client mqtt(&client, MQTT_HOST, MQTT_PORT);


// Setup MQTT topics
String input_topic  = String() + "mqtt2coolix/" + DEVICE_ID + "/set";
String output_topic = String() + "mqtt2coolix/" + DEVICE_ID;
Adafruit_MQTT_Subscribe mqtt_subscribe      = Adafruit_MQTT_Subscribe   (&mqtt, input_topic.c_str(), MQTT_QOS_1);
Adafruit_MQTT_Publish   mqtt_publish        = Adafruit_MQTT_Publish     (&mqtt, output_topic.c_str());

// Air conditioner
IRCoolixAC ac(kIrLed);

EasyButton button(0); // 0 - Flash button

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in X seconds...");
       mqtt.disconnect();
       delay(1000);  // wait X seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}

void publishState()
{
    StaticJsonBuffer<1024> jsonBuffer;
    JsonObject& stateJson = jsonBuffer.createObject();
    
    stateJson["temp"]   = ac.getTemp();
    stateJson["mode"]   = ac.getMode();
    stateJson["fan"]    = ac.getFan();
    stateJson["power"]  = ac.getPower();
    stateJson["memory"] = system_get_free_heap_size();
    stateJson["uptime"] = millis() / 1000;
    stateJson["version"]    = APP_VERSION;

    char jsonStr[jsonBuffer.size()];
    stateJson.prettyPrintTo(jsonStr, jsonBuffer.size());

    // Publish state to output topic
    mqtt_publish.publish(jsonStr);
    Serial.print("MQTT published: ");
    Serial.println(jsonStr);
}

// Forcibly mount the SPIFFS. Formatting the SPIFFS if needed.
//
// Returns:
//   A boolean indicating success or failure.
bool mountSpiffs(void) {
  Serial.println("Mounting SPIFFS...");
  if (SPIFFS.begin()) return true;  // We mounted it okay.
  // We failed the first time.
  Serial.println("Failed to mount SPIFFS!\nFormatting SPIFFS and trying again...");
  SPIFFS.format();
  if (!SPIFFS.begin()) {  // Did we fail?
    Serial.println("DANGER: Failed to mount SPIFFS even after formatting!");
    delay(1000);  // Make sure the debug message doesn't just float by.
    return false;
  }
  return true;  // Success!
}

const char* gConfigFile = "/config.json";

bool saveConfig(void) {
    Serial.println("Saving the config.");
    bool success = false;
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["temp"]   = ac.getTemp();
    json["mode"]   = ac.getMode();
    json["fan"]    = ac.getFan();
    json["power"]  = ac.getPower();

    if (mountSpiffs()) {
        File configFile = SPIFFS.open(gConfigFile, "w");
        if (!configFile) {
            Serial.println("Failed to open config file for writing.");
        } else {
            Serial.println("Writing out the config file.");
            json.printTo(configFile);
            configFile.close();
            Serial.println("Finished writing config file.");
            success = true;
        }
        SPIFFS.end();
    }
    return success;
}

bool loadConfigFile(void) {
    bool success = false;
    if (mountSpiffs()) {
    Serial.println("Counted the file system");
    if (SPIFFS.exists(gConfigFile)) {
        Serial.println("Config file exists");

        File configFile = SPIFFS.open(gConfigFile, "r");
        if (configFile) {
            Serial.println("Opened config file");
            size_t size = configFile.size();
            // Allocate a buffer to store contents of the file.
            std::unique_ptr<char[]> buf(new char[size]);

            configFile.readBytes(buf.get(), size);
            Serial.print("Config file content:");
            Serial.println(buf.get());

            DynamicJsonBuffer jsonBuffer;
            JsonObject& json = jsonBuffer.parseObject(buf.get());
            if (json.success()) {
                Serial.println("Json config file parsed ok.");

                ac.setTemp(json["temp"]);
                ac.setMode(json["mode"]);
                ac.setFan(json["fan"]);
                ac.setPower(json["power"]);
                
                Serial.println("AC state recovered from json.");
                success = true;
            } else {
                Serial.println("Failed to load json config");
            }
            Serial.println("Closing the config file.");
            configFile.close();
        }
    } else {
        Serial.println("Config file doesn't exist!");
    }
    Serial.println("Unmounting SPIFFS.");
    SPIFFS.end();
  }
  return success;
}


void setup()
{
    delay(10);
    irsend.begin();
    Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
    
    ac.begin();
    loadConfigFile();


    wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.autoConnect("ESP-COOLIX-1", "12341234"); // IMPORTANT! Blocks execution. Waits until connected

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

    mqtt_subscribe.setCallback([](char *str, uint16_t len){
        Serial.println(String("Got mqtt message: ") + str);

        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(str);


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
        saveConfig();

    });


    button.onPressed([](){
        Serial.println("Flash button pressed!");
        Serial.println(ac.toString());
        ac.send(1);         // send IR
        publishState();     // publish to mqtt
    });
}


long    lastPublishTime = 0;
long    publishInterval = 60*1000;

void loop()
{
    button.read();

    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect
    // function definition further below.
    MQTT_connect();
    
    
    // wait X milliseconds for subscription messages
    mqtt.processPackets(10);

    
    // publish state every X seconds
    if(millis() > lastPublishTime + publishInterval){
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





