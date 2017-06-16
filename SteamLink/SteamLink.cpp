#include <SteamLink.h>

void phex(uint8_t *data, unsigned int length) {
  char sbuf[17]; 
  char hbuf[49]; 
  uint8_t ih = 0;

  hbuf[48] = '\0';
  sbuf[16] = '\0';
  for (int i=0; i<length; i++) {
    sprintf(&hbuf[ih*3], "%02x ", (char) data[i]);
    if ((data[i] >= 0x20) && (data[i] <= 0x7f)) {
        sbuf[ih] = (char) data[i];
    } else {
        sbuf[ih] = '.';
    }
    ih += 1;
    if ((i % 16) == 15) {
        Serial.print(hbuf);
        Serial.println(sbuf);
        ih = 0;
    }
  }
  if (ih > 0) {
      sbuf[ih] = '\0';
      hbuf[ih*3] = '\0';
      Serial.print(hbuf);
      Serial.println(sbuf);
  }
}

