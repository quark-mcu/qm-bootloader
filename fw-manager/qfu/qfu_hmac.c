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

#include "tinycrypt/hmac.h"

#include "qm_common.h"
#include "fw-manager_utils.h"
#include "qfu_hmac.h"
#include "bl_data.h"

#define DEBUG_MSG (0)
#if (DEBUG_MSG)
#define DBG_PRINTF(...) QM_PRINTF(__VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif

/* Validate HMAC extended header. */
int qfu_hmac_check_hdr(const qfu_hdr_t *qfu_hdr, uint16_t n_data_blocks,
		       const bl_flash_partition_t *part)
{
	sha256_t hmac_digest;
	int hdr_size;
	int retv;
	const qfu_hdr_hmac_t *hmac_hdr = (void *)qfu_hdr->ext_hdr;
	const int t_idx = part->target_idx;

	/*
	 * Check if device is provisioned (i.e., the authentication key is
	 * different from the default one).
	 */
	if (fm_hmac_is_default_key(&bl_data->fw_key)) {
		return -1;
	}
	/*
	 * Check if the Security Version Number (SVN) of the image is valid: it
	 * must be equal to or greater than the current SVN associated with the
	 * partition (more precisely, the target that this partition belongs
	 * to).
	 */
	if (hmac_hdr->svn < bl_data->targets[t_idx].svn) {
		return -2;
	}

	/*
	 * The header size on which we compute the HMAC is variable, due to
	 * the HMAC ext-header, which has an initial fixed-length part and a
	 * variable number of SHA256 digests (one for each data block).
	 */
	hdr_size = sizeof(*qfu_hdr) + sizeof(qfu_hdr_hmac_t) +
		   (sizeof(sha256_t) * (n_data_blocks));
	/* Compute HMAC and verify that the one in the header matches it. */
	fm_hmac_compute_hmac(qfu_hdr, hdr_size, &bl_data->fw_key, &hmac_digest);
	retv = memcmp(&hmac_digest, &hmac_hdr->hashes[n_data_blocks],
		      sizeof(sha256_t));

	return retv;
}

/* Validate image block. */
int qfu_hmac_check_block_hash(const uint8_t *data, uint32_t len,
			      const qfu_hdr_t *qfu_hdr, uint32_t data_blk_num)
{
	struct tc_sha256_state_struct ctx;
	sha256_t digest;
	const qfu_hdr_hmac_t *hmac_hdr = (void *)qfu_hdr->ext_hdr;
	const sha256_t *block_hash = &hmac_hdr->hashes[data_blk_num];

	tc_sha256_init(&ctx);
	tc_sha256_update(&ctx, data, len);
	tc_sha256_final(digest.u8, &ctx);

	return memcmp(&digest, block_hash, sizeof(sha256_t));
}
