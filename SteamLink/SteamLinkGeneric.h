#ifndef STEAMLINKGENERIC_H
#define STEAMLINKGENERIC_H

typedef void (*on_receive_handler_function)(uint8_t* buffer, uint8_t size); // user
typedef void (*on_receive_bridge_handler_function)(uint8_t* buffer, uint8_t size, uint32_t slid, uint8_t flags, uint8_t rssi); // bridge

class SteamLinkGeneric {

 public:

  // constructor
  SteamLinkGeneric(uint32_t slid);

  virtual void init(bool encrypted=false, uint8_t* key=NULL);

  /// \send
  /// \brief sends string to store
  /// \param buf a nul terminated string to send
  /// \returns true if message sends succesfully
  virtual bool send(uint8_t* buf) = 0;

  virtual void update();

  virtual void register_receive_handler(on_receive_handler_function on_receive);

  /// bridge_send
  /// \brief sends packet to store from slid if upstream
  ///        sends packet to slid from store if downstream
  /// \param packet is a pointer for the packet to send
  /// \param packet_size is the size of the packet
  /// \param slid is the steamlink id of the receiver node
  virtual bool bridge_send(uint8_t* packet, uint8_t packet_size, uint32_t slid, uint8_t flags, uint8_t rssi) = 0;

  /// \register_bridge_handler
  /// \brief if the message is not for this node AND there is a bridge registered, send to bridge handler
  virtual void register_bridge_handler(on_receive_bridge_handler_function on_receive);

  virtual void set_bridge();

  virtual void unset_bridge();

  /// \is_primary
  /// \brief check if this node can talk directly to the store
  ///  all primary nodes can talk to the store
  /// \returns true if primary
  virtual bool is_primary();

 protected:

  uint32_t _slid;
  bool _is_primary = 0;

  // bridge stuff
  bool _is_bridge = false;
  // handlers
s on_receive_handler_function _on_receive = NULL;
  on_receive_bridge_handler_function _on_bridge_receive = NULL;

  // encryption mode
  bool _encrypted;
  uint8_t* _key;

 private:

}
#endif
