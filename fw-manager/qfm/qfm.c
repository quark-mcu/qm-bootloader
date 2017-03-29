/**
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

#include "qm_common.h"
#include "qm_interrupt.h"

#include "qfm_packets.h"
#include "bl_data.h"
#include "fw-manager_utils.h"
#include "../dfu/dfu.h"
/* qfu_format.h included because of authentication enum (qfm_auth_type_t) */
#include "../qfu/qfu_format.h"
#include "tinycrypt/hmac.h"
#include "rom_version.h"

#if (QUARK_SE)
#define QFM_SYS_INFO_INIT_SOC_TYPE (QFM_SOC_TYPE_QUARK_SE)
#define QFM_SYS_INFO_INIT_TARGET_LIST                                          \
	{                                                                      \
		{                                                              \
			.target_type = QFM_TARGET_TYPE_X86                     \
		}                                                              \
		,                                                              \
		{                                                              \
			.target_type = QFM_TARGET_TYPE_SENSOR                  \
		}                                                              \
	}
#elif(QUARK_D2000)
#define QFM_SYS_INFO_INIT_SOC_TYPE (QFM_SOC_TYPE_QUARK_D2000)
#define QFM_SYS_INFO_INIT_TARGET_LIST                                          \
	{                                                                      \
		{                                                              \
			.target_type = QFM_TARGET_TYPE_X86                     \
		}                                                              \
	}
#endif

#if (ENABLE_FIRMWARE_MANAGER_AUTH)
#define AUTHENTICATION_ID QFU_EXT_HDR_HMAC256
#else
#define AUTHENTICATION_ID QFU_EXT_HDR_NONE
#endif

/*-----------------------------------------------------------------------*/
/* FORWARD DECLARATIONS                                                  */
/*-----------------------------------------------------------------------*/
static void qfm_init(uint8_t alt_setting);
static void qfm_get_processing_status(dfu_dev_status_t *status,
				      uint32_t *poll_timeout_ms);
static void qfm_clear_status(void);
static void qfm_dnl_process_block(uint32_t block_num, const uint8_t *data,
				  uint16_t len);
static int qfm_dnl_finalize_transfer(uint32_t block_num);
static void qfm_upl_fill_block(uint32_t block_num, uint8_t *data,
			       uint16_t max_len, uint16_t *len);
static void qfm_abort_transfer(void);

/*-----------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                      */
/*-----------------------------------------------------------------------*/

/*
 * QFM request handler variable (used by DFU core when alternate setting zero
 * is selected).
 */
const dfu_request_handler_t qfm_dfu_rh = {
    &qfm_init, &qfm_get_processing_status, &qfm_clear_status,
    &qfm_dnl_process_block, &qfm_dnl_finalize_transfer, &qfm_upl_fill_block,
    &qfm_abort_transfer,
};

/** The variable holding the outgoing QFM System Information response packet. */
static qfm_sys_info_rsp_t sys_info_rsp = {
    .qfm_pkt_type = QFM_SYS_INFO_RSP,
    .sysupd_version = QM_VER_ROM,
    .soc_type = QFM_SYS_INFO_INIT_SOC_TYPE,
    .auth_type = AUTHENTICATION_ID,
    .target_count = BL_BOOT_TARGETS_NUM,
    .partition_count = BL_FLASH_PARTITIONS_NUM,
    .targets = QFM_SYS_INFO_INIT_TARGET_LIST,
};

/** Flag indicating whether a SySInfo response is pending or not. */
static bool sys_info_rsp_pending;

/**
 * The DFU status of this DFU request handler.
 */
static dfu_dev_status_t dfu_status;

/*-----------------------------------------------------------------------*/
/* STATIC FUNCTIONS (QFM functions)                                      */
/*-----------------------------------------------------------------------*/
/**
 * Prepare a QFM System Information response (QFM_SYS_INFO_RSP) packet.
 *
 * This function is called when a QFM System Information request
 * (QFM_SYS_INFO_REQ) is received.
 */
static void prepare_sys_info_rsp(void)
{
	int i;
	bl_flash_partition_t *part;

	/* Fill up the packet's partition descriptors. */
	for (i = 0; i < BL_FLASH_PARTITIONS_NUM; i++) {
		part = &bl_data->partitions[i];
		sys_info_rsp.partitions[i].app_present =
		    (*part->start_addr != 0xFFFFFFFF);
		sys_info_rsp.partitions[i].app_version = part->app_version;
	}
	/* Fill up the packet's target descriptors. */
	for (i = 0; i < BL_BOOT_TARGETS_NUM; i++) {
		sys_info_rsp.targets[i].active_partition_idx =
		    bl_data->targets[i].active_partition_idx;
	}

	sys_info_rsp_pending = true;
}

