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

#include "clk.h"
#include "qm_common.h"
#include "qm_flash.h"

#include "../bl_data.h"
#include "../dfu/dfu.h"
#include "bl_data.h"
#include "fw-manager_config.h"
#include "qfu_format.h"
#include "qfu_hmac.h"
#include "qm_interrupt.h"

/* Set DEBUG_MSG to 1 to enable debugging messages. */
#define DEBUG_MSG (0)
#if (DEBUG_MSG)
#define DBG_PRINTF(...) QM_PRINTF(__VA_ARGS__)
/* Replace calls to qm_flash with calls to the debugging printf. */
#define qm_flash_page_write(ctrl, reg, pg, data, len)                          \
	do {                                                                   \
		DBG_PRINTF("[SUPPRESSED] qm_flash_page_write()\n");            \
		(void)(pg);                                                    \
	} while (0);
#else
#define DBG_PRINTF(...)
#endif

/**
 * The size of the header buffer.
 *
 * It is equal to the size of the QFU base header plus the maximum size of the
 * extended header.
 */
#define HDR_BUF_SIZE (sizeof(qfu_hdr_t) + QFU_HMAC_HDR_MAX_SIZE)

/** Number of blocks used for header (always 1 with current block sizes). */
#define NUM_HDR_BLOCKS (1)

#if (ENABLE_FIRMWARE_MANAGER_AUTH)
/* When authentication is enabled, the extended header must be the HMAC one. */
#define QFU_EXPECTED_EXT_HDR (QFU_EXT_HDR_HMAC256)
/*
 * Macro to check if the extended header is valid.
 *
 * It is expected to return 0 on success (i.e., extended header is valid).
 *
 * When authentication is enabled, this macro calls the function for verifying
 * the HMAC extended header.
 */
#define qfu_check_ext_hdr(img_hdr, data_blocks, part)                          \
	qfu_hmac_check_hdr(img_hdr, data_blocks, part)
#else
/* When authentication is not enabled, no extended header is allowed. */
#define QFU_EXPECTED_EXT_HDR (QFU_EXT_HDR_NONE)
/*
 * Macro to check if the extended header is valid.
 *
 * It is expected to return 0 on success (i.e., extended header is valid).
 *
 * When authentication is disabled, the check is always successful.
 */
#define qfu_check_ext_hdr(img_hdr, data_blocks, part) (0)
#endif /* ENABLE_FIRMWARE_MANAGER_AUTH */

/*-----------------------------------------------------------------------*/
/* FORWARD DECLARATIONS                                                  */
/*-----------------------------------------------------------------------*/
static void qfu_init(uint8_t alt_setting);
static void qfu_get_status(dfu_dev_status_t *status, uint32_t *poll_timeout_ms);
static void qfu_clear_status(void);
static void qfu_dnl_process_block(uint32_t block_num, const uint8_t *data,
				  uint16_t len);
static int qfu_dnl_finalize_transfer(uint32_t block_num);
static void qfu_upl_fill_block(uint32_t block_num, uint8_t *data,
			       uint16_t max_len, uint16_t *len);
static void qfu_abort_transfer(void);

/*-----------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                      */
/*-----------------------------------------------------------------------*/
/**
 * QFU request handler variable.
 *
 * This DFU request handler is used by DFU core when an alternate setting
 * different from 0 is selected.
 */
const dfu_request_handler_t qfu_dfu_rh = {
    &qfu_init, &qfu_get_status, &qfu_clear_status, &qfu_dnl_process_block,
    &qfu_dnl_finalize_transfer, &qfu_upl_fill_block, &qfu_abort_transfer,
};

/** The DFU (error) status of this DFU request handler. */
static dfu_dev_status_t qfu_err_status;

/** The partition associated with the current alternate setting. */
static bl_flash_partition_t *part;
/** The current alternate setting; needed to verify the QFU header. */
static uint8_t active_alt_setting;

/** The buffer where we store the full QFU header (base one + extended one). */
static uint8_t hdr_buf[HDR_BUF_SIZE];

/** The header of the QFU image being processed. */
static qfu_hdr_t *const img_hdr = (void *)&hdr_buf;

/** The buffer where we store the QFU block being processed. */
/*
 * NOTE: this buffer is introduced to simplify the handling of the last block,
 * which may be smaller than QFU_BLOCK_SIZE and not a multiple of 4 bytes (look
 * at qfu_handle_blk() for more details); however, if RAM usage becomes a
 * problem, it can be removed, reusing the qda_buf or usb_buf in some ugly way.
 */
