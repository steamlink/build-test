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

uint8_t data[] = "Hello World!";
// Dont put this on the stack:
uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];

void loop()
{
  Serial.println("Sending message out!");
    
  // Send a message to a rf95_mesh_server
  // A route to the destination will be automatically discovered.
  if (sl.send(data, sizeof(data))){
    // It has been reliably delivered to the next node.
    // Now wait for a reply from the ultimate server
    uint8_t len = sizeof(buf);

    if (sl.receive(buf, &len, 3000))
    {
      Serial.print("got reply: ");
      Serial.println((char*)buf);
    }
    else
      Serial.println("No reply! Are intermediate nodes running?");
  }
  else
     Serial.println("Send failed. Are the intermediate mesh servers running?");
  delay(1000);
}

