
#ifndef STEAMLINK_TYPES_H
#define STEAMLINK_TYPES_H

#pragma pack(push,1)
struct sl_config {
  // freq: the SX1276 chips support multiple radio frequencies, we use
  // 915 MHz in North America (ISM Band), leave as default
  float freq = 915.0;
  // set to default values
  RH_RF95::ModemConfigChoice mod_conf = RH_RF95::Bw125Cr45Sf128; // ModemConfigChoice Bw125Cr45Sf128
  uint8_t node_address;
  // mesh ID
  uint8_t mesh_id;
  // private key
  uint16_t key;
};
#pragma pack(pop)

struct sl_pins {
  uint8_t cs = 8;
  uint8_t reset = 4;
  uint8_t interrupt = 3;
};



#endif
