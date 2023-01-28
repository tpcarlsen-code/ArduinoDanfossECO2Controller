#ifndef _CRYPT_H_
#define _CRYPT_H_

#include <stdint.h>

void decrypt(uint8_t *in, int inLength, const char *key);
void encrypt(uint8_t *in, int inLength, const char *key);

#endif