// Mesh has much greater memory requirements, and you may need to limit the
// max message length to prevent wierd crashes
#define RH_MESH_MAX_MESSAGE_LEN 50
#include "SteamLink.h"

SteamLink sl;
void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("Starting LoRa");
  sl.set_pins(8,4,3);
  sl.init();
}

uint8_t data[80];
// Dont put this on the stack:
uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];
int packet_num = 0;
long beforeTime, afterTime;
void loop()
{
  Serial.println("Sending message out!");
  snprintf((char*) data,80, "Hello World %d", packet_num++);
  beforeTime = millis();
  // Send a message to a rf95_mesh_server
  // A route to the destination will be automatically discovered.
  if (!sl.send(data, sizeof(data))){
    Serial.print("Sent data at: ");
    Serial.println(beforeTime);
  }
  else
     Serial.println("Send failed. Are the intermediate mesh servers running?");
  //delay(10);
  afterTime = millis();

  Serial.print("Total time taken to send was: ");
  Serial.println(afterTime - beforeTime);
}

