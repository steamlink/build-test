// SL_testclient0
// send pkts to bridge at address 1
// Mesh has much greater memory requirements, and you may need to limit the
// max message length to prevent wierd crashes
#define SL_TOKEN "3ca5de6fa904467e4e66f1fc8e6f54bd1500000000c064440206"

#define MAX_MESSAGE_LEN 50

#include <SteamLink.h>

#define VER "4"

// for Feather M0
#if 1
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
#define LORALED 13
#define VBATPIN A7
   
#endif

// for Adafruit Huzzah breakout
#if 0
#define RFM95_CS 15
#define RFM95_RST 4
#define RFM95_INT 5
#undef LORALED
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
  Serial.println(F("!ID SL_testclient0 " VER));

  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  digitalWrite(LED, HIGH);

#ifdef LORALED
  pinMode(LORALED, OUTPUT);
#endif

  sl.set_pins(RFM95_CS, RFM95_RST, RFM95_INT);
  sl.init(SL_TOKEN);
  sl.register_handler(sl_on_receive);

  Serial.println("Steamlink init done");
  bLast = 2;
}


// Dont put this on the stack:
uint8_t buf[MAX_MESSAGE_LEN];
int beforeTime = 0, afterTime = 0, nextSendTime = 0;
int waitInterval = 4000;

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
  Serial.println("restarting loop");
  uint8_t len = sizeof(buf);

  sl.update();

  Serial.println("after update");

  bCurrent = digitalRead(BUTTON);
  if ((millis() > nextSendTime) || (bCurrent != bLast)) {
    Serial.println("inside if statement");
    packet_num += 1;
    if (bCurrent != bLast) {
      int8_t value = (bCurrent == LOW ? 1 : 0);
      snprintf((char*) data, sizeof(data), "Button %i pkt: %d", value, packet_num);
      bLast = bCurrent;
    } else {
      snprintf((char*) data, sizeof(data), "Hello World! pkt: %d vBat: %dmV", packet_num, getBatInfo());
    }
    beforeTime = millis();
#ifdef LORALED
    digitalWrite(LORALED, HIGH);
#endif
    Serial.println("before sl send");
    bool rc = sl.send(data);
    Serial.println("returned sl send");
#ifdef LORALED
    digitalWrite(LORALED, LOW);
#endif
	rc = SL_SUCCESS;
    if (rc == SL_SUCCESS)
    {
      Serial.println("success!");
      afterTime = millis() - beforeTime;
      // It has been reliably delivered to the next node.
      Serial.print("Sent \"");
      Serial.print((char *)data);
      Serial.print( "\" time: ");
      Serial.println(afterTime);
    }
    else {
      Serial.println("sl.send failed. Bridge down?");
    }
    nextSendTime = millis() + waitInterval;
  }
  Serial.println("eol");
}

void sl_on_receive(uint8_t *buf, uint8_t len) {
    Serial.print("sl_on_receive: len: ");
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
