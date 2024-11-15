#include <EEPROM.h>
#include <FS.h>

/*
  Upload this sketch to your ESP8266 to erase 
  - all files in the SPIFFS, 
  - all data in the EEPROM
  - WiFi credentials (SSID, password)

  Also overwrites the previous program with this one (obviously).
*/

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("STARTING...");
  
  EEPROM.begin(4096);
  Serial.println("EEPROM initialized");
  
  for (int i = 0; i < 4096; ++i){
    EEPROM.write(i,0x00);
  }

  Serial.println("EEPROM cleaned");

  SPIFFS.begin();
  Serial.println("SPIFFS initialized");

  SPIFFS.format();
  Serial.println("SPIFFS cleaned");
  
  ESP.eraseConfig();

  Serial.println("WiFi credentials erased");

  Serial.println("DONE!");

  ESP.reset();
}

void loop() {
  
}
