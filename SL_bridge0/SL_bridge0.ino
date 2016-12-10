// SL_bidge0
// -*- mode: C++ -*-
// first run a a bridge

#define VER "5"

#include <SPI.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#include <SteamLink.h>
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
// Blue LED to flash on LoRa traffic
#define LORA_LED 2
// Red LED to flash on mqtt
#define MQTT_LED 0
#endif

// minimum transmisison gap (ms)
#define MINTXGAP 0 /// was 125 TODO: remove it and related code once waitCAD is shown to work

// sizes of queues
#define LORAQSIZE 40	// probably more than available memory
#define MQTTQSIZE 100

// WiFi Creds are in SL_Credentials.h

#if USE_SSL
WiFiClientSecure client;
#else
WiFiClient client;
#endif


// n_typ_0 pkt
#define N_TYP_VER 0
struct n_typ_0 {
  uint8_t  sw_id;
  uint8_t  npayload[];
};

// b_typ_0 pkt
#define B_TYP_VER 0
struct b_typ_0 {
  unsigned  b_typ: 4;
  unsigned  n_typ: 4;
  uint8_t  node_id;
  uint8_t  rssi;
  struct n_typ_0  bpayload;
};

// loraQ entry
struct lorapkt {
  uint8_t toaddr;
  struct n_typ_0 pkt;
};


boolean slinitdone = false;

// stats counters
long slsent, slreceived = 0;
long mqttsent, mqttreceived = 0;

// timers
int nextSendTime = 0;
long nextretry = 0;

// high water mark for MQTT queue
int hwm = MQTTQSIZE - 2;

// queues
SL_RingBuff mqttQ(MQTTQSIZE);
SL_RingBuff loraQ(LORAQSIZE);

// MQTT client class
Adafruit_MQTT_Client mqtt(&client, SL_SERVER, SL_SERVERPORT, SL_CONID,  SL_USERNAME, SL_KEY);

// Setup topic 'SL/xx/data' for publishing data from nodes.
Adafruit_MQTT_Publish slpublish = Adafruit_MQTT_Publish(&mqtt, \
      "SL/" SL_MESHID "/data");
// Setup topic 'SL/xx/status' for publishing status info from this bridge.
Adafruit_MQTT_Publish slstatus = Adafruit_MQTT_Publish(&mqtt, \
      "SL/" SL_MESHID "/status");

// listen to topic SL/mesh_xx/control for pkts destined for nodes
Adafruit_MQTT_Subscribe mqtt_subs_control = Adafruit_MQTT_Subscribe(&mqtt, \
      "SL/" SL_MESHID  "/control");
// listen to topic SL/mesh_xx/state for commands to the bridge itself
Adafruit_MQTT_Subscribe mqtt_subs_state = Adafruit_MQTT_Subscribe(&mqtt, \
      "SL/" SL_MESHID  "/state");

// Inst Steamlink
SteamLink sl;


