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

#ifndef __DFU_H__
#define __DFU_H__

#include <stdint.h>

#include "qm_common.h"

#include "fw-manager_config.h"
#include "../bl_data.h"

/* DFU attributes */
#define DFU_ATTR_CAN_DNLOAD 0x01
#define DFU_ATTR_CAN_UPLOAD 0x02
#define DFU_ATTR_MANIFESTATION_TOLERANT 0x4

/* Maximum supported block size. */
#define DFU_MAX_BLOCK_SIZE (QFU_BLOCK_SIZE)

/* DFU Version (as BCD). */
#define DFU_VERSION_BCD (0x0101)

/**
 * Number of alternate settings.
 *
 * Number of partitions + QFM alternate setting (i.e., alt setting 0).
 */
#define DFU_NUM_ALT_SETTINGS (1 + BL_FLASH_PARTITIONS_NUM)

/* These are exposed in DFU Descriptors. */
/* Detach timeout. */
#define DFU_DETACH_TIMEOUT (0xFFFF)

/**
 * DFU attributes (bitfield).
 *
 * DFU bmAttributes (bitfield): 0x07 (bitWillDetach = 0,
 * bitManifestationTollerant = 1, bitCanUpload = 1, bitCanDnload = 1)
 */
#define DFU_ATTRIBUTES                                                         \
	(BIT(DFU_ATTR_CAN_DNLOAD) | BIT(DFU_ATTR_CAN_UPLOAD) |                 \
	 BIT(DFU_ATTR_MANIFESTATION_TOLERANT))

/**
 * DFU device statuses.
 */
typedef enum {
	/** OK: No error condition is present. */
	DFU_STATUS_OK = 0x00,
	/** errTARGET: File is not targeted for this device. */
	DFU_STATUS_ERR_TARGET = 0x01,
	/**
	 * errFILE: File is for this device but fails some vendor-specific
	 * verification test.
	 */
	DFU_STATUS_ERR_FILE = 0x02,
	/** errWRITE: Device is unable to write memory. */
	DFU_STATUS_ERR_WRITE = 0x03,
	/** errERASE: Memory erase function failed. */
	DFU_STATUS_ERR_ERASE = 0x04,
	/** errCHECK_ERASED: Memory erase check failed. */
	DFU_STATUS_ERR_CHECK_ERASED = 0x05,
	/** errPROG: Program memory function failed. */
	DFU_STATUS_ERR_PROG = 0x06,
	/** errVERIFY: Programmed memory failed verification. */
	DFU_STATUS_ERR_VERIFY = 0x07,
	/**
	 * errADDRESS: Cannot program memory due to received address that is
	 * out of range.
	 */
	DFU_STATUS_ERR_ADDRESS = 0x08,
	/**
	 * errNOTDONE: Received DFU_DNLOAD with wLength = 0, but device does
	 * not think it has all of the data yet.
	 */
	DFU_STATUS_ERR_NOTDONE = 0x09,
	/**
	 * errFIRMWARE: Device’s firmware is corrupt. Device cannot return to
	 * run-time (non-DFU) operations.
	 */
	DFU_STATUS_ERR_FIRMWARE = 0x0A,
	/** errVENDOR: iString indicates a vendor-specific error. */
	DFU_STATUS_ERR_VENDOR = 0x0B,
	/** errUSBR: Device detected unexpected USB reset signaling. */
	DFU_STATUS_ERR_USBR = 0x0C,
	/** errPOR: Device detected unexpected power on reset. */
	DFU_STATUS_ERR_POR = 0x0D,
	/**
	 * errUNKNOWN: Something went wrong, but the device does not know what
	 * it was.
	 */
	DFU_STATUS_ERR_UNKNOWN = 0x0E,
	/** errSTALLEDPKT: Device stalled an unexpected request. */
	DFU_STATUS_ERR_STALLEDPKT = 0x0F
} dfu_dev_status_t;

/**
 * DFU device states.
 */
