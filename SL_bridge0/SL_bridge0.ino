// SL_bidge0
// -*- mode: C++ -*-
// first run a a bridge

// Mesh has much greater memory requirements, and you may need to limit the
// max message length to prevent wierd crashes
// Since our ARM platform has nor immediate memory constraint, we'll go with
// a larger message size
#define RH_MESH_MAX_MESSAGE_LEN 250

#define VER "3"

#include <RHMesh.h>
#include <RH_RF95.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#include "SL_Credentials.h"
#include "SL_RingBuff.h"

#define USE_SSL 1

#define  MIN(a,b) (((a)<(b))?(a):(b))
#define  MAX(a,b) (((a)>(b))?(a):(b))


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

// minimum transmisison gap (ms)
#define MINTXGAP 125

// sizes of queues
#define LORAQSIZE 50	// probably more than available memory
#define MQTTQSIZE 150

// Initial frequency for the bridge, can be changed via .../config/freq topic
#define RF95_FREQ 915.0

// Inital Lora address of the bridge, can be changes via .../config/addr topic
#define RF95_ADDRESS 4

// Inital Lora tx power for the bridge, can be changes via .../config/power topic
#define RF95_POWER 23

// WiFi Creds are in SL_Credentials.h

#if USE_SSL
WiFiClientSecure client;
#else
WiFiClient client;
#endif

// config variables, matched to config topics
uint8_t rf95addr = RF95_ADDRESS;
double  rf95freq = RF95_FREQ;
uint8_t rf95power = RF95_POWER;
boolean slinitdone = false;

// stats counters
long rf95sent, rf95received = 0;
long mqttsent, mqttreceived = 0;

// timers
int beforeTime = 0, afterTime = 0, nextSendTime = 0;
long nextretry = 0;

int hwm = MQTTQSIZE - 2;

// queues
SL_RingBuff mqttQ(MQTTQSIZE);
SL_RingBuff loraQ(LORAQSIZE);

// MQTT client class
Adafruit_MQTT_Client mqtt(&client, SL_SERVER, SL_SERVERPORT, SL_CONID,  SL_USERNAME, SL_KEY);

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
Adafruit_MQTT_Subscribe slconf_state = Adafruit_MQTT_Subscribe(&mqtt, \
      SL_USERNAME "/" SL_CONID  "/state");


// Singleton instance of the radio driver
RH_RF95 driver(RFM95_CS, RFM95_INT);

// Class to manage message delivery and receipt, using the driver declared above
RHMesh manager(driver, RF95_ADDRESS);


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


void *allocmem(size_t size) {

  void *ret;
  ret = malloc(size);
  if (ret == 0) {
	Serial.println("out of memory!!");
	while(1);
  }
  return ret;
}


//
// SETUP
//
void setup()
{
  int8_t cnt;
  Serial.begin(115200);
  delay(200);
  Serial.println(F("!ID SL_bridge" VER));

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

  WiFi.begin(WLAN_SSID, WLAN_PASS);

  cnt = 40;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (cnt-- == 0) {
      Serial.println(F("\ndie, wait for reset"));
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
  slconf_addr.setCallback(&CBsetrf95addr);
  slconf_freq.setCallback(&CBsetrf95freq);
  slconf_power.setCallback(&CBsetrf95power);
  slconf_node.setCallback(&CBsendmsgtonode);
  slconf_state.setCallback(&CBupdatestate);

  mqtt.subscribe(&slconf_addr);
  mqtt.subscribe(&slconf_freq);
  mqtt.subscribe(&slconf_power);
  mqtt.subscribe(&slconf_node);
  mqtt.subscribe(&slconf_state);

  // set lwt
  mqtt.will(SL_USERNAME "/" SL_CONID "/status", "OFFLINE", 0, 1);

  // we still must init RH
  slinitdone = false;
}


uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];

