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

#ifndef __BOOT_CLK_H__
#define __BOOT_CLK_H__

#include "qm_common.h"
#include "qm_soc_regs.h"
#include "clk.h"
#include "flash_layout.h"

/**
 * Bootloader clocking functions.
 *
 * @defgroup groupBOOTCLK Bootloader clock
 * @{
 */

/**
 * Set clock mode and divisor for the hybrid oscillator.
 *
 * Change the operating mode and clock divisor of the hybrid clock source.
 * Changing this clock speed affects all peripherals.
 *
 * @param[in] mode System clock source operating mode.
 * @param[in] div  System clock divisor.
 *
 * @return Resulting status code.
 * @retval 0 if successful.
 */
int boot_clk_hyb_set_mode(const clk_sys_mode_t mode, const clk_sys_div_t div);

/**
 * Setup trim-codes if needed.
 *
 * Check if trim codes are already in flash. If not they are either copied
 * from OTP to flash, or, if not available in OTP, computed and stored in
 * flash.
 *
 * This function is expected to be called only during the first boot if
 * firmware manager is not enabled.
 */
void boot_clk_trim_code_check_and_setup(void);

/**
 * Populate output parameter with trim codes.
 *
 * For each frequency, this function checks whether the corresponding code is
 * in OTP. Otherwise it is directly computed.
 *
 * This function is expected to be called only during the first boot if
 * firmware manager is enabled.
 *
 * @param[out] data where trim codes are stored into. Must not be null.
 *
 * @return Resulting status code.
 * @retval 0 if successful.
 */
int boot_clk_trim_code_compute(qm_flash_data_trim_t *const ptr_trim_codes);

/**
 * @}
 */

#endif /* __BOOT_CLK_H__ */
