
#include <iostream>

#define CBC 0
#define ECB 1
#define SL_TOKEN_LENGTH 23

#pragma pack(push, 1)
struct  mystruct {
  uint8_t key[16];
  uint8_t mesh_id; // = 10;
  float freq;
  uint8_t modem_config; // = 1;
  uint8_t node_id;
};
#pragma pack(pop)

void shex(uint8_t* buf, uint8_t* str, unsigned int size);
void phex(uint8_t* str, unsigned int size);
/*
void test_encrypt_cbc(void);
void test_decrypt_cbc(void);


uint8_t* encrypt_alloc(uint8_t* outlen, uint8_t* in, uint8_t inlen, uint8_t* key) {
  uint8_t num_blocks = int((inlen+15)/16);
  uint8_t* out = (uint8_t*) malloc(num_blocks*16);
  *outlen = num_blocks*16;
  memset(out + inlen, 0, *outlen - inlen);
  for(int i = 0; i < num_blocks; ++i) {
    AES128_ECB_encrypt(in+i*16, key, in + i*16);
  }
  return in;
};

void decrypt(uint8_t* in, uint8_t inlen, uint8_t* key) {
  uint8_t num_blocks = inlen/16;
  std::cout<<int(num_blocks)<<std::endl;
  uint8_t* out = (uint8_t*) malloc(inlen);
  for(int i = 0; i < num_blocks; i++) {
    AES128_ECB_decrypt(in+i*16, key, in + i*16);
  }
 };
*/
void check_string(uint8_t* str) {
  std::cout<<str<<std::endl;
};

int main() {

  // sl key
  uint8_t key[16] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };

  // make sure AES loads fine
  std::cout<<"aes lib loads fine..."<<std::endl;

  // initialize struct
  struct mystruct foo;

  // sanity check size of struct
  std::cout<<"size of foo is: "<<sizeof(foo)<<std::endl;

  // create foo
  memcpy(foo.key, key, 16);
  foo.freq = 915.0;
  foo.mesh_id = 10;
  foo.modem_config = 0;
  foo.node_id = 3;
  // sanity check the key
  std::cout<<"key is: "<<std::endl;
  phex(foo.key, 16);
  /*
  uint8_t str[18] = "Hello World";

  std::cout<<str<<std::endl;
  std::cout<<"size of str is: "<<sizeof(str)<<std::endl;


  uint8_t packet_size;
  uint8_t* packet;
  uint8_t* outstr;
  packet = encrypt_alloc(&packet_size, str, sizeof(str), key);
  std::cout<<"Packet size is: "<<packet_size<<std::endl;
  decrypt(packet, packet_size, key);

  std::cout<<"Decrypted values is: "<<std::endl;
  std::cout<<packet<<std::endl;

  //free(packet);
  //free(outstr);
*/

  // serialize by memcpy
  uint8_t buf[SL_TOKEN_LENGTH];
  memcpy(buf, &foo, SL_TOKEN_LENGTH);

  // print buf as sanity check
  std::cout<<"buf is: "<<std::endl;
  phex(buf, SL_TOKEN_LENGTH);

  // Take pasted string and recreate struct
  // 2b7e151628aed2a6abf7158809cf4f3c000102030405060708090a0b0c0d0e0f0a01
  uint8_t str[80] = "2b7e151628aed2a6abf7158809cf4f3c0a00c064440003";
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
  std::cout<<"Mesh id is "<<int(bar.mesh_id)<<std::endl;
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
/*
  for (i = 0; i < size; i++)
    sscanf((char*)(str+i*2),"%2hhx", &buf[i]);
*/
}

// prints string as hex
void phex(uint8_t* str, unsigned int size)
{
    unsigned char i;
    for(i = 0; i < size; ++i)
        printf("%.2x", str[i]);
    printf("\n");
}

