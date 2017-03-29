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
#include <stddef.h>

#include "fw-manager_comm.h"

#include "fw-manager_utils.h"
#include "bl_data.h"
#include "boot_clk.h"
#include "rom_version.h"

#define BL_DATA_BLANK_VALUE ((uint32_t)0xFFFFFFFF)

#if (UNIT_TEST)
/* Test variable simulating the 2-page BL-Data section in flash. */
uint8_t test_bl_data_pages[QM_FLASH_PAGE_SIZE_BYTES * 2];

static const bl_data_t *const bl_data_main =
    (bl_data_t *)&test_bl_data_pages[0];
static const bl_data_t *const bl_data_bck =
    (bl_data_t *)&test_bl_data_pages[QM_FLASH_PAGE_SIZE_BYTES];
uint8_t test_num_loops;
#define FOREVER() --test_num_loops
#define BL_DATA_SECTION_START ((uint32_t *)test_bl_data_pages)
#define BL_DATA_SECTION_END ((uint32_t *)(test_bl_data_pages + 1))
#else
/* Pointer to BL-Data Main in flash. */
static const bl_data_t *const bl_data_main =
    (bl_data_t *)BL_DATA_SECTION_MAIN_ADDR;
/* Pointer to BL-Data Backup in flash. */
static const bl_data_t *const bl_data_bck =
    (bl_data_t *)BL_DATA_SECTION_BACKUP_ADDR;
/* The start address of the BL-Data section in flash. */
#define BL_DATA_SECTION_START                                                  \
	((uint32_t *)(BL_DATA_FLASH_REGION_BASE +                              \
		      (BL_DATA_SECTION_BASE_PAGE * QM_FLASH_PAGE_SIZE_BYTES)))
/* The end address of the BL-Data section in flash. */
#define BL_DATA_SECTION_END                                                    \
	((uint32_t *)(BL_DATA_FLASH_REGION_BASE +                              \
		      ((BL_DATA_SECTION_BASE_PAGE + BL_DATA_SECTION_PAGES) *   \
		       QM_FLASH_PAGE_SIZE_BYTES)))
#define FOREVER() (1)
#endif

/* The initialization values for the target descriptor array in BL-data. */
static const bl_boot_target_t targets_defaults[BL_BOOT_TARGETS_NUM] =
    BL_TARGET_LIST;
/* The initialization values for the partition descriptor array in BL-data. */
static const bl_flash_partition_t partitions_defaults[BL_FLASH_PARTITIONS_NUM] =
    BL_PARTITION_LIST;

/* The RAM-copy of BL-Data. */
static bl_data_t bl_data_shadow;

/* Pointer to the RAM-copy of the bootloader data (BL-Data). */
bl_data_t *const bl_data = &bl_data_shadow;

/**
 * Initialize BL-Data.
 *
 * Both the RAM copy and the flash copies of BL-Data are initialized. As part
 * of the initialization process, trim codes are computed.
 */
static void bl_data_init(void)
{
	/* Trim codes computation. */
	boot_clk_trim_code_compute(&bl_data->trim_codes);
	/* Store ROM version in BL-Data. */
	bl_data->rom_version = QM_VER_ROM;
	/* Initialize target and partition descriptor lists. */
	memcpy(&bl_data->targets, &targets_defaults, sizeof(bl_data->targets));
	memcpy(&bl_data->partitions, &partitions_defaults,
	       sizeof(bl_data->partitions));
	/* Save BL-Data to flash. */
	bl_data_shadow_writeback();
}

/**
 * Copy the BL-Data struct passed as input to a specific flash page.
 *
 * @param[in] A pointer to the BL-Data to be saved in flash. Must not be null.
 * @patam[in] The flash page where to save the BL-Data.
 */
static void bl_data_copy(const bl_data_t *data, int bl_page)
{
	qm_flash_page_write(BL_DATA_FLASH_CONTROLLER, BL_DATA_FLASH_REGION,
			    bl_page, (uint32_t *)data,
			    sizeof(bl_data_t) / sizeof(uint32_t));
}

/**
 * Erase partition.
 *
 * Erase all the pages of an application partition.
 *
 * @param[in] part The partition to be erased. Must not be null.
 */
static void bl_data_erase_partition(const bl_flash_partition_t *part)
{
	uint32_t page;
	qm_flash_reg_t *flash_regs;

	for (page = part->first_page;
	     page < (part->first_page + part->num_pages); page++) {
		qm_flash_page_erase(part->controller, QM_FLASH_REGION_SYS,
				    page);
	}
	/* Flash content has changed, flush prefetch buffer. */
	flash_regs = QM_FLASH[part->controller];
	flash_regs->ctrl |= QM_FLASH_CTRL_PRE_FLUSH_MASK;
	flash_regs->ctrl &= ~QM_FLASH_CTRL_PRE_FLUSH_MASK;
}