static uint8_t blk_buf[QFU_BLOCK_SIZE];

/**
 * Prepare BL-Data Section to firmware update.
 *
 * Mark the partition that is going to be updated as inconsistent, so that if
 * the upgrade fails, the partition will be erased during BL-Data sanitization
 * at boot.
 */
static void prepare_bl_data(void)
{
	/* Flag partition as invalid */
	part->is_consistent = false;
	/* Write back bl-data to flash */
	bl_data_shadow_writeback();
}

/**
 * Handle a block expected to contain a QFU header.
 *
 * @param[in] data The block to be processed. Must not be null.
 * @param[in] len  The length of the data.
 *
 * @return DFU_STATUS_OK if the header is valid, an error DFU status otherwise.
 */
static dfu_dev_status_t qfu_handle_hdr(const uint8_t *data, int len)
{
	DBG_PRINTF("handle_qfu_hdr()\n");
	uint16_t n_data_blocks;

	/*
	 * The length of header blocks must be equal to the QFU block size
	 * (since the host is expected to pad the header to make its size a
	 * multiple of the QFU block size).
	 */
	if (len != QFU_BLOCK_SIZE) {
		return DFU_STATUS_ERR_ADDRESS;
	}

	/*
	 * Immediately store the header in our internal buffer, since it is
	 * probably safer than the external I/O buffer.
	 */
	memcpy(img_hdr, data, HDR_BUF_SIZE);

	/* Verify image 'magic' field. */
	if (img_hdr->magic != QFU_HDR_MAGIC) {
		return DFU_STATUS_ERR_TARGET;
	}
	/* Verify Vendor ID (if VID enforcing is active). */
	if (FM_CFG_ENFORCE_VID && (img_hdr->vid != DFU_CFG_VID)) {
		return DFU_STATUS_ERR_TARGET;
	}
	/* Verify Product ID (if PID enforcing is active). */
	if (FM_CFG_ENFORCE_APP_PID && (img_hdr->pid != DFU_CFG_PID)) {
		return DFU_STATUS_ERR_TARGET;
	}
	/* Verify DFU-mode Product ID (id DFU PID enforcing is active). */
	if (FM_CFG_ENFORCE_DFU_PID && (img_hdr->pid_dfu != DFU_CFG_PID_DFU)) {
		return DFU_STATUS_ERR_TARGET;
	}
	/*
	 * Verify that the image is actually for the selected partition /
	 * alternate setting.
	 */
	if (img_hdr->partition != active_alt_setting) {
		return DFU_STATUS_ERR_ADDRESS;
	}
	/*
	 * Note: even if DFU allows host tools to use a block size smaller than
	 * the maximum one specified by the device, we force the block size to
	 * be equal to the maximum block size (i.e., the page size), since this
	 * simplifies the flashing logic, thus leading to smaller footprint.
	 *
	 * This is not a huge limitation, since by default dfu-util uses the
	 * maximum block size and there is no benefit for users to specify a
	 * smaller one.
	 */
	if (img_hdr->block_sz != QFU_BLOCK_SIZE) {
		DBG_PRINTF("Block size error: %d\n", img_hdr->block_sz);
		return DFU_STATUS_ERR_FILE;
	}
	n_data_blocks = img_hdr->n_blocks - NUM_HDR_BLOCKS;
	/* Image size cannot be bigger than the partition size (in pages). */
	if (n_data_blocks * QFU_BLOCK_SIZE_PAGES > part->num_pages) {
		DBG_PRINTF("ERROR: data_blocks > part->num_pages\n");
		DBG_PRINTF("data_blocks: %d\n", n_data_blocks);
		DBG_PRINTF("img_hdr->n_blocks: %d\n", img_hdr->n_blocks);
		return DFU_STATUS_ERR_ADDRESS;
	}
	/* The extended header must be the expected one. */
	if (img_hdr->ext_hdr_type != QFU_EXPECTED_EXT_HDR) {
		return DFU_STATUS_ERR_FILE;
	}
	/* Perform checks specific for the current extended header. */
	if (qfu_check_ext_hdr(img_hdr, n_data_blocks, part)) {
		return DFU_STATUS_ERR_FILE;
	}

	return DFU_STATUS_OK;
}

