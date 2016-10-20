// Simple queue/ringbuffer class

class SL_RingBuff {
  public:
	// initialize with number of elements in the queue
   SL_RingBuff(int size);
	// put an element into the ring, return 0 if ring is full
   int enqueue (char * val);
	// return the number of elements in the ring
   int queuelevel ();
	// get an element from the ring, return 0 if ring is empty
   char *dequeue ();

  private:
    int _ringsize = 0;
    int _head = 0;
    int _tail = 0;
    char **_ring;
};
