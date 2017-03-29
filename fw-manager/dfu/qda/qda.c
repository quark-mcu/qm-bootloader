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

#include "qm_common.h"
#include "qm_flash.h"
#include "qm_init.h"

#include "qda.h"
#include "../core/dfu_core.h"
#include "../dfu.h"
#include "../bl_data.h"
#include "qda_packets.h"
#include "xmodem.h"
#include "xmodem_io_uart.h"
#include "fw-manager_config.h"

/*--------------------------------------------------------------------------*/
/*                              MACROS                                      */
/*--------------------------------------------------------------------------*/

/* Additional XMODEM_BLOCK_SIZE bytes needed because of QDA overhead */
#define QDA_BUF_SIZE (QFU_BLOCK_SIZE + XMODEM_BLOCK_SIZE)

/*--------------------------------------------------------------------------*/
/*                    GLOBAL VARIABLES                                      */
/*--------------------------------------------------------------------------*/

/**
 * The buffer for incoming and outgoing QDA packets.
 *
 * Note: some outgoing packets are pre-compiled and have their own variable.
 */
static uint8_t qda_buf[QDA_BUF_SIZE];

/*--------------------------------------------------------------------------*/
/*                           FORWARD DECLARATIONS                           */
/*--------------------------------------------------------------------------*/
static void qda_process_pkt(uint8_t *data, size_t len);
static void qda_ack(void);
static void qda_stall(void);
static void handle_upload_req(qda_upl_req_payload_t *req);
static void qda_dfu_get_status_rsp(dfu_dev_state_t state,
				   dfu_dev_status_t status,
				   uint32_t poll_timeout);
static void qda_dfu_get_state_rsp(dfu_dev_state_t state);
static void qda_dfu_dsc_rsp(void);

/*--------------------------------------------------------------------------*/
/*                            GLOBAL FUNCTIONS                              */
/*--------------------------------------------------------------------------*/
/* Initialize the QDA module (by initializing required modules). */
void qda_init(void)
{
	xmodem_io_uart_init();
	dfu_init();
}

/*
 * Receive and process QDA packets.
 *
 * Receive and process QDA packets, until the communication becomes idle (i.e.,
 * no data is received for a certain amount of time).
 */
void qda_receive_loop(void)
{
	int len;

	do {
		/*
		 * Receive a new packet using XMODEM.
		 *
		 * xmodem_receive() is blocking: the function returns when the
		 * XMODEM transfer is completed or an unrecoverable reception
		 * error occurs (e.g., a transmission starts, but then timeouts
		 * or the maximum number of retries is exceeded). The function
		 * returns the length of the received data on success, a
		 * negative error code otherwise.
		 */
		len = xmodem_receive_package(qda_buf, sizeof(qda_buf));
		if (len > 0) {
			qda_process_pkt(qda_buf, len);
		}
		/*
		 * NOTE: for this function to work properly, XMODEM must be
		 * changed to return a special value when the failure is due to
		 * a timeout and not an error.
		 *
		 * For now we do not distinguish between a timeout and an
		 * unrecoverable error: in both cases we exit the loop.
		 */
	} while (len > 0);
}

/*--------------------------------------------------------------------------*/
/*                    STATIC FUNCTION DEFINITION                            */
/*--------------------------------------------------------------------------*/

/**
 * Process a QDA packet.
 *
 * Parse, process, and reply to an incoming QDA packet.
 *
 * @param[in] data The buffer containing the packet. Must not be null.
 * @param[in] len  The length of packet or its upper bound (since XMODEM may
 * 		   add some padding bytes).
 */
