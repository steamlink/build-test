// SL_testclient0
// send pkts to bridge at address 1
// Mesh has much greater memory requirements, and you may need to limit the
// max message length to prevent wierd crashes
#define MAX_MESSAGE_LEN 50

#include <SteamLink.h>

#define VER "2"

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

// test LED and button
#define LED 5
#define BUTTON 6


#define  MIN(a,b) (((a)<(b))?(a):(b))
#define  MAX(a,b) (((a)>(b))?(a):(b))

SteamLink sl;

/* Packet building */
uint8_t data[100];
int packet_num = 0;

// button state
int8_t bLast, bCurrent = 2;

void sl_on_receive(uint8_t *buf, uint8_t len) ;
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

  digitalWrite(LED, HIGH);

#define SL_TOKEN "2b7e151628aed2a6abf7158809cf4f3c0000009900c06444000300"
  sl.set_pins(RFM95_CS, RFM95_RST, RFM95_INT);
  sl.init(SL_TOKEN);
  sl.register_handler(sl_on_receive);

  Serial.println("Steamlink init done");
  bLast = 2;
}


// Dont put this on the stack:
uint8_t buf[MAX_MESSAGE_LEN];
int beforeTime = 0, afterTime = 0, nextSendTime = 0;
int waitInterval = 20000;

//
// LOOP
//
void loop()
{
  uint8_t len = sizeof(buf);

  sl.update();
  bCurrent = digitalRead(BUTTON);
  if ((millis() > nextSendTime) || (bCurrent != bLast)) {
    packet_num += 1;
    if (bCurrent != bLast) {
      int8_t value = (bCurrent == LOW ? 1 : 0);
      snprintf((char*) data, sizeof(data), "Button %i pkt: %d", value, packet_num);
      bLast = bCurrent;
    } else {
      snprintf((char*) data, sizeof(data), "Hello World! pkt: %d", packet_num);
    }
    beforeTime = millis();
    if (sl.send(data) == SL_SUCCESS)
    {
      afterTime = millis() - beforeTime;
      // It has been reliably delivered to the next node.
      Serial.print("Sent \"");
      Serial.print((char *)data);
      Serial.print( "\" time: ");
      Serial.println(afterTime);
    }
    else {
      Serial.println("sendtoWait failed. Bridge down?");
    }
    nextSendTime = millis() + waitInterval;
  }
}

void sl_on_receive(uint8_t *buf, uint8_t len) {
    Serial.print("got len: ");
    Serial.print(len);
    Serial.print(" msg: ");
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
