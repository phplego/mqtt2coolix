#include <Arduino.h>
#include <DallasTemperature.h>


String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".gz")) return "application/x-gzip";
    return "text/plain";
}

//------------------------------------------
//Convert device id to String
String getAddressToString(DeviceAddress deviceAddress) {
    String str = "";
    for (uint8_t i = 0; i < 8; i++) {
        if ( deviceAddress[i] < 16 ) str += String(0, HEX);
        str += String(deviceAddress[i], HEX);
    }
    return str;
}