typedef enum {
	/** appIDLE: Device is running its normal application. */
	DFU_STATE_APP_IDLE = 0,
	/**
	 * appDETACH: Device is running its normal application, has received
	 * the DFU_DETACH request, and is waiting for a USB reset.
	 */
	DFU_STATE_APP_DETACH = 1,
	/**
	 * dfuIDLE: Device is operating in the DFU mode and is waiting for
	 * requests.
	 */
	DFU_STATE_DFU_IDLE = 2,
	/**
	 * dfuDNLOAD-SYNC: Device has received a block and is waiting for the
	 * host to solicit the status via DFU_GETSTATUS.
	 */
	DFU_STATE_DFU_DNLOAD_SYNC = 3,
	/**
	 * dfuDNBUSY: Device is programming a control-write block into its
	 * nonvolatile memories.
	 */
	DFU_STATE_DFU_DNBUSY = 4,
	/**
	 * dfuDNLOAD-IDLE: Device is processing a download operation. Expecting
	 * DFU_DNLOAD requests.
	 */
	DFU_STATE_DFU_DNLOAD_IDLE = 5,
	/**
	 * dfuMANIFEST-SYNC: Device has received the final block of firmware
	 * from the host and is waiting for receipt of DFU_GETSTATUS to begin
	 * the Manifestation phase; or device has completed the Manifestation
	 * phase and is waiting for receipt of DFU_GETSTATUS.
	 */
	DFU_STATE_DFU_MANIFEST_SYNC = 6,
	/** dfuMANIFEST: Device is in the Manifestation phase. */
	DFU_STATE_DFU_MANIFEST = 7,
	/**
	 * dfuMANIFEST-WAIT-RESET: Device has programmed its memories and is
	 * waiting for a USB reset or a power on reset.
	 */
	DFU_STATE_DFU_MANIFEST_WAIT_RESET = 8,
	/**
	 * dfuUPLOAD-IDLE: The device is processing an upload operation.
	 * Expecting DFU_UPLOAD requests.
	 */
	DFU_STATE_DFU_UPLOAD_IDLE = 9,
	/**
	 * dfuERROR: An error has occurred. Awaiting the DFU_CLRSTATUS request.
	 */
	DFU_STATE_DFU_ERROR = 10
} dfu_dev_state_t;

/**
 * DFU Request Handler struct.
 *
 * This struct group all the functions of a DFU Request handler, which is a
 * software component in charge of processing the data coming from DFU_DNLOAD
 * transfers and providing data for DFU_UPLOAD transfers.
 */
typedef struct {
	/**
	 * Initialize the DFU Request Handler.
	 *
	 * This function is called when a DFU alternate setting associated with
	 * the handler is selected. This function pointer must not be null.
	 *
	 * @param[in] alt_setting The specific alternate setting activating this
	 * 			  handler.
	 */
	void (*init)(uint8_t alt_setting);
	/**
	 * Get the processing status of the last DNLOAD block.
	 *
	 * This function is called by the DFU logic to ask the handler for
	 * information about the processing of the last DNLOAD block. This
	 * function pointer must not be null.
	 *
	 * @param[out] status		A pointer to the variable where to store
	 * 				the (error) status of the processing. If
	 * 				no error has occurred the variable is
	 * 				set to DFU_STATUS_OK. The pointer must
	 *				not be null.
	 *
	 * @param[out] poll_timeout_ms	A pointer to the variable where to
	 * 				store the poll timeout. If the
	 * 				processing is completed, the variable is
	 *				set to zero; otherwise it is set to the
	 *				expected remaining time. The pointer
	 *				must not be null.
	 */
	void (*get_proc_status)(dfu_dev_status_t *status,
				uint32_t *poll_timeout_ms);
	/**
	 * Clear the status and state of the handler.
	 *
	 * This function is called by the DFU logic when a DFU_CLRSTATUS
	 * request is received. The host issues such a request after an error,
	 * to recover from it. This function pointer must not be null.
	 */
	void (*clr_status)(void);
	/**
	 * Process a DFU_DNLOAD block.
	 *
	 * This function is called by the DFU logic when a DNLOAD block is
	 * received. This function pointer must not be null.
	 *
	 * @param blk_num The block number (first block is always block 0).
	 * @param data	  The buffer containing the block payload. The
	 *		  pointer must not be null.
	 * @param len	  The length of the payload.
	 */
	void (*proc_dnload_blk)(uint32_t blk_num, const uint8_t *data,
				uint16_t len);
	/**
	 * Finalize the current DFU_DNLOAD transfer.
	 *
	 * This function is called by the DFU logic when a DNLOAD block with
	 * zero length is received. This function pointer must not be null.
	 *
	 * @param blk_num The block number of the finalize request.
	 *
	 * @return QMSI return code.
	 * @retval 0 if handler agrees with the end of the transfer.
	 * @retval -EINVAL if handler was actually expecting more data.
	 */
	int (*fin_dnload_xfer)(uint32_t blk_num);
	/**
	 * Fill up a DFU_UPLOAD block.
	 *
	 * This function is called by the DFU logic when a request for an
	 * UPLOAD block is received. The handler is in charge of filling the
	 * payload of the block. This function pointer must not be null.
	 *
	 * @param[in]  blk_num The block number (first block is always block 0).
	 * @param[out] data    The buffer where the payload will be put. The
	 *		       pointer must not be null.
	 * @param[in]  req_len The amount of data requested by the host.
	 * @param[out] len     A pointer to the variable where to store the
	 *                     actual amount of data provided by the handler
	 * 		       (len < req_len *means that there is no more data
	 * 		       to send, i.e., this is the last block). The
	 *		       pointer must not be null.
	 */
	void (*fill_upload_blk)(uint32_t blk_num, uint8_t *data,
				uint16_t req_len, uint16_t *len);
	/**
	 * Abort current DNLOAD transfer.
	 *
	 * This function pointer must not be null.
	 */
	void (*abort_dnload_xfer)(void);
} dfu_request_handler_t;

#endif /* __DFU_H__ */
