#include "SteamLink.h"


#undef DBG // no local debug
// bool encrypted = true by default
void SteamLink::init(char* token, bool encrypted) {
  // Class to manage message delivery and receipt, using the driver declared above
  //DBG debug("entering init()");

  //DBG debug("extracting token fields");
  extract_token_fields((uint8_t*) token, SL_TOKEN_LENGTH);
  
  debug("Node configuration");
#ifdef DBG
  phex(conf.key, 16);
  Serial.println();
#endif
  Serial.print("sl_id: ");
  Serial.println(conf.sl_id);
  Serial.print("freq: ");
  Serial.println(conf.freq);
  Serial.print("mod_conf: ");
  Serial.println(conf.mod_conf);
  Serial.print("node_address: ");
  Serial.println(conf.node_address);
  
  driver = new RH_RF95(pins.cs, pins.interrupt);
//  manager = new RHMesh(*driver, conf.node_address);
  manager = new RHDatagram(*driver, conf.node_address);
  debug("Running in Datagram mode");


  // initialize manager
  if (!manager->init()) {
    debug("SL_FATAL: manager initialization failed");
    while (1);
  } 

  debug("manager init done!");
  // Set frequency
  if (!driver->setFrequency(conf.freq)) {
    debug("SL_FATAL: setFrequency failed");
    while (1);
  }

  // Set modem configuration
  bool rc;
  if (conf.mod_conf > 3) {
	driver->setModemRegisters(&modem_config[conf.mod_conf-4]);
    rc = true;
  }
  else 
	rc = driver->setModemConfig((RH_RF95::ModemConfigChoice) conf.mod_conf);
  if (!rc) {
    debug("SL_FATAL: setModemConfig failed with invalid configuration");
    while (1);
  }

  debug("Modem config done!");
  // set timeout for CAD to 10s
  // TODO: Make sure CAD actually does something!!!
  randomSeed(analogRead(A0));
  driver->setCADTimeout(10000);
  // set antenna power
  driver->setTxPower(tx_power, false);
  // set encryption mode
  encryption_mode = encrypted;
}

SL_ERROR SteamLink::send(uint8_t* buf) {
  uint8_t len = strlen((char*) buf) + 1;	//N.B. Send terminating \0
  send(buf, SL_DEFAULT_BRIDGE, len);
}

SL_ERROR SteamLink::send(uint8_t* buf, uint8_t to_addr, uint8_t len) {
  //DBG debug("entering send()");
  // note: 1st 4 bytes are always the sl_id
  uint8_t* encrypted_packet;
  uint8_t* packet;
  uint8_t encrypted_packet_size;
  uint8_t packet_size;
  uint8_t sent;
   
  // TODO: change with actual determined error codes
  if (len >= SL_MAX_MESSAGE_LEN) return SL_FAIL;
  
  if (encryption_mode) {
    encrypted_packet = encrypt_alloc(&encrypted_packet_size, buf, len, conf.key);
#ifdef DBG
	Serial.print("sl.send: size ");
    Serial.print(encrypted_packet_size);
	Serial.print(" to: ");
	Serial.print(to_addr);
    Serial.print(" packet: '");
    Serial.print((char *)buf);
    Serial.print("' encrypt: ");
    phex(encrypted_packet, encrypted_packet_size);
	Serial.println();
#endif
    packet_size = encrypted_packet_size + 4;
    packet = (uint8_t*) malloc(packet_size);
    memcpy(&packet[0], &conf.sl_id, sizeof(conf.sl_id));
    memcpy(&packet[4], encrypted_packet, encrypted_packet_size);
    free(encrypted_packet);
//    sent = manager->sendtoWait(packet, packet_size, to_addr);
    sent = manager->sendto(packet, packet_size, to_addr);
    free(packet);
  } else {	// not encrypted -> send raw
//    sent = manager->sendtoWait(buf, len, to_addr);
    sent = manager->sendto(buf, len, to_addr);
  }

  // figure out error codes
  if (sent == 0)  {
    return SL_SUCCESS;
  }  else {
    return SL_FAIL;
  }
}

void SteamLink::register_handler(on_receive_handler_function on_receive) {
  _on_receive = on_receive;
}

void SteamLink::register_handler(on_receive_from_handler_function on_receive_from) {
  _on_receive_from = on_receive_from;
}

