/**********************************************************\
|                                                          |
| xxtea.h                                                  |
|                                                          |
| XXTEA encryption algorithm library for C.                |
|                                                          |
| Encryption Algorithm Authors:                            |
|      David J. Wheeler                                    |
|      Roger M. Needham                                    |
|                                                          |
| Code Authors: Chen fei <cf850118@163.com>                |
|               Ma Bingyao <mabingyao@gmail.com>           |
| LastModified: Mar 3, 2015                                |
|                                                          |
\**********************************************************/

#ifndef XXTEA_INCLUDED
#define XXTEA_INCLUDED

#include <stdlib.h>
#include <inttypes.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Function: xxtea_encrypt
 * @data:    Data to be encrypted
 * @len:     Length of the data to be encrypted
 * @key:     Symmetric key
 * @out_len: Pointer to output length variable
 * Returns:  Encrypted data or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
void * xxtea_encrypt(const void * data, size_t len, const void * key, size_t * out_len);

/**
 * Function: xxtea_decrypt
 * @data:    Data to be decrypted
 * @len:     Length of the data to be decrypted
 * @key:     Symmetric key
 * @out_len: Pointer to output length variable
 * Returns:  Decrypted data or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
void * xxtea_decrypt(const void * data, size_t len, const void * key, size_t * out_len);

static uint8_t * xxtea_ubyte_decrypt(const uint8_t * data, size_t len, const uint8_t * key, size_t * out_len);

void xxtea_init();

#ifdef __cplusplus
}
#endif

#endif
