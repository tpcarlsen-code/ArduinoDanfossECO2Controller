#include "crypt.h"
#include <stdint.h>
#include <Arduino.h>
#include "xxtea.h"

uint8_t hexToUint(char hex)
{
  if (hex >= 48 && hex <= 57)
  {
    return (uint8_t)hex - 48;
  }
  // value is a-f
  return hex - 87;
}

uint8_t twoHexToUint(char hex1, char hex2)
{
  return hexToUint(hex1) * 16 + hexToUint(hex2);
}

void hexStringToUint(const char *hex, uint8_t *res)
{
  for (int i = 0; i < strlen(hex); i += 2)
  {
    res[i / 2] = twoHexToUint(hex[i], hex[i + 1]);
  }
}

void formatHex(uint8_t *in, int lenIn, char *out)
{
  char f[2];
  for (int i = 0; i < lenIn; i++)
  {
    sprintf(f, "%02X", in[i]);
    out[i * 2] = f[0];
    out[i * 2 + 1] = f[1];
  }
}

void reverse_chunks(uint8_t *data, int len)
{
  uint8_t temp[4];
  for (int i = 0; i < len; i += 4)
  {
    temp[0] = data[i + 3];
    temp[1] = data[i + 2];
    temp[2] = data[i + 1];
    temp[3] = data[i];
    data[i] = temp[0];
    data[i + 1] = temp[1];
    data[i + 2] = temp[2];
    data[i + 3] = temp[3];
  }
}

void decrypt(uint8_t *in, int inLength, const char *key)
{
  reverse_chunks(in, inLength);

  uint8_t keyBytes[16];
  hexStringToUint(key, keyBytes);

  size_t s = 0;
  uint8_t *result;
  result = (uint8_t *)xxtea_decrypt(in, inLength, keyBytes, &s);
  reverse_chunks(result, inLength);
  memcpy((void *)in, (void *)result, inLength);
}

void encrypt(uint8_t *in, int inLength, const char *key)
{
  reverse_chunks(in, inLength);

  uint8_t keyBytes[16];
  hexStringToUint(key, keyBytes);

  size_t s = 0;
  uint8_t *result;
  result = (uint8_t *)xxtea_encrypt(in, inLength, keyBytes, &s);
  reverse_chunks(result, inLength);
  memcpy((void *)in, (void *)result, inLength);
}
