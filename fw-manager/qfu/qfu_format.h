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

#ifndef __QFU_FORMAT_H__
#define __QFU_FORMAT_H__

#include <stdint.h>

#include "bl_data.h"

/* QFU_HDR_MAGIC = "QFUH" */
#define QFU_HDR_MAGIC (0x48554651)

/**
 * The enumeration of possible authentication mechanism.
 */
typedef enum {
	QFU_EXT_HDR_NONE = 0,    /**< No authentication. */
	QFU_EXT_HDR_SHA256 = 1,  /**< SHA256 verification extended header. */
	QFU_EXT_HDR_HMAC256 = 2, /**< HMAC256 authentication extended header. */
} qfu_auth_type_t;

/**
 * The structure of the QFU header.
 *
 * The QFU base header can be followed by an extended header whose size must be
 * a multiple of 32 bits.
 */
typedef struct __attribute__((__packed__)) {
	uint32_t magic;	/**< Header magic: 'QFUH'. */
	uint16_t vid;	  /**< Target Vendor ID. */
	uint16_t pid;	  /**< Target Product ID. */
	uint16_t pid_dfu;      /**< Target Product ID when in DFU mode. */
	uint16_t partition;    /**< Target partition ID. */
	uint32_t version;      /**< Firmware version. */
	uint16_t block_sz;     /**< Block size. */
	uint16_t n_blocks;     /**< Total number of blocks; incl. the header. */
	uint16_t ext_hdr_type; /**< Type of extended-header. */
	uint16_t rsvd;	 /**< Reserved. */
	uint32_t ext_hdr[];    /**< Pointer to the extended header. */
} qfu_hdr_t;

/**
 * The structure of the QFU SHA256 extended header.
 */
typedef struct __attribute__((__packed__)) {
	sha256_t img_digest; /**< Image SHA256 hash. */
} qfu_hdr_sha_t;

/**
 * The structure of the QFU HMAC256 extended header.
 *
 * +------------------------------+
 * |             svn              |
 * +------------------------------+
 * |         blk_sha256[0]        |
 * +------------------------------+
 * |         blk_sha256[1]        |
 * +------------------------------+
 * |             ....             |
 * +------------------------------+
 * | blk_sha256[num_data_blk - 1] |
 * +------------------------------+
 * |            hmac256           |
 * +------------------------------+
 */
typedef struct __attribute__((__packed__)) {
	/** Security version number of the image. */
	uint32_t svn;
	/**
	 * Variable-length array of hashes.
	 *
	 * The array contains one SHA256 hash for each data block plus the
	 * final HMAC256 signature of the entire QFU header (base header +
	 * extended header).
	 */
	sha256_t hashes[];
} qfu_hdr_hmac_t;

#endif /* __QFU_FORMAT_H__ */