#if (ENABLE_FIRMWARE_MANAGER_AUTH == 0)
/*
 * Application Erase.
 *
 * Erase all application code from flash.
 *
 * This functionality is not available when authentication is enabled.
 */
static void app_erase(void)
{
	int i;
	bl_flash_partition_t *part;

	/*
	 * First update bl-data by marking every partition as inconsistent and
	 * setting the app version of each partition to undefined.
	 */
	for (i = 0; i < BL_FLASH_PARTITIONS_NUM; i++) {
		part = &bl_data->partitions[i];
		part->is_consistent = false;
	}
	bl_data_shadow_writeback();
	/*
	 * Then call bl_data_sanitize() to make it erase the partitions and
	 * mark them back as consistent.
	 */
	bl_data_sanitize();
}
#endif

#if (ENABLE_FIRMWARE_MANAGER_AUTH)
static dfu_dev_status_t qfm_update_key(const qfm_update_pkt_t *pkt,
				       hmac_key_t *key_adress)
{
	static sha256_t computed_hmac;
	dfu_dev_status_t retv;

	/* Disable interrupts for security reasons. */
	qm_irq_disable();
	/* Calculate the HMAC of the key packet using the fw key. */
	fm_hmac_compute_hmac(pkt, sizeof(*pkt) - sizeof(pkt->mac),
			     &bl_data->fw_key, &computed_hmac);
	/* Calculate the HMAC of the previous HMAC using the revocation key. */
	fm_hmac_compute_hmac(computed_hmac.u8, sizeof(sha256_t),
			     &bl_data->rv_key, &computed_hmac);

	/* Default return value is error. */
	retv = DFU_STATUS_ERR_VENDOR;
	/* Verify that the HMAC of the packet matches the computed one. */
	if (memcmp(&pkt->mac, &computed_hmac, sizeof(pkt->mac)) == 0) {
		memcpy(key_adress, &pkt->key, sizeof(hmac_key_t));
		/*
		 * No need to clear the packet content (containing the key)
		 * since the DFU buffer (where the packet is located) is cleared
		 * by the DFU Core module.
		 */
		bl_data_shadow_writeback();
		/* Update return value to success. */
		retv = DFU_STATUS_OK;
	}
	/* Re-enable interrupts */
	qm_irq_enable();

	return retv;
}
#endif /* ENABLE_FIRMWARE_MANAGER_AUTH */

/**
 * Parse and process the incoming QFM request.
 *
 * @param[in] data The buffer containing the packet to be parsed. Must not be
 * 		   null.
 * @param[in] len  The length of the packet to be parsed.
 *
 * @return The DFU Device Status of the processing result.
 */
static dfu_dev_status_t process_qfm_req(const uint8_t *data, uint16_t len)
{
	const qfm_generic_pkt_t *pkt;

	if (len < sizeof(qfm_generic_pkt_t)) {
		return DFU_STATUS_ERR_TARGET;
	}
	/*
	 * Note: currently, we do not perform any additional check on the
	 * received packet length in order to keep the footprint low. There is
	 * no security risk here: if not enough bytes are received, the
	 * processing fails due to parsing errors; whereas if too many bytes
	 * are received, the additional ones are just discarded.
	 */
	pkt = (qfm_generic_pkt_t *)data;
	switch (pkt->type) {
	case QFM_SYS_INFO_REQ:
		prepare_sys_info_rsp();
		return DFU_STATUS_OK;
#if (ENABLE_FIRMWARE_MANAGER_AUTH == 0)
	/* App erase is enabled only if authentication is disabled. */
	case QFM_APP_ERASE:
		/*
		 * App erase takes just a few ms so we can safely perform it
		 * here, instead of replying to the DFU_DNLOAD request first.
		 */
		app_erase();
		return DFU_STATUS_OK;
#else /* ENABLE_FIRMWARE_MANAGER_AUTH == 1 */
	/* Key provisioning is enabled only when authentication is enabled. */
	case QFM_UPDATE_FW_KEY:
		if (fm_hmac_is_default_key(&bl_data->rv_key)) {
			return DFU_STATUS_ERR_VENDOR;
		}
		return qfm_update_key((qfm_update_pkt_t *)pkt,
				      &bl_data->fw_key);
	case QFM_UPDATE_RV_KEY:
		return qfm_update_key((qfm_update_pkt_t *)pkt,
				      &bl_data->rv_key);
#endif
	default:
		return DFU_STATUS_ERR_TARGET;
	}
}

