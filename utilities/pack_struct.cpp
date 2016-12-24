
#include <iostream>

#define CBC 0
#define ECB 1

#pragma pack(push, 1)
struct  mystruct {
  uint8_t key[16];
  uint8_t sl_id[4]; 
  float freq;
  uint8_t modem_config; // = 1;
  uint8_t node_id;
};
#pragma pack(pop)
#define SL_TOKEN_LENGTH sizeof(mystruct)

void shex(uint8_t* buf, uint8_t* str, unsigned int size);
void phex(uint8_t* str, unsigned int size);

void check_string(uint8_t* str) {
  std::cout<<str<<std::endl;
};

int main() {

  // sl key
  uint8_t key[16] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };

  // sl_id
  uint8_t sl_id[4] = {0x0, 0x0, 0x0, 0x99 };

  // make sure AES loads fine
  std::cout<<"aes lib loads fine..."<<std::endl;

  // initialize struct
  struct mystruct foo;

  // sanity check size of struct
  std::cout<<"size of foo is: "<<sizeof(foo)<<std::endl;

  // create foo
  memcpy(foo.key, key, 16);
  foo.freq = 915.0;
  memcpy(foo.sl_id, sl_id, 4);
  foo.modem_config = 0;
  foo.node_id = 4;
  // sanity check the key
  std::cout<<"key is: "<<std::endl;
  phex(foo.key, 16);

  // serialize by memcpy
  uint8_t buf[SL_TOKEN_LENGTH];
  memcpy(buf, &foo, SL_TOKEN_LENGTH);

  // print buf as sanity check
  std::cout<<"buf is: "<<std::endl;
  phex(buf, SL_TOKEN_LENGTH);

  // Take pasted string and recreate struct
  uint8_t str[80] = "2b7e151628aed2a6abf7158809cf4f3c0000009900c064440004";
  std::cout<<"Pasted string is: "<<str<<std::endl;

  // scan in to buf
  uint8_t new_buf[SL_TOKEN_LENGTH];
  shex(new_buf, str, SL_TOKEN_LENGTH);

  // sanity check what we've copied:
  std::cout<<"new buf is: "<<std::endl;
  phex(new_buf, SL_TOKEN_LENGTH);

  // try recreating struct
  struct mystruct bar;

  memcpy(&bar, new_buf, SL_TOKEN_LENGTH);

  // check
  std::cout<<"Key is: "<<std::endl;
  phex(bar.key, 16);
  std::cout<<"sl_id is: "<<std::endl;
  phex(bar.sl_id, 4);
  std::cout<<"Modem config is "<<int(bar.modem_config)<<std::endl;
  std::cout<<"freq is:  "<<float(bar.freq)<<std::endl;
  std::cout<<"node address is:  "<<float(bar.node_id)<<std::endl;


}


void shex(uint8_t* buf, uint8_t* str, unsigned int size) {
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

// prints string as hex
void phex(uint8_t* str, unsigned int size)
{
    unsigned char i;
    for(i = 0; i < size; ++i)
        printf("%.2x", str[i]);
    printf("\n");
}
