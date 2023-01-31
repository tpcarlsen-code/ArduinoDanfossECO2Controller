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

void hexStringToUint(const char *hex, uint8_t *res, int hexLen)
{
  for (int i = 0; i < hexLen; i += 2)
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
  uint8_t temp[64];
  for (int i = 0; i < len; i += 4)
  {
    temp[i] = data[i + 3];
    temp[i + 1] = data[i + 2];
    temp[i + 2] = data[i + 1];
    temp[i + 3] = data[i];
    data[i] = temp[i];
    data[i + 1] = temp[i + 1];
    data[i + 2] = temp[i + 2];
    data[i + 3] = temp[i + 3];
  }
}

// Used by the decrypt/encrypt functions to hold key data.
//uint8_t *
// If it stops working this is why.

void decrypt(uint8_t *in, int inLength, const char *key)
{
  reverse_chunks(in, inLength);

  uint8_t keyBytes[16];
  hexStringToUint(key, keyBytes, 32);

  size_t s = 0;
  uint8_t *result;
  result = (uint8_t *)xxtea_decrypt(in, inLength, keyBytes, &s);
  reverse_chunks(result, inLength);
  for (int i = 0; i < inLength; i++)
  {
    in[i] = result[i];
  }
}

void encrypt(uint8_t *in, int inLength, const char *key)
{
  reverse_chunks(in, inLength);
  
  uint8_t keyBytes[16];
  hexStringToUint(key, keyBytes, 32);

  size_t s = 0;
  uint8_t *result;
  result = (uint8_t *)xxtea_encrypt(in, inLength, keyBytes, &s);
  reverse_chunks(result, inLength);
  for (int i = 0; i < inLength; i++)
  {
    in[i] = result[i];
  }
}