#if USE_SSL
void verifyFingerprint() {

  const char* host = SL_SERVER;

  Serial.print(F("verifying SSL cert for "));
  Serial.println(host);

  if (! client.connect(host, SL_SERVERPORT)) {
    Serial.println(F("Connection failed. Halting execution."));
    while(1);
  }

  if (client.verify(sl_server_fingerprint, host)) {
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
  Serial.println(F("WiFi Network " WLAN_SSID));

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
  mqtt_subs_state.setCallback(&CBupdatestate);
  mqtt_subs_control.setCallback(&CBupdatecontrol);

  mqtt.subscribe(&mqtt_subs_state);
  mqtt.subscribe(&mqtt_subs_control);

  // set lwt
  mqtt.will("SL/" SL_MESHID "/status", "OFFLINE", 0, 1);

  // we still must init SL
  slinitdone = false;
}


// 
void sl_on_receive(uint8_t* buffer, uint8_t size, uint8_t from)
{
  struct b_typ_0 *msg;

  if (mqttQ.queuelevel() < hwm) {
#ifdef LORA_LED
    digitalWrite(LORA_LED, LOW);
#endif
	Serial.printf("got %s from %s\n", size, from);
    slreceived += 1;

    msg = (struct b_typ_0 *) allocmem(size+sizeof(b_typ_0));
	msg->b_typ = B_TYP_VER;
	msg->n_typ = N_TYP_VER;
    msg->node_id = from;
	msg->rssi = sl.get_last_rssi();
    memcpy(&msg->bpayload, buffer, size);
    if (mqttQ.enqueue((uint8_t *)msg, size+sizeof(n_typ_0)) == 0) {
      Serial.println("mqttQ FULL, pkt dropped");
      hwm = MAX(hwm - 1, 2);		// decrease high water mark
    }
    else {
      Serial.printf("from %i RSSI %i size %i data: \"%s\"\n", from, msg->rssi, size, (char*)buffer);
    }
#ifdef LORA_LED
    digitalWrite(LORA_LED, HIGH);
#endif
  }
}

//
// LOOP
//
void loop()
{
  BridgeConnect();

  if (mqttQ.queuelevel() && mqtt.connected()) {
	mqsend();
  }

  if (loraQ.queuelevel()) {
    slsend();
  }
}


//
// retrieve a packet from the loraQ and send it 
void slsend() {
  struct lorapkt *pkt;
  uint8_t len;

  if (millis() < nextSendTime) 
    // don't send if we are in the TX wait gap
    return;
  pkt = (struct lorapkt *)loraQ.dequeue(&len);

  Serial.print(F("send lora addr: "));
  Serial.print(pkt->toaddr);
  Serial.print(" len: ");
  Serial.println(len - sizeof(struct lorapkt));
#ifdef LORA_LED
  digitalWrite(LORA_LED, LOW);
#endif

  if (sl.send((uint8_t *)&pkt->pkt, pkt->toaddr, len - sizeof(struct lorapkt))) {
    Serial.println("sent OK");
    slsent += 1;
  }
  else {
    Serial.println("send failed");
  }
  nextSendTime = millis() + MINTXGAP;
#ifdef LORA_LED
  digitalWrite(LORA_LED, HIGH);
#endif
  free(pkt);
}



// retrieve msg and mqtt publish
void mqsend() {
  boolean ret;
  uint8_t *msg;
  uint8_t len;

#ifdef MQTT_LED
   digitalWrite(MQTT_LED, LOW);
#endif
  msg = mqttQ.dequeue(&len);
  ret = slpublish.publish(msg, len);
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


void UpdStatus(char *newstatus)
{
  uint8_t buf[40];
  uint8_t i, len;

  len = snprintf((char *)buf, sizeof(buf), "%s/%li/%li/%li/%li/%i/%i", \
		newstatus, slsent, slreceived, mqttsent, mqttreceived, \
		mqttQ.queuelevel(), loraQ.queuelevel());
  len = MIN(len+1,sizeof(buf));
  i = 5;
  while (!slstatus.publish(buf, len) && i--) {
      mqtt.processPackets(250);
      Serial.println(F("status update failed: "));
      delay(500);
  }
  if (i == 0) 
	while(1);
  Serial.print(F("status update: "));
  Serial.println((char *)buf);
}

//
// BridgeConnect:   establish connection to MQTT server and setup RH Mesh
void BridgeConnect()
{

  MQTT_connect();
  // prevent lockout of mqtt
  mqtt.processPackets(10);

  if (slinitdone)
    return;

  float f;
  Serial.print("size of float: ");
  Serial.println(sizeof(f));
  sl.set_pins(RFM95_CS, RFM95_RST, RFM95_INT);
  sl.init(SL_TOKEN, false);	// no cryto
  sl.register_handler(sl_on_receive);
  slinitdone = true;
}



// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care of (re-)connecting.
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

  Serial.print(F("MQTT connect " SL_SERVER ":"));
  Serial.print(SL_SERVERPORT);
  Serial.print(" at ");
  Serial.print(millis());
  Serial.print("millis: ");

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

void CBupdatestate(char  *cmd, uint16_t len) {
	mqttreceived += 1;
	updatestate(cmd, len);
}

void CBupdatecontrol(char *data, uint16_t len) {

	struct b_typ_0 *cntl = (struct b_typ_0 *)data;

	mqttreceived += 1;
	if ((cntl->b_typ != 0) && (cntl->n_typ != 0)) {
		return;
	}
	sendmsgtonode(&cntl->bpayload, len - sizeof(struct b_typ_0), cntl->node_id);
}


void sendmsgtonode(struct n_typ_0 *msg, uint8_t len, uint8_t toaddr) {
  struct lorapkt *pkt;

  pkt = (struct lorapkt *)allocmem(len + sizeof(lorapkt));
  pkt->toaddr = toaddr;
  memcpy((char *)&pkt->pkt, msg, len-1);
  loraQ.enqueue((uint8_t *)pkt, len + sizeof(lorapkt));
}


void *updatestate(char *cmd, uint16_t len) {

  if (cmd && (cmd[0] == 'r')) {
    UpdStatus("Resetting");
    slinitdone = false;
    mqtt.disconnect();
  } else  {
    UpdStatus("Online");
  }
}
