/**********************************************************\
|                                                          
| xxtea.h                                                  
|                                                          
| XXTEA encryption algorithm library for C.                
|                                                          
| Encryption Algorithm Authors:                            
|      David J. Wheeler                                    
|      Roger M. Needham                                    
|                                                          
| Code Authors: Chen fei <cf850118@163.com>                
|               Ma Bingyao <mabingyao@gmail.com>           
| LastModified: Mar 3, 2015                                
|
| The MIT License (MIT)
| 
| Copyright (c) 2008-2016 Ma Bingyao mabingyao@gmail.com
|
| Permission is hereby granted, free of charge, to any 
| person obtaining a copy of this software and associated 
| documentation files (the "Software"), to deal in the 
| Software without restriction, including without 
| limitation the rights to use, copy, modify, merge, 
| publish, distribute, sublicense, and/or sell copies of 
| the Software, and to permit persons to whom the Software
| is furnished to do so, subject to the following 
| conditions:
| 
| The above copyright notice and this permission notice 
| shall be included in all copies or substantial 
| portions of the Software.
| 
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
| ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
| TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
| PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT 
| SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR 
| ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
| ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT 
| OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
| OTHER DEALINGS IN THE SOFTWARE.
|                                                          
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
