/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the Intel Corporation nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL CORPORATION OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __FW_MANAGER_UTILS_H__
#define __FW_MANAGER_UTILS_H__

#include <stdint.h>
#include <stdbool.h>

#include "tinycrypt/hmac.h"

#include "bl_data.h"

/**
 *  Compute CRC.
 *
 *  This function computes the CRC16-CCITT of the payload, which means:
 *  - with CRC initialized to 0xffff
 *  - with 16 'zero' bits appended to the end of the message
 *  - 0x1021 for the polynomial
 *
 * @param[in] data Input buffer which will be used to calculate the checksum.
 * 		   Must not be null.
 * @param[in] len  The size of the buffer.
 *
 *  @return Computed CRC value.
 */
uint16_t fm_crc16_ccitt(const uint8_t *data, int len);

/**
 * Check if an FM HMAC key has the default value (i.e., 0x00).
 *
 * @param[in] key The key to check. Must not be null.
 *
 * @retval true  If the key is the default one.
 * @retval false If the key is not the default one.
 */
bool fm_hmac_is_default_key(const hmac_key_t *key);

/**
 * Compute the HMAC for some input data.
 *
 * @param[in]  data     A pointer to the input data. Must not be null.
 * @param[in]  data_len The length of the input data.
 * @param[in]  key      The key to be used to compute the HMAC. Must not be
 * 			null.
 * @param[out] hmac     The buffer where to store the resulting HMAC digest.
 * 			Must not be null.
 */
void fm_hmac_compute_hmac(const void *data, size_t data_len,
			  const hmac_key_t *key, sha256_t *hmac);

#endif /* __FW_MANAGER_UTILS_H__ */