/**
 * Sanitize application flash partitions.
 *
 * Check and fix inconsistent partitions. Fixing consists in erasing the entire
 * partition and marking it back as consistent.
 *
 * @note Empty partitions are not booted, even if marked as consistent.
 *
 * @return Whether or not a writeback of bl-data is needed.
 * @retval false Writeback not needed (no partition has been fixed and bl-data
 * 		 has not been updated).
 * @retval true  Writeback needed (at least one partition has been fixed and
 *		 therefore bl-data has been updated).
 */
static bool bl_data_sanitize_partitions(void)
{
	int wb_needed;
	int i;

	wb_needed = false;
	bl_flash_partition_t *part;

	for (i = 0; i < BL_FLASH_PARTITIONS_NUM; i++) {
		part = &bl_data->partitions[i];
		if (part->is_consistent == false) {
			bl_data_erase_partition(part);
			part->is_consistent = true;
			wb_needed = true;
		}
	}

	return wb_needed;
}

/**
 * Loop infinitely if BL-Data flash section is not entirely blank.
 *
 * Check that the entire BL-Data flash section (i.e., both BL-Data Main page and
 * BL-Data Backup page) is blank.
 *
 * If the check fails, this function never returns and execution is stopped.
 */
static void bl_loop_if_not_blank(void)
{
	const uint32_t *ptr;

	for (ptr = BL_DATA_SECTION_START; ptr < BL_DATA_SECTION_END; ptr++) {
		if (*ptr != BL_DATA_BLANK_VALUE) {
			/*
			 * Check has not succeeded: as the device could be
			 * compromised, execution is stopped.
			 */
			qm_irq_disable();
			while (FOREVER()) {
			}
		}
	}
}

/*
 * Check the validity of BL-Data and fix/init it if necessary.
 *
 * The logic of this functions is defined in conjunction with the firmware
 * image update logic (see QFU module).
 */
int bl_data_sanitize(void)
{
	const uint32_t crc_main =
	    fm_crc16_ccitt((uint8_t *)bl_data_main, offsetof(bl_data_t, crc));
	const uint32_t crc_bck =
	    fm_crc16_ccitt((uint8_t *)bl_data_bck, offsetof(bl_data_t, crc));

	if (bl_data_main->crc != crc_main) {
		if (bl_data_bck->crc != crc_bck) {
			/*
			 * Both BL-Data Main and BL-Data Backup are invalid.
			 * This is expected when the BL-Data flash section has
			 * not been initialized yet. We expect the entire
			 * BL-Data flash section (i.e., the entire two pages)
			 * to be blank: if not, the following function call
			 * never returns.
			 */
			bl_loop_if_not_blank();
			/*
			 * Perform initial device provisioning:
			 * initialize the BL-Data section in flash and the
			 * RAM-copy of BL-Data.
			 */
			bl_data_init();
		} else {
			/*
			 * BL-Main is corrupted. This can happen when a
			 * previous firmware image upgrade failed while
			 * updating BL-Data Main.
			 *
			 * Restore BL-Data Main by copying the content of
			 * BL-Data Backup over it.
			 */
			bl_data_copy(bl_data_bck, BL_DATA_SECTION_MAIN_PAGE);
		}
	} else if (memcmp(bl_data_main, bl_data_bck, sizeof(bl_data_t))) {
		/*
		 * BL-Data Main is valid and up to date, but BL-Data Backup
		 * has a different content than BL-Data Main. This means
		 * that BL-Data Backup is either corrupted (expected when
		 * the previous firmware update has failed while updating
		 * BL-Data backup) or outdated (expected when the previous
		 * firmware update has failed after updating BL-Data Main,
		 * but before beginning to update BL-Data Backup).
		 *
		 * Restore BL-Data Backup with the content of BL-Data Main.
		 */
		bl_data_copy(bl_data_main, BL_DATA_SECTION_BACKUP_PAGE);
	}
	/*
	 * Update the shadowed BL-Data in RAM with the content of BL-Data Main
	 */
	memcpy(bl_data, bl_data_main, sizeof(*bl_data));
	/*
	 * Now that BL-Data is consistent, we can sanitize partitions.
	 *
	 * Note: if any partition is sanitized, shadowed bl-data is updated and
	 * need to be written back.
	 */
	if (bl_data_sanitize_partitions()) {
		bl_data_shadow_writeback();
	}

	return 0;
}

/*
 * Store BL-Data to flash.
 *
 * The RAM-copy of BL-Data is written back to flash, on both pages: BL-Data
 * Main first, BL-Data backup then.
 */
int bl_data_shadow_writeback(void)
{
	bl_data->crc =
	    fm_crc16_ccitt((uint8_t *)bl_data, offsetof(bl_data_t, crc));
	bl_data_copy(bl_data, BL_DATA_SECTION_MAIN_PAGE);
	bl_data_copy(bl_data, BL_DATA_SECTION_BACKUP_PAGE);

	return 0;
}
