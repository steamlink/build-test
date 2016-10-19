
class SL_RingBuff {
  public:
   SL_RingBuff(int size);
   int enqueue (char * val);
   int queuelevel ();
   char *dequeue ();

  private:
    int _ringsize = 0;
    int _head = 0;
    int _tail = 0;
    char **_ring;
};
