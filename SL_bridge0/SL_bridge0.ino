// SL_bidge0
// -*- mode: C++ -*-
// first run a a bridge

// Mesh has much greater memory requirements, and you may need to limit the
// max message length to prevent wierd crashes
#define RH_MESH_MAX_MESSAGE_LEN 50

#include <RHMesh.h>
#include <RH_RF95.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include "SL_Creadentials.h"
#include "SL_RingBuff.h"

#define USE_SSL 1

#define  MIN(a,b) (((a)<(b))?(a):(b))

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
// Blue LED to flash on lora traffic
#define LORA_LED 2
// Red LED to flash on mqtt
#define MQTT_LED 0

#endif


// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

// In this small artifical network of 4 nodes,
#define CLIENT_ADDRESS 1
#define SERVER1_ADDRESS 2
#define SERVER2_ADDRESS 3
#define SERVER3_ADDRESS 4

// WiFi Creds are in SL_Credentials.h

#if USE_SSL
WiFiClientSecure client;
#else
WiFiClient client;
#endif

// config variables, matched to config topics
uint8_t sladdr = SERVER2_ADDRESS;
double   slfreq = RF95_FREQ;
uint8_t slpower = 23;
boolean slinit = false;

SL_RingBuff mqttQ(8);
SL_RingBuff loraQ(8);

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, SL_SERVER, SL_SERVERPORT, SL_CONID,  SL_USERNAME, SL_KEY);

// io.adafruit.com SHA1 fingerprint for mqtt.steamlink.net
// const char* fingerprint = "7E 68 14 B7 6C B4 B5 2C 8C 6D A0 0E 47 73 CA 6C 82 8C F5 BD";
// SHA1 fingerprint for mqtt.steamlink.net's SSL certificate
const char* fingerprint = "E3 B9 24 8E 45 B1 D2 1B 4C EF 10 61 51 35 B2 DE 46 F1 8A 3D";

// Setup a feed called 'slpublish' for publishing.
// Notice MQTT topics for SL follow the form: <username>/<conid>/<type>
//   where type is one of data, status, config
Adafruit_MQTT_Publish slpublish = Adafruit_MQTT_Publish(&mqtt, \
      SL_USERNAME "/" SL_CONID "/data");
Adafruit_MQTT_Publish slstatus = Adafruit_MQTT_Publish(&mqtt, \
      SL_USERNAME "/" SL_CONID "/status");

// Subscribe to 'config' topics

Adafruit_MQTT_Subscribe slconf_addr = Adafruit_MQTT_Subscribe(&mqtt, \
      SL_USERNAME "/" SL_CONID  "/config/addr");
Adafruit_MQTT_Subscribe slconf_freq = Adafruit_MQTT_Subscribe(&mqtt, \
      SL_USERNAME "/" SL_CONID  "/config/freq");
Adafruit_MQTT_Subscribe slconf_power = Adafruit_MQTT_Subscribe(&mqtt, \
      SL_USERNAME "/" SL_CONID  "/config/power");
Adafruit_MQTT_Subscribe slconf_node = Adafruit_MQTT_Subscribe(&mqtt, \
      SL_USERNAME "/" SL_CONID  "/config/node");


// Singleton instance of the radio driver
RH_RF95 driver(RFM95_CS, RFM95_INT);

// Class to manage message delivery and receipt, using the driver declared above
RHMesh manager(driver, SERVER3_ADDRESS);

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
// void MQTT_connect();
// void verifyFingerprint();

#if USE_SSL
void verifyFingerprint() {

  const char* host = SL_SERVER;

  Serial.print(F("verifying SSL cert for "));
  Serial.println(host);

  if (! client.connect(host, SL_SERVERPORT)) {
    Serial.println(F("Connection failed. Halting execution."));
    while(1);
  }

  if (client.verify(fingerprint, host)) {
    Serial.println(F("Connection secure."));
  } else {
    Serial.println(F("Connection insecure! Halting execution."));
    while(1);
  }
}
#endif


//
// SETUP
//
void setup() 
{
  int8_t cnt;
  Serial.begin(115200);
  delay(200);
  Serial.println(F("!ID SL_bridge0"));
  
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

#ifdef LORA_LED
  pinMode(LORA_LED, OUTPUT);
  digitalWrite(LORA_LED, HIGH);
#endif
#ifdef MQTT_LED
  pinMode(MQTT_LED, OUTPUT);
  digitalWrite(MQTT_LED, HIGH);
#endif

  mqtt.disconnect();
  
  delay(1000);

  WiFi.begin(WLAN_SSID, WLAN_PASS);

  cnt = 40;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (cnt-- == 0) {
      Serial.println(F("\nwait for reset"));
      while (1);
    }
  }
  Serial.println("");
  
  Serial.print(F("WiFi connected, IP address: "));
  Serial.println(WiFi.localIP());

#if USE_SSL
// check the fingerprint of mqtt.steamlink.net's SSL cert
  verifyFingerprint();
#endif

  // set callbacks and subscribe to the config topics
  slconf_addr.setCallback(&CBsladdr);
  slconf_freq.setCallback(&CBslfreq);
  slconf_power.setCallback(&CBslpower);
  slconf_node.setCallback(&CBslnode);

  
  mqtt.subscribe(&slconf_addr);
  mqtt.subscribe(&slconf_freq);
  mqtt.subscribe(&slconf_power);
  mqtt.subscribe(&slconf_node);

