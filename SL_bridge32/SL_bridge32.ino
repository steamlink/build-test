// SL_bridge32
// -*- mode: C++ -*-
// bridge on ESP32 for a SteamLink base LoRa network
// https://steamlink.net/

#define VER "11"

#include <SteamLink.h>
#include <SteamLinkESP.h>
#include <SteamLinkLora.h>
#include <SteamLinkBridge.h>

#include "SL_Credentials.h"

#define SL_ID_ESP 0x121
#define SL_ID_LORA 0x120

struct SL_NodeCfgStruct ESPconfig = {
	SL_ID_ESP, 
	"ESP_289",
	"ESP32 Side of Bridge", 
	43.43,
	-79.23,
	180,
	60,	// max_silence
	0,  // sleeps
	1,	// pingable
	0, // battery powered
	0  // radio params
};

struct SL_NodeCfgStruct LoRaconfig = {
	SL_ID_LORA, 
	"LoRa_288",
	"LoRa side of bridge", 
	43.43,
	-79.23,
	180,
	60,	// max_silence
	0,	// sleeps
	1,	// pingable
	0,
	0
};


#if 0
// for ESP32 "heltec_wifi_lora_32"
#define RFM95_CS 18		// GPIO18
#define RFM95_RST 14
#define RFM95_INT 26
#define OLED 1
//#define SPI_MISO 19	
//#define SPI_MOSI 27
//#define SPI_SCK 5
#endif
#if 0
// for Feather M0
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
#endif

#if 1
// for Adafruit Huzzah breakout
#define RFM95_CS 15
#define RFM95_RST 4
#define RFM95_INT 5
// Blue LED to flash on LoRa traffic
#define LORA_LED 2
// Red LED to flash on mqtt
#define MQTT_LED 0
#endif


#ifdef OLED
#include <SSD1306.h>
SSD1306 display(0x3c, GPIO_NUM_4, GPIO_NUM_15);
#endif


void esp_on_receive(uint8_t* buffer, uint8_t size);
void lora_on_receive(uint8_t* buffer, uint8_t size);

SteamLinkESP slesp(&ESPconfig);
SteamLinkLora sllora(&LoRaconfig);
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
  Serial.print(F("!ID SL_bridge32 " VER " slid_ESP: " ));
  Serial.print(SL_ID_ESP, HEX);
  Serial.print(F(" slid_LoRa: "));
  Serial.println(SL_ID_LORA, HEX);

#ifdef OLED
  Serial.println("OLED Init");
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW); // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 to high 
    // Initialising the UI will init the display too.
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  
  display.drawString(0, 0, "SteamLink Bridge");
  display.display();
#endif

  slesp.init((void *) &sl_ESP_config, sizeof(sl_ESP_config));
  slesp.register_receive_handler(esp_on_receive);
  Serial.println(F("slesp.init done"));

  sllora.set_pins(RFM95_CS, RFM95_RST, RFM95_INT);
  sllora.init((void *) &sl_Lora_config, sizeof(sl_Lora_config));
  sllora.register_receive_handler(lora_on_receive);
  Serial.println(F("sllora.init done"));

  slbridge.bridge(&sllora);
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
#ifdef OLED
    display.drawString(0, 10, "pkt received:");
    display.drawString(0, 20, (char*)buf);
    display.display();
#endif
}


void lora_on_receive(uint8_t *buf, uint8_t len) {
  Serial.print("sl_on_receive: len: ");
  Serial.print(len);
  Serial.print(" msg: ");
  Serial.println((char*)buf);
}

