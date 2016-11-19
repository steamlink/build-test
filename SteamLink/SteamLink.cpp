#include "SteamLink.h"

void SteamLink::init(uint8_t* token) {
  // Class to manage message delivery and receipt, using the driver declared above
  debug("entering init()");

  debug("extracting token fields");
  extract_token_fields(token, SL_TOKEN_LENGTH);
  
  debug("dumping token fields");
  phex(conf.key, 16);
  //debug(conf.mesh_id);
  Serial.println(conf.mesh_id);
  //debug(conf.freq);
  Serial.println(conf.freq);
  //debug(conf.mod_conf);
  Serial.println(conf.mod_conf);
  //debug(conf.node_address);
  Serial.println(conf.node_address);
  
  driver = new RH_RF95(pins.cs, pins.interrupt);
  manager = new RHMesh(*driver, conf.node_address);

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
  if (!driver->setModemConfig(conf.mod_conf)) {
    debug("SL_FATAL: setModemConfig failed with invalid configuration");
    while (1);
  }
  debug("Modem config done!");
  // set timeout for CAD to 10s
  // TODO: Make sure CAD actually does something!!!
  driver->setCADTimeout(10000);
  // set antenna power
  driver->setTxPower(tx_power, false);
}

SL_ERROR SteamLink::send(uint8_t* buf) {
  send(buf, SL_DEFAULT_BRIDGE);
}

SL_ERROR SteamLink::send(uint8_t* buf, uint8_t to_addr) {
  debug("entering send()");
  uint8_t len = strlen((char*) buf) + 1;	//N.B. Send terminating \0
  uint8_t* packet;
  uint8_t packet_size;
   
  // TODO: change with actual determined error codes
  if (len >= SL_MAX_MESSAGE_LEN) return FAIL;
  Serial.println("Printing packet size");
  
  packet = encrypt_alloc(&packet_size, buf, len, conf.key);
  Serial.println(packet_size);
  debug("printing packet in hex ");
  phex(packet, packet_size);
  bool sent = manager->sendtoWait(packet, packet_size, to_addr);

  free(packet);
  
  // figure out error codes
  if (sent == 0)  {
    return SUCCESS;
  }  else {
    return FAIL;
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
  uint8_t rcvlen;
  uint8_t from; // TODO: might need to "validate" sender!
  //recv packet
  bool received = manager->recvfromAck(slrcvbuffer, &rcvlen, &from);
  if (received) {
    decrypt(slrcvbuffer, rcvlen, conf.key);
    if (_on_receive != NULL) {
      _on_receive(slrcvbuffer, rcvlen);
    } else if (_on_receive_from != NULL) {
      _on_receive_from(slrcvbuffer, rcvlen, from);
    }
  }
}

void SteamLink::extract_token_fields(uint8_t* str, uint8_t size){
  uint8_t buf[SL_TOKEN_LENGTH]; 

  // Make sure we're validated
  if(!validate_token(str, size)){
    Serial.println("SL_FATAL: Token invalid!");
    while(1);
  }

  Serial.print("sizeof(conf) ");
  Serial.println(sizeof(conf));

  // scan in the values
  shex(buf, str, SL_TOKEN_LENGTH);
  debug("after shex");
  phex(buf, SL_TOKEN_LENGTH);
  // copy values in to struct
  memcpy(&conf, buf, SL_TOKEN_LENGTH);
  debug("after memcpy");
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
  Serial.print("Entering encrypt alloc, inlen: ");
  Serial.println(inlen);
  uint8_t num_blocks = int((inlen+15)/16);
  debug("printing numblocks:");
  Serial.println(num_blocks);
  uint8_t* out = (uint8_t*) malloc(num_blocks*16);
  debug("Allocated memory for out");
  *outlen = num_blocks*16;
  memcpy(out, in, inlen);
  memset(out + inlen, 0, *outlen - inlen);
  for(int i = 0; i < num_blocks; ++i) {
    Serial.println(i);
    AES128_ECB_encrypt(out + i*16, key, out + i*16);
  }
  debug("finishing encryption");
  return out;
};

// decrypt
void SteamLink::decrypt(uint8_t* in, uint8_t inlen, uint8_t* key) {
  uint8_t num_blocks = inlen/16;
  uint8_t* out = (uint8_t*) malloc(inlen);
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


void SteamLink::phex(uint8_t *data, unsigned int length) // prints 8-bit data in hex with leading zeroes
{
       Serial.print("0x"); 
       for (int i=0; i<length; i++) { 
         if (data[i]<0x10) {Serial.print("0");} 
         Serial.print(data[i],HEX); 
         Serial.print(" "); 
       }
}



