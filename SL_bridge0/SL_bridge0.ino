// SL_bidge0
// -*- mode: C++ -*-
// bridge for a SteamLink base LoRa network
// https://steamlink.net/

#define VER "8"

#include <SteamLink.h>
#include <SteamLinkESP.h>
#include <SteamLinkLora.h>
#include <SteamLinkBridge.h>

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

SteamLinkESP slesp(0x110);

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
  Serial.println(F("!ID SL_testclient0 " VER));

  slesp.init();
  slesp.register_receive_handler(esp_on_receive);

  Serial.println("Steamlink init done");
  bLast = 2;
}


// Dont put this on the stack:
uint8_t buf[100];
int beforeTime = 0, afterTime = 0, nextSendTime = 0;
int waitInterval = 20000;

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
void loop()
{
  uint8_t len = sizeof(buf);
  slesp.update();
}

void esp_on_receive(uint8_t *buf, uint8_t len) {
    Serial.print("slesp_on_receive: len: ");
    Serial.print(len);
    Serial.print(" msg: ");
    Serial.println((char*)buf);
}