void SteamLink::update() {
  // allocate max size
  uint8_t rcvlen = sizeof(slrcvbuffer);
  uint8_t *packet;
  uint8_t packet_len;
  uint8_t from; // TODO: might need to "validate" sender!
  uint8_t to;
  //recv packet
//  bool received = manager->recvfromAck(slrcvbuffer, &rcvlen, &from);
  bool received = manager->recvfrom(slrcvbuffer, &rcvlen, &from, &to);
  if (received && (to == conf.node_address)) {
    last_rssi = driver->lastRssi();
    // decrypt if we have encryption mode on
#ifdef DBG
    Serial.print("update: recvfromAck: ");
    phex(slrcvbuffer, rcvlen);
    Serial.println();
#endif
    if(encryption_mode) {
     // slrcvbuffer[0] has sl_id with 4 bytes
      packet = &slrcvbuffer[4];
      decrypt(packet, rcvlen-4, conf.key);
      packet_len = strlen((char *)packet);
	} else {
      packet = slrcvbuffer;
      packet_len = rcvlen;
    }
    if (_on_receive != NULL) {
      _on_receive(packet, packet_len);
    } else if (_on_receive_from != NULL) {
      _on_receive_from(packet, packet_len, from);
    }
  }
}

uint8_t SteamLink::get_last_rssi() {
  return last_rssi;
}
void SteamLink::extract_token_fields(uint8_t* str, uint8_t size){
  uint8_t buf[SL_TOKEN_LENGTH]; 

  // Make sure we're validated
  if(!validate_token(str, size)){
    Serial.println("SL_FATAL: Token invalid!");
    while(1);
  }

  // scan in the values
  shex(buf, str, SL_TOKEN_LENGTH);
  //DBG debug("after shex");
  //DBG phex(buf, SL_TOKEN_LENGTH);
  // copy values in to struct
  memcpy(&conf, buf, SL_TOKEN_LENGTH);
  //DBG debug("after memcpy");
}

// Takes in buf to write to, str with hex to convert from, and length of buf (i.e. num of hex bytes)
void SteamLink::shex(uint8_t* buf, uint8_t* str, unsigned int size) {
  unsigned char i;
  uint8_t a,b;

  for (i = 0; i < size; i++) {
    a = str[i*2] - 0x30;
    if (a > 9) a -= 39;
    b = str[i*2+1] - 0x30;
    if (b > 9) b -= 39;
    buf[i] = (a << 4) + b;
  }
}

bool SteamLink::validate_token(uint8_t* str, uint8_t size){
  // TODO: actually validate the token!
  return true;
}
void SteamLink::set_pins(uint8_t cs=8, uint8_t reset=4, uint8_t interrupt=3) {
  pins.cs = cs;
  pins.reset = reset;
  pins.interrupt = interrupt;
}

uint8_t* SteamLink::encrypt_alloc(uint8_t* outlen, uint8_t* in, uint8_t inlen, uint8_t* key) {
  //DBG Serial.print("Entering encrypt alloc, inlen: ");
  //DBG Serial.println(inlen);
  uint8_t num_blocks = int((inlen+15)/16);
  //DBG debug("printing numblocks:");
  //DBG Serial.println(num_blocks);
  uint8_t* out = (uint8_t*) malloc(num_blocks*16);
  //DBG debug("Allocated memory for out");
  *outlen = num_blocks*16;
  memcpy(out, in, inlen);
  memset(out + inlen, 0, *outlen - inlen);
  for(int i = 0; i < num_blocks; ++i) {
    //DBG Serial.println(i);
    AES128_ECB_encrypt(out + i*16, key, out + i*16);
  }
  //DBG debug("finishing encryption");
  return out;
};

// decrypt
void SteamLink::decrypt(uint8_t* in, uint8_t inlen, uint8_t* key) {
  uint8_t num_blocks = inlen/16;
  for(int i = 0; i < num_blocks; i++) {
    AES128_ECB_decrypt(in+i*16, key, in + i*16);
  }
};

void SteamLink::debug(char* string) {
#ifdef DEBUG
  if(Serial) {
    Serial.println(string);
  }
#endif
  return;
};

// prints 8-bit data in hex with leading zeroes
void SteamLink::phex(uint8_t *data, unsigned int length) {
  Serial.print("0x"); 
  for (int i=0; i<length; i++) { 
    if (data[i]<0x10) {Serial.print("0");} 
    Serial.print(data[i],HEX); 
    Serial.print(" "); 
  }
}



