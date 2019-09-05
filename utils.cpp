
#include <Arduino.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <Adafruit_MQTT_Client.h>


String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".gz")) return "application/x-gzip";
    return "text/plain";
}


// Convert DeviceAddress object to String
//
// Returns:
//      A String containing device address.
String getAddressToString(DeviceAddress deviceAddress) {
    String str = "";
    for (uint8_t i = 0; i < 8; i++) {
        if ( deviceAddress[i] < 16 ) str += String(0, HEX);
        str += String(deviceAddress[i], HEX);
    }
    return str;
}


// Forcibly mount the SPIFFS. Formatting the SPIFFS if needed.
//
// Returns:
//   A boolean indicating success or failure.
bool mountSpiffs(void)
{
    Serial.println("Mounting SPIFFS...");
    if (SPIFFS.begin())
        return true; // We mounted it okay.
    // We failed the first time.
    Serial.println("Failed to mount SPIFFS!\nFormatting SPIFFS and trying again...");
    SPIFFS.format();
    if (!SPIFFS.begin()){ // Did we fail?
        Serial.println("DANGER: Failed to mount SPIFFS even after formatting!");
        delay(1000); // Make sure the debug message doesn't just float by.
        return false;
    }
    return true; // Success!
}

/**
 * Returns contents of the file or empty string on fail.
 */
String fileGetContents(const char * filename)
{
    String contents = "";
    if (mountSpiffs()) {
        if (SPIFFS.exists(filename)) {

            File theFile = SPIFFS.open(filename, "r");
            if (theFile) {
                Serial.println("Opened config file");
                size_t size = theFile.size();
                // Allocate a buffer to store contents of the file.
                char buf[size+1];
                buf[size] = 0;

                theFile.readBytes(buf, size);

                contents = buf;

                theFile.close();
            }
        } else {
            Serial.println(String("File '") + filename + "' doesn't exist!");
        }
        Serial.println("Unmounting SPIFFS.");
        SPIFFS.end();
    }
    return contents;
}


bool loadConfig(const char * filename, std::function<void(DynamicJsonDocument)> onLoadCallback)
{
    bool success = false;

    String text = fileGetContents(filename);

    if(text == "") 
        return false;

    const int JSON_SIZE = 1024;

    DynamicJsonDocument json(JSON_SIZE);
    DeserializationError error = deserializeJson(json, text);
    if (!error) {
        Serial.println("Json config file parsed ok.");

        // run onLoadCallback
        onLoadCallback(json);
        
        return true;
    } else {
        Serial.println("Failed to load json config");
        return false;
    }    

}



bool saveConfig(const char * filename, DynamicJsonDocument json) 
{
    Serial.println("Saving the config.");
    bool success = false;

    if (mountSpiffs()) {
        File configFile = SPIFFS.open(filename, "w");
        if (!configFile) {
            Serial.println("Failed to open config file for writing.");
        } else {
            Serial.println("Writing out the config file.");
            serializeJson(json, configFile);
            configFile.close();
            Serial.println("Finished writing config file.");
            success = true;
        }
        SPIFFS.end();
    }
    return success;
}

bool saveConfig(const char * filename, String jsonStr) 
{
    bool success = false;

    if (mountSpiffs()) {
        File configFile = SPIFFS.open(filename, "w");
        if (!configFile) {
            Serial.println("Failed to open config file for writing.");
        } else {
            configFile.write(jsonStr.c_str());
            configFile.close();
            success = true;
        }
        SPIFFS.end();
    }
    return success;
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect(Adafruit_MQTT_Client * mqtt) {
    int8_t ret;

    // Stop if already connected.
    if (mqtt->connected()) {
        return;
    }

    Serial.print("Connecting to MQTT... ");

    uint8_t retries = 5;
    while ((ret = mqtt->connect()) != 0) { // connect will return 0 for connected
        Serial.println(mqtt->connectErrorString(ret));
        Serial.println("Retrying MQTT connection in X seconds...");
        mqtt->disconnect();
        delay(1000);  // wait X seconds
        retries--;
        if (retries == 0) {
            return;
        }
    }
    Serial.println("MQTT Connected!");
}