/**
 * Handle a block expected to contain a QFU data block to be written to flash.
 *
 * @param[in] blk_num The sequence number of the block to be processed.
 * @param[in] data The block to be processed. Must not be null.
 * @param[in] len  The len of the block.
 *
 * @return DFU_STATUS_OK if the header is valid, an error DFU status otherwise.
 */
static dfu_dev_status_t qfu_handle_blk(uint32_t blk_num, const uint8_t *data,
				       uint32_t len)
{
	DBG_PRINTF("handle_qfu_blk(): blk_num = %u; len = %u\n", blk_num, len);
	uint32_t target_page;
	const uint32_t *page_addr;
	uint32_t *buf_ptr;
	int i;
	qm_flash_reg_t *flash_regs;

	/*
	 * Verify block validity:
	 * - block_num must be < number of blocks declared in header
	 * - len must be equal to declared block size, with the exception of
	 *   the last, which can be smaller (but not greater!)
	 */
	if (blk_num >= img_hdr->n_blocks || len > img_hdr->block_sz ||
	    (blk_num + 1 < img_hdr->n_blocks && len != img_hdr->block_sz)) {
		return DFU_STATUS_ERR_ADDRESS;
	}
	/*
	 * Set our internal block buffer to 0xFF so that we can always write it
	 * entirely to flash (i.e., we do not have to handle the length of the
	 * last block in a special way).
	 */
	memset(blk_buf, 0xFF, sizeof(blk_buf));
	/* Copy the block in our internal buffer. */
	memcpy(blk_buf, data, len);
#if (ENABLE_FIRMWARE_MANAGER_AUTH)
	if (qfu_hmac_check_block_hash(blk_buf, len, img_hdr,
				      blk_num - NUM_HDR_BLOCKS)) {
		/*
		 * If block hash verification fails, we call bl_data_sanitize()
		 * in order to erase the partition (i.e., what has been written
		 * so far) and mark the partition back as consistent (but
		 * empty).
		 */
		bl_data_sanitize();
		return DFU_STATUS_ERR_FILE;
	}
#endif
	/* If first data block, prepare bl_data (mark partition as invalid). */
	if (blk_num == NUM_HDR_BLOCKS) {
		prepare_bl_data();
	}
	/*
	 * Write the block to flash, one page at a time (a block can be
	 * composed of multiple pages.
	 */
	target_page = part->first_page +
		      ((blk_num - NUM_HDR_BLOCKS) * QFU_BLOCK_SIZE_PAGES);
	buf_ptr = (uint32_t *)blk_buf;
	page_addr = (uint32_t *)part->start_addr +
		    ((blk_num - NUM_HDR_BLOCKS) * QFU_BLOCK_SIZE_PAGES *
		     QM_FLASH_PAGE_SIZE_DWORDS);
	for (i = 0; i < QFU_BLOCK_SIZE_PAGES; i++) {
		qm_flash_page_write(part->controller, QM_FLASH_REGION_SYS,
				    target_page, buf_ptr,
				    QM_FLASH_PAGE_SIZE_DWORDS);
		/* Flash content has changed, flush prefetch buffer. */
		flash_regs = QM_FLASH[part->controller];
		flash_regs->ctrl |= QM_FLASH_CTRL_PRE_FLUSH_MASK;
		flash_regs->ctrl &= ~QM_FLASH_CTRL_PRE_FLUSH_MASK;
#if (!UNIT_TEST)
		/* Verify flash write has been successfully completed. */
		if (memcmp(buf_ptr, page_addr, QM_FLASH_PAGE_SIZE_BYTES)) {
			return DFU_STATUS_ERR_VERIFY;
		}
#else
		(void)page_addr;
#endif
		buf_ptr += QM_FLASH_PAGE_SIZE_DWORDS;
		page_addr += QM_FLASH_PAGE_SIZE_DWORDS;
		target_page++;
	}

	return DFU_STATUS_OK;
}

/*-----------------------------------------------------------------------*/
/* STATIC FUNCTIONS (DFU Request Handler implementation)                 */
/*-----------------------------------------------------------------------*/

/*
 * Initialize the QFU DFU Request Handler.
 *
 * This function is called when a QFU alt setting is selected (i.e., every
 * alternate setting > 0).
  */
