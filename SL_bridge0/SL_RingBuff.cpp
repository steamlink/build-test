
#include <malloc.h>
#include "SL_RingBuff.h"


SL_RingBuff::SL_RingBuff(int size) {
	_ringsize = size;
	_ring = (char **)malloc(size * sizeof(char *));
}


// Put something into the buffer. Returns 0 when the buffer was full,
// 1 when the stuff was put sucessfully into the buffer
int SL_RingBuff::enqueue (char * val) {
  int newtail = (_tail + 1) % _ringsize;
  if (newtail == _head) {
     // Buffer is full, do nothing
     return 0;
  }
  else {
     _ring[_tail] = val;
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
char *SL_RingBuff::dequeue () {
  if (_head == _tail) {
     return 0;
  }
  else {
     char *val = _ring[_head];
     _head  = (_head + 1) % _ringsize;
     return val;
  }
}