#if 0
void test_decrypt_cbc(void)
{
  // Example "simulating" a smaller buffer...

  uint8_t key[] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };
  uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
  uint8_t in[]  = { 0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46, 0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
                    0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee, 0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2,
                    0x73, 0xbe, 0xd6, 0xb8, 0xe3, 0xc1, 0x74, 0x3b, 0x71, 0x16, 0xe6, 0x9e, 0x22, 0x22, 0x95, 0x16,
                    0x3f, 0xf1, 0xca, 0xa1, 0x68, 0x1f, 0xac, 0x09, 0x12, 0x0e, 0xca, 0x30, 0x75, 0x86, 0xe1, 0xa7 };
  uint8_t out[] = { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
                    0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
                    0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
                    0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10 };
  uint8_t buffer[64];

  AES128_CBC_decrypt_buffer(buffer, in,  64, key, iv);
  /*
  AES128_CBC_decrypt_buffer(buffer+0, in+0,  16, key, iv);
  AES128_CBC_decrypt_buffer(buffer+16, in+16, 16, 0, 0);
  AES128_CBC_decrypt_buffer(buffer+32, in+32, 16, 0, 0);
  AES128_CBC_decrypt_buffer(buffer+48, in+48, 16, 0, 0);
  */
  printf("CBC decrypt: ");

  if(0 == memcmp((char*) out, (char*) buffer, 64))
  {
    printf("SUCCESS!\n");
  }
  else
  {
    printf("FAILURE!\n");
  }
}

void test_encrypt_cbc(void)
{
  uint8_t key[] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };
  uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
  uint8_t in[]  = { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
                    0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
                    0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
                    0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10 };
  uint8_t out[] = { 0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46, 0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
                    0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee, 0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2,
                    0x73, 0xbe, 0xd6, 0xb8, 0xe3, 0xc1, 0x74, 0x3b, 0x71, 0x16, 0xe6, 0x9e, 0x22, 0x22, 0x95, 0x16,
                    0x3f, 0xf1, 0xca, 0xa1, 0x68, 0x1f, 0xac, 0x09, 0x12, 0x0e, 0xca, 0x30, 0x75, 0x86, 0xe1, 0xa7 };
  uint8_t buffer[64];

  AES128_CBC_encrypt_buffer(buffer, in, 64, key, iv);

  printf("CBC encrypt: ");

  if(0 == memcmp((char*) out, (char*) buffer, 64))
  {
    printf("SUCCESS!\n");
  }
  else
  {
    printf("FAILURE!\n");
  }
}

*/
/*
void encrypt(unsigned char *Data) {
  std::cout<<"in function encrypt..."<<std::endl;
  std::cout<<Data<<std::endl;
}

int main() {
  unsigned char data[80] = "Hello World";
  encrypt(data);
  return 0;
}
*/

/*
void phex(uint8_t* str);

int main() {

  unsigned char key[16] =  { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
  unsigned char data[16] = "012345678901234";
  std::cout<<sizeof(data)<<std::endl;
  std::cout<<data<<std::endl;


  // print the resulting cipher as 4 x 16 byte strings
  std::cout<<"ciphertext:\n";

  uint8_t len = sizeof(data);
  uint8_t num_blocks = (len+15)/16;
  std::cout<<"Num of blocks are "<<std::endl;
  std::cout<<(unsigned) num_blocks<<std::endl;
  unsigned char* buf = (unsigned char*) malloc(num_blocks*16);
  memset(buf + len, 0, num_blocks*16-len);
  memcpy(buf, data, len);
  std::cout<<"buf is "<<buf<<std::endl;

  int i;
  std::cout<<"phex of blocks"<<std::endl;
  for (i = 0; i < num_blocks; ++i) {
    phex(buf + (i*16));
  }
  std::cout<<"phex of encrypted"<<std::endl;
  for(i = 0; i < num_blocks; ++i) {
      AES_Encrypt(data+i*16, key);
      phex(data + (i*16));
    }
  std::cout<<"buf is "<<data<<std::endl;

  std::cout<<'\n';
  std::cout<<"phex of decrypted"<<std::endl;
  for(i = 0; i < num_blocks; ++i) {
    AES_Encrypt( data+i*16, key);
    phex(data + (i*16));
  }
  std::cout<<"buf is "<<data<<std::endl;
  std::cout<<'\n';

  return 0;
}

// prints string as hex
void phex(uint8_t* str) {
  unsigned char i;
  for(i = 0; i < 16; ++i)
    std::cout<<str[i];
  std::cout<<'\n';
}

*/

#endif
