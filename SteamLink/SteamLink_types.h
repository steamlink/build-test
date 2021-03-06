
#ifndef STEAMLINK_TYPES_H
#define STEAMLINK_TYPES_H

#pragma pack(push,1)
struct sl_config {
  // private key
  uint8_t key[16];
  // SteamLink ID
  uint32_t sl_id;
  // freq: the SX1276 chips support multiple radio frequencies, we use
  // 915 MHz in North America (ISM Band), leave as default
  float freq = 915.0;
  // TODO: explicitly map between ModemConfigChoice and mod_conf
  uint8_t mod_conf = 0; // 0 maps to RH_RF95::Bw125Cr45Sf128;
  // RH_RF95::ModemConfigChoice mod_conf = RH_RF95::Bw125Cr45Sf128;
  // node address
  uint8_t node_address;
};
#pragma pack(pop)
#define SL_TOKEN_LENGTH sizeof(sl_config)

struct sl_pins {
  uint8_t cs = 8;
  uint8_t reset = 4;
  uint8_t interrupt = 3;
};

enum SL_ERROR {
SL_SUCCESS,
SL_FAIL
};

#endif
