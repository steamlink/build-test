#ifndef STEAMLINKGENERIC_H
#define STEAMLINKGENERIC_H

#include <SteamLink.h>
#include <SteamLinkPacket.h>

#define SL_DEFAULT_STORE_ADDR 1
#define SL_DEFAULT_TEST_ADDR  0
#define SL_DEFAULT_QOS 0

class SteamLinkGeneric {

 public:

  // constructor
  SteamLinkGeneric(uint32_t slid);

  virtual void init(void *conf, uint8_t config_length);

  /// \send
  /// \brief for user to send string to store
  /// \param buf a nul terminated string to send
  /// \returns true if message sends succesfully
  virtual bool send(uint8_t* buf);

  /// ADMIN DATA PACKETS

  virtual bool send_ds(uint8_t* payload, uint8_t payload_length);

  virtual bool send_bs(uint8_t* payload, uint8_t payload_length);

  virtual bool send_on();

  virtual bool send_ak();

  virtual bool send_nk();

  virtual bool send_tr(uint8_t* payload, uint8_t payload_length);

  virtual bool send_ss(uint8_t* payload, uint8_t payload_length);

  virtual void update();

  virtual void handle_admin_packet(uint8_t* packet, uint8_t packet_length);

  /// HANDLER REGISTRATIONS

  virtual void register_receive_handler(on_receive_handler_function on_receive);
  /// \register_admin_handler
  /// \brief if the message is not for this node AND there is a bridge registered, send to bridge handler
  virtual void register_bridge_handler(on_receive_bridge_handler_function on_receive);

  virtual void sign_on_procedure();

  /// DRIVER LEVEL CALLS

  /// driver_send
  /// \brief sends packet to the slid
  /// \param packet is a pointer for the packet to send
  /// \param packet_size is the size of the packet
  /// \param slid is the steamlink id of the receiver node
  virtual bool driver_send(uint8_t* packet, uint8_t packet_length, uint32_t slid, bool is_test);

  virtual bool driver_receive(uint8_t* &packet, uint8_t &packet_size, uint32_t &slid, bool &is_test);

  virtual uint32_t get_slid();

  virtual void set_bridge(BridgeMode mode);

  virtual bool generic_send(uint8_t* packet, uint8_t packet_length, uint32_t slid);


 protected:

  uint32_t _slid;

  // handlers
  on_receive_handler_function _on_receive = NULL;
  on_receive_bridge_handler_function _bridge_handler = NULL;

  // encryption mode
  bool _encrypted;
  uint8_t* _key;

  uint8_t _last_rssi = 0;

  // bridge mode
  // _bridge_mode
  // 0 not connected to bridge
  // 1 is store_side
  // 2 is node_side
  BridgeMode _bridge_mode = unbridged;

  bool sign_on_complete = false;

 private:

};
#endif