static void qda_process_pkt(uint8_t *data, size_t len)
{
	qda_pkt_t *pkt;
	qda_dnl_req_payload_t *dnload_req;
	qda_upl_req_payload_t *upload_req;
	qda_set_alt_setting_payload_t *altset_req;
	size_t expected_len;
	dfu_dev_state_t state;
	dfu_dev_status_t status;
	uint32_t poll_timeout;
	int retv;

	pkt = (qda_pkt_t *)data;
	expected_len = sizeof(*pkt);

	/*
	 * This check is not really needed when using XMODEM (since len will
	 * always be > 128 bytes), but better safe than sorry.
	 */
	if (len < expected_len) {
		qda_stall();
		return;
	}

	switch (pkt->type) {
	case QDA_PKT_DFU_DESC_REQ:
		/* Handle a DFU descriptor request. */
		qda_dfu_dsc_rsp();
		return;
	case QDA_PKT_DFU_SET_ALT_SETTING:
		/* Handle a 'set alternate setting' request. */
		altset_req = (qda_set_alt_setting_payload_t *)pkt->payload;
		retv = dfu_set_alt_setting(altset_req->alt_setting);
		if (retv == 0) {
			qda_ack();
			return;
		}
		qda_stall();
		return;
	case QDA_PKT_DFU_DNLOAD_REQ:
		/* Handle a DFU DNLOAD request. */
		dnload_req = (qda_dnl_req_payload_t *)pkt->payload;
		expected_len += sizeof(*dnload_req) + dnload_req->data_len;
		if (len >= expected_len) {
			retv = dfu_process_dnload(dnload_req->block_num,
						  dnload_req->data,
						  dnload_req->data_len);
			if (retv == 0) {
				qda_ack();
				return;
			}
		}
		qda_stall();
		return;
	case QDA_PKT_DFU_UPLOAD_REQ:
		/* Handle a DFU UPLOAD request. */
		upload_req = (qda_upl_req_payload_t *)pkt->payload;
		/*
		 * UPLOAD requests are handled differently from the others in
		 * order to reuse qda_buf.
		 */
		handle_upload_req(upload_req);
		return;
	case QDA_PKT_DFU_GETSTATUS_REQ:
		/* Handle a DFU GET_STATUS request. */
		retv = dfu_get_status(&status, &state, &poll_timeout);
		if (retv == 0) {
			qda_dfu_get_status_rsp(state, status, poll_timeout);
			return;
		}
		qda_stall();
		return;
	case QDA_PKT_DFU_CLRSTATUS:
		/* Handle a DFU CLEAR_STATUS request. */
		retv = dfu_clr_status();
		if (retv == 0) {
			qda_ack();
			return;
		}
		qda_stall();
		return;
	case QDA_PKT_DFU_GETSTATE_REQ:
		/* Handle a DFU GET_STATE request. */
		retv = dfu_get_state(&state);
		if (retv == 0) {
			qda_dfu_get_state_rsp(state);
			return;
		}
		qda_stall();
		return;
	case QDA_PKT_DFU_ABORT:
		/* Handle a DFU ABORT request. */
		retv = dfu_abort();
		if (retv == 0) {
			qda_ack();
			return;
		}
		qda_stall();
		return;
	case QDA_PKT_RESET:
		/* Handle a reset request. */
		qda_ack();
		qm_soc_reset(QM_COLD_RESET);
		return;
	/* QDA_PKT_DFU_DETACH should not be received */
	/* QDA_PKT_DEV_DESC_REQ is not supported */
	default:
		/* Send a stall message if QDA request is invalid. */
		qda_stall();
		return;
	}
}

/*
 * USB Ack response
 *
 * -------------
 * |4B|TYPE    |
 * -------------
 */
static void qda_ack(void)
{
	static const qda_pkt_t pkt = {.type = QDA_PKT_ACK};

	xmodem_transmit_package((uint8_t *)&pkt, sizeof(pkt));
}

/*
 * USB Stall response
 *
 * -------------
 * |4B|TYPE    |
 * -------------
 */
static void qda_stall(void)
{
	static const qda_pkt_t pkt = {.type = QDA_PKT_STALL};

	xmodem_transmit_package((uint8_t *)&pkt, sizeof(pkt));
}