//
// LOOP
//
void loop()
{
  uint8_t len = sizeof(buf);
  uint8_t from;
  int8_t rssi;
  boolean ret;
  char *msg;


  SLConnect();
  // read and queue all waiting pkts
  // don't accept any more if mqttQ is almost full
  while (driver.available() && (mqttQ.queuelevel() < hwm)) {

#ifdef LORA_LED
    digitalWrite(LORA_LED, LOW);
#endif
    beforeTime = millis();
    ret = manager.recvfromAck(buf, &len, &from);
    afterTime = millis() - beforeTime;
    if (ret) {
      rf95received += 1;
      rssi = driver.lastRssi();

      msg = (char *) allocmem(len+10);
      snprintf(msg, len+9, "%3i,%4i|%s", from, rssi, buf);
      if (mqttQ.enqueue(msg) == 0) {
		Serial.println("mqttQ FULL, pkt dropped");
		hwm = MAX(hwm - 1, 2);		// decrease high water mark
      }
      else {
        Serial.printf("from %i RSSI %i data: \"%s\" time: %li \n", from, rssi, (char*)buf, afterTime);
      }
    }
#ifdef LORA_LED
    digitalWrite(LORA_LED, HIGH);
#endif
  }

  if ((mqttQ.queuelevel())&& (mqtt.connected())) {
#ifdef MQTT_LED
      digitalWrite(MQTT_LED, LOW);
#endif
    msg = mqttQ.dequeue();
    ret = slpublish.publish(msg);
    if (!ret) {
      Serial.println(F("mqtt publish failed"));
    }
	else {
	  mqttsent += 1;
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


void UpdStatus(char *newstatus)
{
  char buf[40];

  snprintf(buf, sizeof(buf), "%s/%li/%li/%li/%li/%i/%i", \
		newstatus, rf95sent, rf95received, mqttsent, mqttreceived, \
		mqttQ.queuelevel(), loraQ.queuelevel());
  while (!slstatus.publish(buf)) {
      mqtt.processPackets(250);
      Serial.println(F("status update failed: "));
      delay(500);
  }
  Serial.print(F("status update: "));
  Serial.println(buf);
}

//
// SLConnect:   establish connection to MQTT server and setup RH Mesh
void SLConnect()
{

  MQTT_connect();
  // prevent lockout of mqtt
  mqtt.processPackets(10);
  if (slinitdone)
    return;

  RH_connect();
  slinitdone = true;
}


// initialize and connect RadioHead
void RH_connect()
{

  if (!manager.init())
    Serial.println(F("RHMesh init failed"));
  setrf95addr(rf95addr);
  setrf95freq(rf95freq);
  setrf95power(rf95power);
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect()
{
  int8_t ret;

  // done if already connected.
  if (slinitdone && mqtt.connected()) {
	nextretry = 0;
    return;
  }
  if (nextretry >= millis())
	return;

  Serial.print(F("MQTT connect to " SL_SERVER " at "));
  Serial.print(millis());
  Serial.print(" port ");
  Serial.print(SL_SERVERPORT);
  Serial.print(": ");

  if ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.print(mqtt.connectErrorString(ret));
    Serial.println(F("! Retry in 2 sec."));
    nextretry = millis() + 2000;
    mqtt.disconnect();
  }
  else {
	Serial.println(" OK");
    UpdStatus("Online");
    nextretry = 0;
  }
}


//
// CallBacks
//
void CBsetrf95addr(uint32_t addr) {
  mqttreceived += 1;
  setrf95addr(addr);
}

void CBsetrf95freq(double freq) {
  mqttreceived += 1;
  setrf95freq(freq);
}

void CBsetrf95power(uint32_t power) {
  mqttreceived += 1;
  setrf95power(power);
}

void CBsendmsgtonode(char *nodedata, uint16_t len) {
  mqttreceived += 1;
  sendmsgtonode(nodedata, len);
}

void CBupdatestate(char *cmd, uint16_t len) {
  mqttreceived += 1;
  updatestate(cmd, len);
}



// Utility
void setrf95addr(uint32_t addr) {
  rf95addr = addr;
  Serial.print(F("RF95 addr: "));
  Serial.println(addr);
  manager.setThisAddress(rf95addr);
}

void setrf95freq(double freq) {
  rf95freq = freq;
  Serial.print(F("RF95 freq: "));
  Serial.println(freq);
  if (!driver.setFrequency(rf95freq)) {
    Serial.println(F("setFrequency failed"));
    while (1);
  }
}

void setrf95power(uint32_t power) {
  rf95power = power;
  Serial.print(F("RF95 tx power: "));
  Serial.println(power);
  driver.setTxPower(rf95power, false);
}

struct lorapkt {
  uint8_t toaddr;
  uint16_t len;
  char *pkt;
};

void sendmsgtonode(char *nodedata, uint16_t len) {
  char   *strtokIndx;
  struct lorapkt *pkt;
  int    plen;

  Serial.print(F("config node, len "));
  Serial.print(len);
  Serial.print(" data: ");
  Serial.println(nodedata);

  pkt = (lorapkt *)allocmem(sizeof(struct lorapkt));
  strtokIndx = strtok(nodedata,",");
  pkt->toaddr = atoi(strtokIndx);
  strtokIndx = strtok(NULL, ",");
  plen = atoi(strtokIndx);
  pkt->len = MIN(len - 1, plen);
  strtokIndx = strtok(NULL, ",");
  pkt->pkt = (char *)allocmem(pkt->len+1);
  memcpy(pkt->pkt, strtokIndx, pkt->len);
  pkt->pkt[pkt->len] = '\0';
  loraQ.enqueue((char *)pkt);
}

void sendlora() {
  struct lorapkt *pkt;

  if (millis() < nextSendTime) 
    return;
  pkt = (struct lorapkt *)loraQ.dequeue();

  Serial.print(F("send lora addr: "));
  Serial.print(pkt->toaddr);
  Serial.print(" data: ");
  Serial.println(pkt->pkt);
#ifdef LORA_LED
  digitalWrite(LORA_LED, LOW);
#endif
  beforeTime = millis();
  if (manager.sendtoWait((uint8_t *)pkt->pkt, pkt->len, pkt->toaddr) == RH_ROUTER_ERROR_NONE) {
    afterTime = millis() - beforeTime;
    Serial.print("sent OK, ");
	Serial.println(afterTime);
    rf95sent += 1;
  }
  else {
    Serial.println("send failed");
  }
  nextSendTime = millis() + MINTXGAP;
#ifdef LORA_LED
  digitalWrite(LORA_LED, HIGH);
#endif
  free(pkt->pkt);
  free(pkt);
}


void updatestate(char *cmd, uint16_t len) {

  if (cmd && (cmd[0] == 'r')) {
    UpdStatus("Resetting");
    slinitdone = false;
    mqtt.disconnect();
  } else  {
    UpdStatus("Online");
  }
}
