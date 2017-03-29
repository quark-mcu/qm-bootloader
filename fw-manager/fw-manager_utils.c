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

#include <string.h>

#include "fw-manager_utils.h"

/*  Compute 16 bits CCITT CRC, with 0x1021 as polynomial */
uint16_t fm_crc16_ccitt(const uint8_t *pdata, int len)
{
	uint32_t data, crc = 0;

	while (len--) {
		data = crc ^ ((int)*pdata++) << 8;
		data = (data >> 12) ^ data >> 8;
		data ^= (data << 5) ^ (data << 12);
		crc = ((crc << 8) ^ data) & 0xffff;
	}

	return (uint16_t)crc;
}

#if (ENABLE_FIRMWARE_MANAGER_AUTH)
/* Check if passed key is equal to the default key. */
bool fm_hmac_is_default_key(const hmac_key_t *key)
{
	unsigned int i;

	/*
	 * All the bytes of a default key are 0x00, so as soon as we find a byte
	 * different from 0x00, we can say that the key is not the default one.
	 */
	for (i = 0; i < (sizeof(*key) / sizeof(uint32_t)); i++) {
		if (key->u32[i] != 0) {
			return false;
		}
	}

	return true;
}

/* Compute the HMAC of the data passed as input. */
void fm_hmac_compute_hmac(const void *data, size_t data_size,
			  const hmac_key_t *key, sha256_t *hmac_digest)
{
	static struct tc_hmac_state_struct ctx;

	/*
	 * NOTE: we don't memset 'ctx' anymore since it is zeroed already
	 * by tc_hmac_final() from TinyCrypt.
	 */

	tc_hmac_set_key(&ctx, key->u8, sizeof(hmac_key_t));
	tc_hmac_init(&ctx);
	tc_hmac_update(&ctx, data, data_size);
	tc_hmac_final(hmac_digest->u8, sizeof(*hmac_digest), &ctx);
}
#endif