/*
 * DFU_UPLOAD response
 *
 * -------------
 * |4B|TYPE    |
 * ------------|
 * |2B|DATA_LEN|
 * ------------|
 * |xB|DATA    |
 * -------------
 *
 */
static void handle_upload_req(qda_upl_req_payload_t *req)
{
	uint16_t max_len;
	uint16_t block_num;
	qda_pkt_t *pkt;
	qda_upl_rsp_payload_t *rsp;
	int retv;

	/* Store request parameters in temporary variables. */
	block_num = req->block_num;
	max_len = req->max_data_len;

	/* Prepare upload response packet. */
	pkt = (qda_pkt_t *)qda_buf;
	pkt->type = QDA_PKT_DFU_UPLOAD_RSP;
	rsp = (qda_upl_rsp_payload_t *)pkt->payload;
	retv =
	    dfu_process_upload(block_num, max_len, rsp->data, &rsp->data_len);
	if (retv == 0) {
		xmodem_transmit_package(qda_buf, sizeof(*pkt) + sizeof(*rsp) +
						     rsp->data_len);
	} else {
		qda_stall();
	}
}

/*
 * DFU_GETSTATUS response
 *
 * -----------------
 * |4B|TYPE        |
 * ----------------|
 * |1B|STATUS      |
 * ----------------|
 * |3B|POLL_TIMEOUT|
 * -----------------
 * |1B|STATE       |
 * -----------------
 *
 */
static void qda_dfu_get_status_rsp(dfu_dev_state_t state,
				   dfu_dev_status_t status,
				   uint32_t poll_timeout)
{
	qda_pkt_t *pkt;
	qda_get_status_rsp_payload_t *rsp;

	pkt = (qda_pkt_t *)qda_buf;
	pkt->type = QDA_PKT_DFU_GETSTATUS_RSP;
	rsp = (qda_get_status_rsp_payload_t *)pkt->payload;
	rsp->status = status;
	rsp->poll_timeout = poll_timeout;
	rsp->state = state;

	xmodem_transmit_package(qda_buf, sizeof(*pkt) + sizeof(*rsp));
}

/*
 * DFU_GETSTATE response
 *
 * -----------------
 * |4B|TYPE        |
 * -----------------
 * |1B|STATE       |
 * -----------------
 */
static void qda_dfu_get_state_rsp(dfu_dev_state_t state)
{
	qda_pkt_t *pkt;
	qda_get_state_rsp_payload_t *rsp;

	pkt = (qda_pkt_t *)qda_buf;
	pkt->type = QDA_PKT_DFU_GETSTATE_RSP;
	rsp = (qda_get_state_rsp_payload_t *)pkt->payload;
	rsp->state = state;

	xmodem_transmit_package(qda_buf, sizeof(*pkt) + sizeof(*rsp));
}

/*
 * Reply with a DFU Descriptors response
 *
 * ----------------------
 * |4B|TYPE             |
 * ----------------------
 * |1B|NUM_ALT_SETTINGS |
 * ----------------------
 * |1B|DFU_ATTRIBUTES   |
 * ----------------------
 * |2B|DETACH_TIMEOUT   |
 * ----------------------
 * |2B|MAX BLOCK SIZE   |
 * ----------------------
 * |2B|DFU_VERSION      |
 * ----------------------
 */
static void qda_dfu_dsc_rsp(void)
{
	static const qda_dfu_dsc_rsp_t rsp = {
	    .type = QDA_PKT_DFU_DESC_RSP,
	    .num_alt_settings = DFU_NUM_ALT_SETTINGS,
	    .bm_attributes = DFU_ATTRIBUTES,
	    .detach_timeout = DFU_DETACH_TIMEOUT,
	    .transfer_size = DFU_MAX_BLOCK_SIZE,
	    .bcd_dfu_ver = DFU_VERSION_BCD,
	};

	xmodem_transmit_package((uint8_t *)&rsp, sizeof(rsp));
}
