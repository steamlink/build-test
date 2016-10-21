// SL_testclient0
// send pkts to bridge at address 4
// Mesh has much greater memory requirements, and you may need to limit the
// max message length to prevent wierd crashes
#define RH_MESH_MAX_MESSAGE_LEN 50

#include <RHMesh.h>
#include <RH_RF95.h>
#include <SPI.h>

#define VER "1"

// for Feather M0
#if 1
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
#endif

// for Adafruit Huzzah breakout
#if 0
#define RFM95_CS 15
#define RFM95_RST 4
#define RFM95_INT 5
#endif

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

// minimum transmisison gap (ms)
#define MINTXGAP 125

// LORA addresses used
#define CLIENT_ADDRESS 1
#define BRIDGE_ADDRESS 4


// test LED and button
#define LED 5
#define BUTTON 6


#define  MIN(a,b) (((a)<(b))?(a):(b))
#define  MAX(a,b) (((a)>(b))?(a):(b))

// Singleton instance of the radio driver
RH_RF95 driver(RFM95_CS, RFM95_INT);

// Class to manage message delivery and receipt, using the driver declared above
RHMesh manager(driver, CLIENT_ADDRESS);

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
  Serial.println(F("!ID SL_testclient" VER));

  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  digitalWrite(LED, HIGH);

  if (!manager.init())
    Serial.println("init failed");
  // Defaults after init are 434.0MHz, 0.05MHz AFC pull-in, modulation FSK_Rb2_4Fd36
  Serial.println("Manager init done");
  if (!driver.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.println("setpower");
  driver.setTxPower(23, false);
  bLast = 2;
}


// Dont put this on the stack:
uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];
int beforeTime = 0, afterTime = 0, nextSendTime = 0;
int waitInterval = 1000;

//
// LOOP
//
void loop()
{
  uint8_t len = sizeof(buf);
  uint8_t from;

  bCurrent = digitalRead(BUTTON);
  if ((millis() > nextSendTime) || (bCurrent != bLast)) {
    packet_num += 1;
    if (bCurrent != bLast) {
      int8_t value = (bCurrent == LOW ? 1 : 0);
      snprintf((char*) data, sizeof(data), "Button %i!  pkt: %d", value, packet_num);
      bLast = bCurrent;
    } else {
      snprintf((char*) data, sizeof(data), "Hello World! pkt: %d", packet_num);
    }
    beforeTime = millis();
    if (manager.sendtoWait(data, sizeof(data), BRIDGE_ADDRESS) == RH_ROUTER_ERROR_NONE)
    {
      afterTime = millis() - beforeTime;
      // It has been reliably delivered to the next node.
      Serial.print("Sent \"");
      Serial.print((char *)data);
      Serial.print("\" to ");
      Serial.print( BRIDGE_ADDRESS);
      Serial.print( ", time: ");
      Serial.println(afterTime);
    }
    else {
      Serial.println("sendtoWait failed. Bridge down?");
    }
    nextSendTime = millis() + waitInterval;
  }

  if (manager.available()) {
    if (manager.recvfromAckTimeout(buf, &len, 3000, &from))
    {
      Serial.print("got msg from : ");
      Serial.print(from);
      Serial.print(": ");
      Serial.println((char*)buf);
      int v = atoi((char *)buf);
	  if (v == 0) {
        digitalWrite(LED, LOW);
	  } else if (v == 1) {
        digitalWrite(LED, HIGH);
	  } else { 
        waitInterval = MAX(MINTXGAP, v);
        Serial.print("waitInterval now ");
        Serial.println(waitInterval);
      }
    }
    else
    {
      Serial.println("available() and not pkt??");
    }
  }
}
