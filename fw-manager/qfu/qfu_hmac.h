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

#ifndef __QFU_HMAC_H__
#define __QFU_HMAC_H__

#include "qfu_format.h"

/**
 * The size of the constant portion of the QFU HMAC extended-header.
 */
#define QFU_HMAC_FIXED_SIZE (sizeof(qfu_hdr_hmac_t) + sizeof(sha256_t))

/**
 * The maximum size of the HMAC extended header.
 *
 * That is the size of the fixed part of the header (including the final HMAC
 * signature) + the maximum size of the array of block hashes (i.e., the size
 * of a SHA256 hash times the maximum number of image blocks; the max number of
 * image blocks is given by the maximum number of pages in a partition divided
 * by number of pages per block).
 */
#define QFU_HMAC_HDR_MAX_SIZE                                                  \
	(QFU_HMAC_FIXED_SIZE +                                                 \
	 (sizeof(sha256_t) * (BL_PARTITION_MAX_PAGES / QFU_BLOCK_SIZE_PAGES)))

/**
 * Check validity of the QFU HMAC header.
 *
 * Authenticate the entire QFU header using the HMAC signature in the HMAC
 * extended header and check that the image Security Version Number (SVN) is
 * >= than the SVN stored in BL-Data.
 *
 * @param[in] qfu_hdr A pointer to the entire QFU header. Must not be null.
 * @param[in] n_data_blocks The number of data blocks in the image.
 * @param[in] part The partition being updated. Must not be null.
 *
 * @return 0 if header is valid, nonzero value otherwise.
 */
int qfu_hmac_check_hdr(const qfu_hdr_t *qfu_hdr, uint16_t n_data_blocks,
		       const bl_flash_partition_t *part);

/**
 * Check validity of a data block.
 *
 * @param[in] data A pointer to the data block. Must not be null.
 * @param[in] len  The length of the data block.
 * @param[in] qfu_hdr A pointer to the entire QFU header. Must not be null.
 * @param[in] blk_num The index of the data block.
 *
 * @return 0 if block is valid, nonzero value otherwise.
 */
int qfu_hmac_check_block_hash(const uint8_t *data, uint32_t len,
			      const qfu_hdr_t *qfu_hdr, uint32_t data_blk_num);

#endif /* __QFU_HMAC_H__ */
