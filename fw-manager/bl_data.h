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

#ifndef __BL_DATA_H__
#define __BL_DATA_H__

#include "qm_flash.h"
#include "soc_flash_partitions.h"

/**
 * Bootloader data structures.
 *
 * This files contains the definition of the structures into which the
 * bootloader (meta-)data is organized.
 *
 * @defgroup groupBL_Data BL Data
 * @{
 */

/*
 * Bootloader data is expected to be stored in a flash section called Bootloader
 * Data Section. Such a section is composed of two pages, each one containing a
 * copy of the bootloader data: one is the main copy and the other one is the
 * backup copy.
 *
 * The backup copy in a different page is necessary in order to recover from
 * power loss during updates, which may cause the corruption of an entire page.
 *
 * The general structure of each copy of bootloader data is the following (see
 * bl_data struct):
 * ----------------------------------
 * |       Shadowed trim codes      | --> qm_flash_data_trim struct
 * ----------------------------------
 * |           ROM version          | --> ROM Version
 * ----------------------------------
 * |        Protection gap          | --> Gap to align not-to-be-protected data
 * |                                |     to 1KB and protect the rest with FPR
 * ----------------------------------
 * | Array of partition descriptors | --> bl_flash_partition struct
 * ----------------------------------
 * |  Array of target descriptors   | --> bl_bool_target struct
 * ----------------------------------
 * |          Firmware key          | --> Firmware Key
 * ----------------------------------
 * |         Revocation Key         | --> Revocation Key
 * ----------------------------------
 * |              CRC               | --> CRC of the previous fields
 * ----------------------------------
 */

/** Number or partitions. */
#if (BL_CONFIG_DUAL_BANK)
/* 2 partitions per target. */
#define BL_FLASH_PARTITIONS_NUM (BL_BOOT_TARGETS_NUM * 2)
#else
/* 1 partition per target. */
#define BL_FLASH_PARTITIONS_NUM (BL_BOOT_TARGETS_NUM * 1)
#endif

/** The page where BL-Data main copy is located. */
#define BL_DATA_SECTION_MAIN_PAGE (BL_DATA_SECTION_BASE_PAGE)
/** The page where BL-Data backup copy is located. */
#define BL_DATA_SECTION_BACKUP_PAGE (BL_DATA_SECTION_BASE_PAGE + 1)

/** The address where BL-Data main copy is located. */
#define BL_DATA_SECTION_MAIN_ADDR                                              \
	(BL_DATA_FLASH_REGION_BASE +                                           \
	 (BL_DATA_SECTION_MAIN_PAGE * QM_FLASH_PAGE_SIZE_BYTES))

/** The address where BL-Data backup copy is located. */
#define BL_DATA_SECTION_BACKUP_ADDR                                            \
	(BL_DATA_FLASH_REGION_BASE +                                           \
	 (BL_DATA_SECTION_BACKUP_PAGE * QM_FLASH_PAGE_SIZE_BYTES))

/** The type of a target descriptor. */
typedef struct bl_boot_target bl_boot_target_t;
/** The type of a partition descriptor. */
typedef struct bl_flash_partition bl_flash_partition_t;

/**
 * The structure of a SHA256 hash.
 *
 * Essentially, an array of 32 bytes.
 */
typedef union {
	uint8_t u8[32];
	uint32_t u32[8];
} sha256_t;

/** The type of HMAC keys used for authentication (i.e., 32-byte keys). */
typedef sha256_t hmac_key_t;

/**
 * The Boot Target Descriptor structure.
 *
 * A boot target is a core capable of running code in a flash partition.
 */
struct bl_boot_target {
	/** The index of the active partition for this target. */
	uint32_t active_partition_idx;

	/**
	 * The Security Version Number (SVN) associated with this target.
	 *
	 * Partitions associated with this target can be updated only with QFU
	 * images having an SVN >= of the target SVN. If the image's SVN is
	 * greater then the target's SVN, the target's SVN is updated to the
	 * image's SVN after the update succeeds.
	 */
	uint32_t svn;
};

/**
 * The Flash Partition Descriptor structure.
 *
 * A flash partition is a portion of the flash reserved for containing
 * application code for a specific target.
 */
struct bl_flash_partition {
	/** The index of target associated with the partition. */
	const uint32_t target_idx;
	/** The flash controller where the partition is hosted. */
	const qm_flash_t controller;
	/** The page number where the partition starts. */
	const uint32_t first_page;
	/** The size (in pages) of the partition. */
	const uint32_t num_pages;

	/**
	 * Application entry point address for the partition.
	 *
	 * Note: the value of this filed may be computed at run-time (deriving
	 * it from the 'controller' and 'first_page' values), but that will
	 * increase bootloader code size, so we prefer to store it in BL-Data
	 * directly.
	 */
	volatile const uint32_t *const start_addr;

	/* Variable fields */
	/** Consistency flag: used to mark partition going to be updated. */
	uint32_t is_consistent;
	/** The version of the application installed in the partition. */
	uint32_t app_version;
};

/**
 * The Bootloader Data structure.
 *
 * This structure defines how the bootloader data stored in flash is organized.
 */
typedef struct bl_data {
	/** Shadowed trim codes. */
	qm_flash_data_trim_t trim_codes;
	/** Shadowed ROM version. */
	uint32_t rom_version;

	/**
	 * Padding for FPR alignment.
	 *
	 * trim_codes and rom_version will be available for apps, while the rest
	 * of the information won't. As the FPR protection is per 1KB block, we
	 * need a gap between unprotected and protected data, so each part is
	 * aligned with 1KB.
	 */
	uint8_t fpr_alignment[QM_FPR_GRANULARITY -
			      sizeof(qm_flash_data_trim_t) - sizeof(uint32_t)];
	/** The list of flash partition descriptors. */
	bl_flash_partition_t partitions[BL_FLASH_PARTITIONS_NUM];
	/** The list of boot target descriptors. */
	bl_boot_target_t targets[BL_BOOT_TARGETS_NUM];
	/** The current fw key. */
	hmac_key_t fw_key;
	/** The current revocation key. */
	hmac_key_t rv_key;
	/** The CRC of the previous fields. */
	uint32_t crc;
} bl_data_t;

/**
 * The pointer to the RAM (shadowed) copy of the bootloader data.
 */
extern bl_data_t *const bl_data;

/**
 * Check validity of BL-Data and fix it if necessary.
 *
 * Check whether the BL-Data Section is valid and, if not, fix it. Fixing the
 * BL-Data section may consists in:
 * - Initializing it, if both BL-Data copy are invalid / missing and BL-Data
 *   flash section is empty (i.e., entirely 0xFF);
 * - Copying BL-Data Main over BL-Data Backup, if BL-Data Backup is invalid;
 * - Copying BL-Data Backup over BL-Data Main, if BL-Data Main is invalid;
 * - Entering an infinite loop if both copies are invalid and BL-Data flash
 *   section is not empty (meaning that a hardware fault or security attack has
 *   happened).
 *
 * Note: the initialization include SoC specific data (e.g., trim codes are
 * computed and stored).
 *
 * @return 0 on success, negative errno otherwise.
 */
int bl_data_sanitize(void);

/**
 * Store shadowed BL-Data to flash, over both BL-Data Main and BL-Data Backup.
 *
 * Store the RAM copy of BL-Data to flash, replacing both the main and the
 * backup copy on flash.
 *
 * @return 0 on success, negative errno otherwise.
 */
int bl_data_shadow_writeback(void);

/**
 * }@
 */

#endif /* __BL_DATA_H__ */
