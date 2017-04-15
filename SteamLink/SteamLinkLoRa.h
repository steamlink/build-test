#ifndef STEAMLINKLORA_H
#define STEAMLINKLORA_H

#define SL_DEFAULT_SLID_SIZE 32 // in bits
#define SL_LORA_DEFAULT_NODE_SIZE 8 // in bits
#define SL_LORA_DEFAULT_MESH_SIZE  SL_DEFAULT_SLID_SIZE - SL_LORA_DEFAULT_NODE_SIZE

#define SL_LORA_DEFAULT_BRIDGE_ADDR 1

#define SL_LORA_MAX_MESSAGE_LEN 64

#define SL_LORA_DEFAULT_TXPWR 23
#define SL_LORA_DEFAULT_FREQUENCY 915

/*
This library needs the following defines:

In the SteamLink LoRa driver the SLID, node_addr and mesh_addr are related:
slid is 32 bits
MSB...........................LSB
[24 bit mesh_id][8 bit node_addr]
*/

class SteamLinkLora {

 public:

  //TODO  needs constructor which passes in SLID
  typedef void (*on_receive_handler_function)(uint8_t* buffer, uint8_t size);

  // buffer is node's payload
  typedef void (*on_receive_bridge_handler_function)(uint8_t* buffer, uint8_t size, uint32_t slid, uint8_t flag, uint8_t rssi);

  // constructor
  SteamLinkLora(uint32_t slid);

  void init(bool encrypted=true, uint8_t* key=NULL);

  /// \send
  /// \brief sends an ntype0_packet encapsulated string to default bridge address
  /// \param buf a nul terminated string to send
  /// \returns true if message sends succesfully
  bool send(uint8_t* buf);

  void update();

  void register_receive_handler(on_receive_handler_function on_receive);

  /// bridge_send
  /// \brief sends an ntype0 packet to slid
  /// \param packet is a pointer to ntype0 packet to send
  /// \param packet_size is the size of the packet
  /// \param slid is the steamlink id of the receiver node
  bool bridge_send(uint8_t* packet, uint8_t packet_size, uint32_t slid);

  /// \register_bridge_handler
  /// \brief if the message is not for this node AND there is a bridge registered, send to bridge handler
  void register_bridge_handler(on_receive_bridge_handler_function on_receive);

  void set_bridge();

  /// get_addrs_from_slid
  /// \brief LoRa driver uses the 32 bit slid
  ///  24 bits are mesh_id and 8 bits are node_addr
  ///  this function is to convert from slid to node_addr and mesh_id
  /// \param slid
  /// \returns node_addr
  uint32_t get_node_from_slid(uint32_t slid);

  uint32_t get_net_from_slid(uint32_t slid);

  //TODO: lora specific?
  void set_pins(uint8_t cs, uint8_t reset, uint8_t interrupt);

 private:

  RH_RF95 *driver;

  // only the driver needs to be aware of node_addr
  uint8_t _node_addr;

  uint32_t _slid;

  // bridge stuff
  bool _is_bridge;

  // handlers
  on_receive_handler_function _on_receive = NULL;
  on_receive_bridge_handler_function _on_receive_bridge = NULL;

  // encryption mode
  bool _encrypted;
  uint8_t* _key;

  // LORA specific pins
  uint8_t _cs_pin;
  uint8_t _reset_pin;
  uint8_t _interrupt_pin;

  // LORA driver stuff
  uint8_t driverbuffer[SL_MAX_MESSAGE_LEN];

}
#endif
