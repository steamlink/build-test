#ifndef STEAMLINKLORA_H
#define STEAMLINKLORA_H

#include <RHDatagram.h>
#include <RH_RF95.h>
#include <SteamLinkGeneric.h>

#define SL_DEFAULT_SLID_SIZE 32 // in bits
#define SL_LORA_DEFAULT_NODE_SIZE 8 // in bits
#define SL_LORA_DEFAULT_MESH_SIZE  SL_DEFAULT_SLID_SIZE - SL_LORA_DEFAULT_NODE_SIZE

#define SL_LORA_DEFAULT_BRIDGE_ADDR 1

#define SL_LORA_MAX_MESSAGE_LEN 64

#define SL_LORA_DEFAULT_TXPWR 23
#define SL_LORA_DEFAULT_FREQUENCY 915

#define SL_LORA_DEFAULT_FLAGS 0

/*
This library needs the following defines:

In the SteamLink LoRa driver the SLID, node_addr and mesh_addr are related:
slid is 32 bits
MSB...........................LSB
[24 bit mesh_id][8 bit node_addr]
*/

class SteamLinkLora : public SteamLinkGeneric {

 public:

  // constructor
  SteamLinkLora(uint32_t slid);

  virtual void init(bool encrypted=true, uint8_t* key=NULL);

  /// \send
  /// \brief sends an ntype0_packet encapsulated string to default bridge address
  /// \param buf a nul terminated string to send
  /// \returns true if message sends succesfully
  virtual bool send(uint8_t* buf);

  virtual void update();

  /// bridge_send
  /// \brief sends an ntype0 packet to slid
  /// \param packet is a pointer to ntype0 packet to send
  /// \param packet_size is the size of the packet
  /// \param slid is the steamlink id of the receiver node
  virtual bool bridge_send(uint8_t* packet, uint8_t packet_size, uint32_t slid, uint8_t flags, uint8_t rssi);

  //TODO: lora specific?

  /// get_addrs_from_slid
  /// \brief LoRa driver uses the 32 bit slid
  ///  24 bits are mesh_id and 8 bits are node_addr
  ///  this function is to convert from slid to node_addr and mesh_id
  /// \param slid
  /// \returns node_addr
  uint8_t get_node_from_slid(uint32_t slid);

  uint32_t get_mesh_from_slid(uint32_t slid);

  void set_pins(uint8_t cs, uint8_t reset, uint8_t interrupt);

  void set_modem_config(uint8_t mod_conf);

 private:

  RH_RF95 *_driver;
  RHDatagram *_manager;

  bool update_modem_config();

  // only the driver needs to be aware of node_addr
  uint8_t _node_addr;

  // LORA specific pins
  uint8_t _cs_pin;
  uint8_t _reset_pin;
  uint8_t _interrupt_pin;

  // modem_config
  uint8_t _mod_conf = 0;

  // LORA driver stuff
  uint8_t driverbuffer[SL_LORA_MAX_MESSAGE_LEN];

  // Custom modem config
  RH_RF95::ModemConfig modem_config[1] = {
    { // conf number 4, see https://lowpowerlab.com/forum/general-topics/issue-with-lora-long-range-modes-and-rfm95rfm96/
      .reg_1d = 0x78, // Reg 0x1D: BW=125kHz, Coding=4/8, Header=explicit
      .reg_1e = 0xc4, // Reg 0x1E: Spread=4096chips/symbol, CRC=enable
      .reg_26 = 0x0c  // Reg 0x26: LowDataRate=On, Agc=On
    }
  };

};
#endif