static void qfu_init(uint8_t alt_setting)
{
	active_alt_setting = alt_setting;
	/* Decrement alt setting since first QFU alt setting is 1 and not 0 */
	part = &bl_data->partitions[alt_setting - 1];
	qfu_err_status = DFU_STATUS_OK;
	/* Call bl-data for extra safety (we ensure bl-data consistency) */
	bl_data_sanitize();
}

/*
 * Get the status and state of the handler.
 *
 * The DFU module calls this function when receiving a DFU_GET_STATUS or
 * DFU_GET_STATE request.
 */
static void qfu_get_status(dfu_dev_status_t *status, uint32_t *poll_timeout_ms)
{
	*status = qfu_err_status;
	/*
	 * NOTE: poll_timeout is always set to zero because the flash is
	 * updated in qfu_dnl_process_block() (i.e., as soon as the block is
	 * received). This is fine for QDA but may need to be changed for USB.
	 */
	*poll_timeout_ms = 0;
}

/*
 * Clear the status and state of the handler.
 *
 * This function is used to reset the handler state machine after an error. It
 * is called by DFU core when a DFU_CLRSTATUS request is received.
 */
static void qfu_clear_status(void)
{
	/*
	 * Clear status is called after a DFU error, which may imply a failed
	 * upgrade; therefore we call bl_data_sanitize() to ensure that bl-data
	 * is fixed and inconsistent partitions are erased if needed.
	 */
	bl_data_sanitize();
	qfu_err_status = DFU_STATUS_OK;
}

/*
 * Process a DFU_DNLOAD block.
 *
 * The DFU_DNLOAD block is expected to contain a QFU header or block.
 */
static void qfu_dnl_process_block(uint32_t block_num, const uint8_t *data,
				  uint16_t len)
{
	/* Disable interrupts for security reasons */
	qm_irq_disable();
	if (block_num == 0) {
		/* Header block */
		qfu_err_status = qfu_handle_hdr(data, len);
	} else {
		/* Data block */
		qfu_err_status = qfu_handle_blk(block_num, data, len);
	}
	/* Re-enable interrupts before returning */
	qm_irq_enable();
}

/*
 * Finalize the current DFU_DNLOAD transfer.
 *
 * This function is called by DFU Core when an empty DFU_DNLOAD request
 * (signaling the end of the current DFU_DNLOAD transfer) is received.
 *
 * In case of the QFU handler, this is where bootloader data (e.g., application
 * version, SVN, image selector, etc.) get updated with information about the
 * new application firmware.
 *
 * A error is returned if additional header blocks were expected.
 */
static int qfu_dnl_finalize_transfer(uint32_t block_num)
{
	DBG_PRINTF("Finalize update\n");
	int t_idx;

	/* Fail if we did not received the right number of blocks. */
	if (block_num != img_hdr->n_blocks) {
		/* call bl_data_sanitize() to erase inconsistent partitions. */
		bl_data_sanitize();
		return -EINVAL;
	}

	part->is_consistent = true;
	part->app_version = img_hdr->version;
	t_idx = part->target_idx;
	bl_data->targets[t_idx].active_partition_idx = active_alt_setting - 1;
#if (ENABLE_FIRMWARE_MANAGER_AUTH)
	bl_data->targets[t_idx].svn = ((qfu_hdr_hmac_t *)img_hdr->ext_hdr)->svn;
#endif
	bl_data_shadow_writeback();

	return 0;
}

/*
 * Fill up a DFU_UPLOAD block.
 *
 * This function is called by the DFU logic when a request for an
 * UPLOAD block is received. The handler is in charge of filling the
 * payload of the block.
 *
 * When the QFU handler is active (i.e., the selected alternate setting is
 * different from 0), DFU_UPLOAD requests are not allowed and therefore an
 * empty payload is always returned.
 */
static void qfu_upl_fill_block(uint32_t blk_num, uint8_t *data,
			       uint16_t req_len, uint16_t *len)
{
	/* Firmware extraction is not allowed: upload nothing */
	(void)blk_num;
	(void)data;
	(void)req_len;
	*len = 0;
}

/*
 * Abort current DNLOAD/UPLOAD transfer and go back to handler's initial state.
 *
 * This function is called by DFU core when a DFU_ABORT request is received.
 */
static void qfu_abort_transfer(void)
{
	/* bl_data_sanitize() erases inconsistent partitions if needed. */
	bl_data_sanitize();
}
