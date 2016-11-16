
#include <malloc.h>
#include "SL_RingBuff.h"

#include <RHMesh.h>
//#include <RH_RF95.h>
//#include "SteamLink_types.h"
//#include <SPI.h>
//#include "aes.h"



// create a ring with 'size' elements


SL_RingBuff::SL_RingBuff(int size) {
  _ringsize = size;
  _ring = (struct relement **)calloc(size,  sizeof(relement));
}


// Put something into the buffer. Returns 0 when the buffer was full,
// 1 when the stuff was put sucessfully into the buffer
int SL_RingBuff::enqueue (uint8_t *val, uint8_t len) {
  Serial.print("enqueue len: ");
  Serial.println(len);
  int newtail = (_tail + 1) % _ringsize;
  if (newtail == _head) {
    // Buffer is full, do nothing
    return 0;
  }
  else {
    _ring[_tail]->len = len;
    _ring[_tail]->pkt = val;
    _tail = newtail;
    return 1;
  }
}

// Return number of elements in the queue.
int SL_RingBuff::queuelevel () {
   return _tail - _head + (_head > _tail ? _ringsize : 0);
}

// Get something from the queue. 0 will be returned if the queue
// is empty
uint8_t *SL_RingBuff::dequeue (uint8_t *len) {
  if (_head == _tail) {
    return 0;
  }
  else {
    uint8_t *val = _ring[_head]->pkt;
    *len = _ring[_head]->len;
    _head  = (_head + 1) % _ringsize;
    return val;
  }
}

