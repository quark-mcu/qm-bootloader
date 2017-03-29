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

#include "fm_hook.h"
#include "qm_fpr.h"
#include "qm_gpio.h"
#include "qm_mpr.h"
#include "qm_pinmux.h"
#include "qm_soc_regs.h"

#include "clk.h"

#include "fm_entry.h"
#include "fw-manager_config.h"
#include "soc_flash_partitions.h"

/** Check if the FM sticky bit is set */
#define FM_STICKY_BIT_IS_ASSERTED() (QM_SCSS_GP->gps0 & BIT(QM_GPS0_BIT_FM))
/** Set the FM sticky bit */
#define FM_STICKY_BIT_ASSERT() (QM_SCSS_GP->gps0 |= BIT(QM_GPS0_BIT_FM))
/** Clear the FM sticky bit */
#define FM_STICKY_BIT_DEASSERT() (QM_SCSS_GP->gps0 &= ~BIT(QM_GPS0_BIT_FM))

/*
 * FPR configuration for FM mode:
 * - Address range: 0 to max flash size
 * - Allow access only to LMT (i.e., DMA and ARC cannot access any flash
 *   portion)
 * - FPR enabled and locked
 */
#define FM_MODE_FPR_CONFIG                                                     \
	((QM_FPR_HOST_PROCESSOR << QM_FPR_RD_ALLOW_OFFSET) |                   \
	 (FLASH_SIZE_KB << QM_FPR_UPPER_BOUND_OFFSET) |                        \
	 (1 << QM_FPR_ENABLE_OFFSET) | (1 << QM_FPR_WRITE_LOCK_OFFSET))

/*
 * MPR configuration for FM mode:
 * - Address range: 0 to max SRAM size
 * - Allow access only to LMT (i.e., DMA and ARC cannot access any SRAM
 *   portion)
 * - MPR enabled and locked
 */
#define FM_MODE_MPR_CONFIG                                                     \
	((QM_SRAM_MPR_AGENT_MASK_HOST << QM_MPR_WR_EN_OFFSET) |                \
	 (QM_SRAM_MPR_AGENT_MASK_HOST << QM_MPR_RD_EN_OFFSET) |                \
	 (RAM_SIZE_KB << QM_FPR_UPPER_BOUND_OFFSET) | QM_MPR_EN_LOCK_MASK)

static __inline__ void set_up_mpr(void)
{
	/* Configure MPR to disable access by arc and dma. */
	QM_MPR->mpr_cfg[0] = FM_MODE_MPR_CONFIG;
}

static __inline__ void set_up_fpr(void)
{
	int i;
	/* Configure FPR to disable access by arc and dma. */
	for (i = QM_FLASH_0; i < QM_FLASH_NUM; i++) {
		QM_FLASH[i]->fpr_rd_cfg[QM_FPR_0] = FM_MODE_FPR_CONFIG;
	}
}

void fm_hook(void)
{
	/*
	 * Get FM pin status.
	 *
	 * We support both regular GPIO and always-on GPIO. However, they must
	 * be handled differently:
	 * - For AON-GPIO we cannot assume a default configuration since in case
	 *   of warm resets the configuration is not re-initialized
	 *   automatically;
	 * - For regular GPIO, we can rely on their default configuration, but
	 *   we have to handle the pin mixing.
	 */
	qm_gpio_state_t state;

	clk_periph_enable(CLK_PERIPH_REGISTER | CLK_PERIPH_CLK |
			  CLK_PERIPH_GPIO_REGISTER);

#if (FM_CONFIG_ENABLE_GPIO_PIN)
#if (FM_CONFIG_USE_AON_GPIO_PORT)
	qm_gpio_reg_t *const gpio_ctrl = QM_GPIO[QM_AON_GPIO_0];
	gpio_ctrl->gpio_inten &= ~BIT(FM_CONFIG_GPIO_PIN);
	gpio_ctrl->gpio_swporta_ddr &= ~BIT(FM_CONFIG_GPIO_PIN);
	qm_gpio_read_pin(QM_AON_GPIO_0, FM_CONFIG_GPIO_PIN, &state);
#else /* FM GPIO is a regular GPIO */
	qm_pmux_select(FM_CONFIG_GPIO_PIN, QM_PMUX_FN_0);
	qm_pmux_pullup_en(FM_CONFIG_GPIO_PIN, true);
	qm_pmux_input_en(FM_CONFIG_GPIO_PIN, true);
	/* No need to configure the GPIO, default configuration is okay. */
	qm_gpio_read_pin(QM_GPIO_0, FM_CONFIG_GPIO_PIN, &state);
	qm_pmux_pullup_en(FM_CONFIG_GPIO_PIN, false);
#endif
#else  /* Don't check FM GPIO status */
	state = QM_GPIO_HIGH;
#endif /* FM_CONFIG_ENABLE_GPIO_PIN */

	/* Enter FM mode if FM sticky bit is set or FM_CONFIG_GPIO_PIN is low */
	if (FM_STICKY_BIT_IS_ASSERTED() || (state == QM_GPIO_LOW)) {
		FM_STICKY_BIT_DEASSERT();
		fm_secure_entry();
	}
}

void fm_secure_entry(void)
{
	set_up_fpr();
	set_up_mpr();
	/* Run the firmware management code; fm_entry() never returns. */
	fm_entry();
}