/*-----------------------------------------------------------------------*/
/* STATIC FUNCTIONS (DFU Request Handler implementation)                 */
/*-----------------------------------------------------------------------*/

/*
 * Initialize the QFM DFU Request Handler.
 *
 * This function is called by the DFU logic when the QFM alternate setting is
 * selected (i.e., alternate setting 0).
 */
static void qfm_init(uint8_t alt_setting)
{
	/* alt_setting is not needed by the QFM DFU request handler */
	(void)alt_setting;
	dfu_status = DFU_STATUS_OK;
}

/*
 * Get the status and state of the handler.
 *
 * The DFU module calls this function when receiving a DFU_GET_STATUS or
 * DFU_GET_STATE request.
 */
static void qfm_get_processing_status(dfu_dev_status_t *status,
				      uint32_t *poll_timeout_ms)
{
	*status = dfu_status;
	*poll_timeout_ms = 0;
}

/*
 * Clear the status and state of the handler.
 *
 * This function is used to reset the handler state machine after an error. It
 * is called by DFU core when a DFU_CLRSTATUS request is received.
 */
static void qfm_clear_status(void)
{
	dfu_status = DFU_STATUS_OK;
}

/*
 * Process a DFU_DNLOAD block.
 *
 * The DFU_DNLOAD block is expected to contain a QFM request.
 */
static void qfm_dnl_process_block(uint32_t block_num, const uint8_t *data,
				  uint16_t len)
{
	sys_info_rsp_pending = false;
	/*
	 * We do not support QFM requests split in multiple blocks: the entire
	 * request must be in the first (and only) block. Therefore we return
	 * an error if block_num is not 0.
	 *
	 * This is not a huge limitation since there is no value for the host
	 * to use multiple blocks.
	 */
	if (block_num != 0) {
		dfu_status = DFU_STATUS_ERR_TARGET;
		return;
	}
	dfu_status = process_qfm_req(data, len);
}

/*
 * Finalize the current DFU_DNLOAD transfer.
 *
 * This function is called by DFU Core when an empty DFU_DNLOAD request
 * (signaling the end of the current DFU_DNLOAD transfer) is received.
 *
 * The handler must return '0' if it agrees with the end of the transfer or
 * '-EIO' if it was actually expecting more data.
 */
static int qfm_dnl_finalize_transfer(uint32_t block_num)
{
	(void)block_num;

	return 0;
}

/*
 * Fill up a DFU_UPLOAD block.
 *
 * This function is called by the DFU logic when a request for an UPLOAD block
 * is received. The handler is in charge of filling the payload of the block.
 *
 * When QFM mode (i.e., alternate setting 0) is active, the host sends a
 * DFU_UPLOAD request to retrieve the response to the QFM Request previously
 * sent in DFU_DNLOAD transfer. Note, however, that not every QFM request
 * expects a QFM response. In fact, at the moment, only the QFM SysInfo request
 * expects a QFM response.
 *
 * For the sake of code-size minimization, we require the host to use a block
 * size (i.e., req_len) greater than the response length.  In other words, the
 * response must fit in a single UPLOAD block.  This is not a huge limitation,
 * since there is no reason for the host to use a block size smaller than the
 * device's maximum block size (typically a few kB).
 */

static void qfm_upl_fill_block(uint32_t blk_num, uint8_t *data,
			       uint16_t req_len, uint16_t *len)
{
	(void)blk_num;

	/* By default no response is returned. */
	*len = 0;
	/*
	 * But if a SysInfo response is pending and the block size is large
	 * enough to contain it, we return it.
	 */
	if (sys_info_rsp_pending && (req_len >= sizeof(sys_info_rsp))) {
		memcpy(data, &sys_info_rsp, sizeof(sys_info_rsp));
		*len = sizeof(sys_info_rsp);
	}
	sys_info_rsp_pending = false;
}

/**
 * Abort current DNLOAD/UPLOAD transfer and go back to handler's initial state.
 *
 * This function is called by DFU core when a DFU_ABORT request is received.
 */
static void qfm_abort_transfer(void)
{
	sys_info_rsp_pending = false;
}