// early connect, to receive config data
  slinit = false;
}


struct {
  char preamb[9];
  uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];
} pkt;

//
// LOOP
//
void loop()
{
  uint8_t len = sizeof(pkt.buf);
  uint8_t from;
  int8_t rssi;
  boolean ret;
  char *msg;

  
  SLConnect();
  // read and queue all waiting pkts
  while (driver.available()) {
#ifdef LORA_LED
    digitalWrite(LORA_LED, LOW);
#endif
    ret = manager.recvfromAck(pkt.buf, &len, &from);
    if (ret) {
      rssi = driver.lastRssi();
      Serial.printf("rqadd %i RSSI %i data: %s ->", from, rssi, (char*)pkt.buf);
      // build mqtt preable:  fromaddr, rssi|

      snprintf(pkt.preamb, sizeof(pkt.preamb), "%3i,%4i", from, rssi);
      pkt.preamb[sizeof(pkt.preamb)-1]='|';
      msg = (char *) malloc(len+1);
      memcpy(msg, &pkt, len+sizeof(pkt.preamb));
      mqttQ.enqueue(msg);
    }
#ifdef LORA_LED
    digitalWrite(LORA_LED, HIGH);
#endif    
  }
  
  if (mqttQ.queuelevel()) {
#ifdef MQTT_LED
      digitalWrite(MQTT_LED, LOW);
#endif
    msg = mqttQ.dequeue();
    ret = slpublish.publish(msg);
    if (!ret) {
      Serial.println(F(" Failed"));
    } else {
      Serial.println(F(" OK!")); 
    }
    free(msg);
#ifdef MQTT_LED
    digitalWrite(MQTT_LED, HIGH);
#endif
  }
  
  if (loraQ.queuelevel()) {
    sendlora();
  }
}


//
// SLConnect:   establish connection to MQTT server and setup RH Mesh
void SLConnect()
{

  MQTT_connect();
  mqtt.processPackets(250);
  if (slinit)
    return;

  while (!slstatus.publish("Online")) {
      mqtt.processPackets(250);
      Serial.println(F("status update failed: "));
      delay(500);
  }
  Serial.println(F("status update OK!"));
  
  Serial.print(F("RHMesh bridge at addr "));
  Serial.println(sladdr);
  if (!manager.init())
    Serial.println(F("RHMesh init failed"));
    
  // Defaults after init are 434.0MHz, 0.05MHz AFC pull-in, modulation FSK_Rb2_4Fd36
  Serial.print(F("setFrequency "));
  Serial.println(slfreq);
  if (!driver.setFrequency(slfreq)) {
    Serial.println(F("setFrequency failed"));
    while (1);
  }
  Serial.print(F("setTxPower: "));
  Serial.println(slpower);
  driver.setTxPower(slpower, false); 
  slinit = true;
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;
  
  // Stop if already connected.
  if (slinit && mqtt.connected()) {
    return;
  }
  Serial.print(F("MQTT connect to " SL_SERVER " at "));
  Serial.print(SL_SERVERPORT);

  uint8_t retries = 10;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println(F("failed! Retry in 5 sec."));
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
          Serial.println(F("retry limit! Wait for watchdog"));
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println(" OK");
}



//
// CallBacks
//
void CBsladdr(uint32_t addr) {
  sladdr = addr;
  Serial.print(F("config new addr: "));
  Serial.println(addr);
}

void CBslfreq(double freq) {
  slfreq = freq;
  Serial.print(F("config new freq: "));
  Serial.println(freq); 
  if (!driver.setFrequency(slfreq)) {
    Serial.println(F("setFrequency failed"));
    while (1);
  }
}

void CBslpower(uint32_t power) {
  slpower = power;
  Serial.print(F("config new power: "));
  Serial.println(power);
  driver.setTxPower(slpower, false); 
}

struct lorapkt {
  uint8_t toaddr;
  uint16_t len;
  char *pkt;
};

void CBslnode(char *nodedata, uint16_t len) {
  char   *strtokIndx;
  struct lorapkt *pkt;
  int    plen;

  Serial.print(F("config node, len "));
  Serial.print(len);
  Serial.print(" data: ");
  Serial.println(nodedata);
  
  pkt = (lorapkt *)malloc(sizeof(struct lorapkt));
  strtokIndx = strtok(nodedata,",");  
  pkt->toaddr = atoi(strtokIndx);
  strtokIndx = strtok(NULL, ",");
  plen = atoi(strtokIndx);
  pkt->len = MIN(len - 1, plen);
  strtokIndx = strtok(NULL, ",");
  pkt->pkt = (char *)malloc(pkt->len+1);
  memcpy(pkt->pkt, strtokIndx, pkt->len);
  pkt->pkt[pkt->len] = '\0';
  loraQ.enqueue((char *)pkt);
}

void sendlora() {
  struct lorapkt *pkt = (struct lorapkt *)loraQ.dequeue();
  
  Serial.print(F("send lora addr: "));
  Serial.print(pkt->toaddr);
  Serial.print(" data: ");
  Serial.println(pkt->pkt);
  if (manager.sendtoWait((uint8_t *)pkt->pkt, pkt->len, pkt->toaddr) == RH_ROUTER_ERROR_NONE) {
    Serial.println("sent OK");
  }
  else {
    Serial.println("send failed");
  } 
  free(pkt->pkt);
  free(pkt);
}


