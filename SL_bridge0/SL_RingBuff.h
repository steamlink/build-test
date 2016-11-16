// Simple queue/ringbuffer class

#include <stdint.h>

struct relement {
  uint8_t len;
  uint8_t *pkt;
};

class SL_RingBuff {
  public:
	// initialize with number of elements in the queue
   SL_RingBuff(int size);
	// put an element into the ring, return 0 if ring is full
   int enqueue (uint8_t *val, uint8_t len);
	// return the number of elements in the ring
   int queuelevel ();
	// get an element from the ring, return 0 if ring is empty
   uint8_t *dequeue (uint8_t *len);

  private:
    int _ringsize = 0;
    int _head = 0;
    int _tail = 0;
    struct relement **_ring;
};
