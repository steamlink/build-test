#include <SteamLink.h>

void phex(uint8_t *data, unsigned int length) {
  Serial.print("0x");
  for (int i=0; i<length; i++) {
    if (data[i]<0x10) {Serial.print("0");}
    Serial.print(data[i],HEX);
    Serial.print(" ");
  }
}
