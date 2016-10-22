
#ifndef STEAMLINK_TYPES_H
#define STEAMLINK_TYPES_H

struct sl_config {
  // freq: the SX1276 chips support multiple radio frequencies, we use
  // 915 MHz in North America (ISM Band)
  float freq = 915.0;

  // tx_power: set the lowest possible power in the range +5 to +23 dB
  int tx_power = 23;

  RH_RF95::ModemConfigChoice mod_conf = RH_RF95::Bw125Cr45Sf128; // ModemConfigChoice Bw125Cr45Sf128

  uint8_t sl_token;
};

struct sl_pins {
  uint8_t cs = 8;
  uint8_t reset = 4;
  uint8_t interrupt = 3;
};

#endif
