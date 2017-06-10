// SL_bidge0
// -*- mode: C++ -*-
// bridge for a SteamLink base LoRa network
// https://steamlink.net/

#define VER "9"

#include <SteamLink.h>
#include <SteamLinkESP.h>
#include <SteamLinkLora.h>
#include <SteamLinkBridge.h>

#include "SL_Credentials.h"

#define SL_ID_ESP 0x111
#define SL_ID_LORA 0x110


#if 0
// for Feather M0
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
#else
// for Adafruit Huzzah breakout
#define RFM95_CS 15
#define RFM95_RST 4
#define RFM95_INT 5
// Blue LED to flash on LoRa traffic
#define LORA_LED 2
// Red LED to flash on mqtt
#define MQTT_LED 0
#endif


void esp_on_receive(uint8_t* buffer, uint8_t size);
void lora_on_receive(uint8_t* buffer, uint8_t size);

SteamLinkESP slesp(SL_ID_ESP);
SteamLinkLora sllora(SL_ID_LORA);
SteamLinkBridge slbridge(&slesp);

/* Packet building */
uint8_t data[100];
int packet_num = 0;

// button state
int8_t bLast, bCurrent = 2;

//
// SETUP
//
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("!ID SL_bridge0 " VER));

  slbridge.bridge(&sllora);
  
  sllora.set_pins(RFM95_CS, RFM95_RST, RFM95_INT);
  sllora.init((void *) &sl_Lora_config);
  Serial.println(F("sllora.init done"));
  slesp.init((void *) &sl_ESP_config);
  Serial.println(F("slesp.init done" VER));
  
  sllora.register_receive_handler(lora_on_receive);
  Serial.println(F("sllora.register_receive_handler done"));
  slesp.register_receive_handler(esp_on_receive);
  Serial.println(F("sslesp.register_receive_handler done"));

}


int getBatInfo() {
#ifdef VBATPIN
    return int(analogRead(VBATPIN) * 6.45); // = *2*3.3/1024*1000
#else
    return 0.0;
#endif
}

//
// LOOP
//

void loop() {
  slbridge.update();
  //slesp.update();
}

// Handlers

void esp_on_receive(uint8_t *buf, uint8_t len) {
    Serial.print("slesp_on_receive: len: ");
    Serial.print(len);
    Serial.print(" msg: ");
    Serial.println((char*)buf);
}


void lora_on_receive(uint8_t *buf, uint8_t len) {
  Serial.print("sl_on_receive: len: ");
  Serial.print(len);
  Serial.print(" msg: ");
  Serial.println((char*)buf);
}

