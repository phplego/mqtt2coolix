#include "WebService.h"


WebService::WebService(WiFiManager* _wifiManager)
{
    this->wifiManager   = _wifiManager;
    this->server        = new ESP8266WebServer (80);
}

void WebService::init()
{
    String menu;
    menu += "<div>";
    menu += "<a href='/'>index</a> ";
    menu += "<a href='/toggle-mqtt'>toggle-mqtt</a> ";
    menu += "<a href='/config'>config</a> ";
    menu += "<a href='/logout'>logout</a> ";
    menu += "</div><hr>";

    this->server->on("/", [this, menu](){
        String str = ""; 
        str += menu;
        str += "<pre>";
        str += String() + "  Temperature  IN: " + TemperatureService::instance->getTemperatureByAddress(TemperatureService::ADDRESS_IN) + " ºC \n";
        str += String() + "  Temperature OUT: " + TemperatureService::instance->getTemperatureByAddress(TemperatureService::ADDRESS_OUT) + " ºC \n";
        str += "\n";
        
        str += String() + "           Uptime: " + (millis() / 1000) + " \n";
        str += String() + "      FullVersion: " + ESP.getFullVersion() + " \n";
        str += String() + "      ESP Chip ID: " + ESP.getChipId() + " \n";
        str += String() + "       CpuFreqMHz: " + ESP.getCpuFreqMHz() + " \n";
        str += String() + "              VCC: " + ESP.getVcc() + " \n";
        str += String() + "         FreeHeap: " + ESP.getFreeHeap() + " \n";
        str += String() + "       SketchSize: " + ESP.getSketchSize() + " \n";
        str += String() + "  FreeSketchSpace: " + ESP.getFreeSketchSpace() + " \n";
        str += String() + "    FlashChipSize: " + ESP.getFlashChipSize() + " \n";
        str += String() + "FlashChipRealSize: " + ESP.getFlashChipRealSize() + " \n";
        str += "</pre>";
        server->send(200, "text/html; charset=utf-8", str);     
    });

    // Edit config
    this->server->on("/config", [this, menu](){

        if(this->server->method() == HTTP_POST){
            String newContent = this->server->arg("content");
            saveConfig("/config.json", newContent);
            server->sendHeader("Location", String("/config"), true);
            server->send ( 302, "text/plain", "");
            return;
        }

        loadConfig("/config.json", [this, menu](DynamicJsonDocument json){
            String content;
            serializeJson(json, content);

            String output = "";
            output += menu;
            output += "<form method='post'>";
            output += "<textarea name='content' cols=80 rows=10>";
            output += content;
            output += "</textarea>";
            output += "</br>";
            output += "<input type='submit'>";
            output += "</form>";
            server->send(200, "text/html", output);
        });
    });
    
    // Toggle MQTT listening (On/Off)
    this->server->on("/toggle-mqtt", [this, menu](){
        if(this->server->method() == HTTP_POST){
            Globals::mqttEnabled = !Globals::mqttEnabled;
            this->server->send(200, "text/html", menu + String("OK! ") + (Globals::mqttEnabled? "enabled" : "disabled"));
        }
        else{
            String output = "";
            output += menu;
            output += "MQTT is ";
            output += (Globals::mqttEnabled ? "enabled" : "disabled");
            output += "<form method='post'><button>Toggle</button></form>";
            this->server->send(400, "text/html", output);
        }
    });


    // Logout (reset wifi settings)
    this->server->on("/logout", [this, menu](){
        if(this->server->method() == HTTP_POST){
            this->server->send(200, "text/html", "OK");
            this->wifiManager->resetSettings();
            ESP.reset();
        }
        else{
            String output = "";
            output += menu;
            output += String() + "<pre>";
            output += String() + "Wifi network: " + WiFi.SSID() + " \n";
            output += String() + "        RSSI: " + WiFi.RSSI() + " \n";
            output += String() + "    hostname: " + WiFi.hostname() + " \n";
            output += String() + "</pre>";
            output += "<form method='post'><button>Forget</button></form>";
            this->server->send(400, "text/html", output);
        }
    });

    // Trying to read file (if route not found)
    this->server->onNotFound( [this](){
        // check if the file exists in the flash memory (SPIFFS), if so, send it
        if(!this->handleFileRead(this->server->uri())){                         
            // Otherwise send 404 error 
            String message = "File Not Found\n\n";
            message += "URI: ";
            message += this->server->uri();
            message += "\nMethod: ";
            message += (this->server->method() == HTTP_GET)?"GET":"POST";
            message += "\nArguments: ";
            message += this->server->args();
            message += "\n";
            for (uint8_t i=0; i < this->server->args(); i++){
                message += " " + this->server->argName(i) + ": " + this->server->arg(i) + "\n";
            }
            this->server->send(404, "text/html", message);
        }
    } );

    this->server->begin();
    Serial.println("HTTP server started at ip " + WiFi.localIP().toString() );

}

bool WebService::handleFileRead(String path)
{
    SPIFFS.begin();

    Serial.println("handleFileRead: " + path);
    if (path.endsWith("/")) path += "index.html";               // If a folder is requested, send the index file
    String contentType = getContentType(path);                  // Get the MIME type
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {     // If the file exists, either as a compressed archive, or normal
        Serial.println(String("\tFile exists: ") + path);
        if (SPIFFS.exists(pathWithGz))                          // If there's a compressed version available
            path += ".gz";                                      // Use the compressed verion
        File file = SPIFFS.open(path, "r");                     // Open the file
        size_t sent = this->server->streamFile(file, contentType);     // Send it to the client
        file.close();                                           // Close the file again
        Serial.println(String("\tSent file: ") + path);
        return true;
    }
    Serial.println(String("\tFile Not Found: ") + path);        // If the file doesn't exist, return false
    return false;

}


void WebService::loop()
{
    this->server->handleClient();
}