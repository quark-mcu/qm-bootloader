/*
 * Copyright (c) 2016, Intel Corporation
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

#include "boot.h"
#include "boot_clk.h"
#include "flash_layout.h"
#include "interrupt/idt.h"
#include "power_states.h"
#include "qm_interrupt.h"
#include "qm_pinmux.h"
#include "soc_boot.h"
#include "qm_fpr.h"
#include "bl_data.h"
#include <string.h>
/* Firmware manager hook */
#include "fm_entry.h"
#include "fm_hook.h"
#include "bl_data.h"
#include "rom_version.h"

#if (DEBUG)
static QM_ISR_DECLARE(double_fault_isr)
{
	soc_boot_halt_cpu();
}
#endif

/* Factory settings for Crystal Oscillator */
/* 7.45 pF load cap for Crystal */
#define OSC0_CFG1_OSC0_FADJ_XTAL_DEFAULT (0x4)
/* Crystal count value set to 5375 */
#define OSC0_CFG0_OSC0_XTAL_COUNT_VALUE_DEFAULT (0x2)
/* Upper and lower bounds for bl_data fpr protecction */
#define BL_DATA_FPR_LOW_BOUND                                                  \
	(((BL_DATA_SECTION_MAIN_PAGE * QM_FLASH_PAGE_SIZE_BYTES) / FPR_SIZE) + \
	 1)
#define BL_DATA_FPR_UP_BOUND                                                   \
	(((BL_DATA_SECTION_BACKUP_PAGE * QM_FLASH_PAGE_SIZE_BYTES) /           \
	  FPR_SIZE) +                                                          \
	 1)

#define BL_DATA_ROM_VERSION_OFFSET_MASK 0x3FFFF

/* FPR settings: enabled and locked for bl_data, no agent allowed. */
#define BL_FPR_CONFIG                                                          \
	((QM_FPR_LOCK_ENABLE << QM_FPR_ENABLE_OFFSET) |                        \
	 (BL_DATA_FPR_LOW_BOUND |                                              \
	  (BL_DATA_FPR_UP_BOUND << QM_FPR_UPPER_BOUND_OFFSET)) |               \
	 (QM_FPR_LOCK))

/*
 * System power settings
 */
static __inline__ void power_setup(void)
{
#if (QUARK_SE)
	/* Pin MUX slew rate settings */
	QM_PMUX_SLEW0 = QM_PMUX_SLEW_4MA_DRIVER;
	QM_PMUX_SLEW1 = QM_PMUX_SLEW_4MA_DRIVER;
	QM_PMUX_SLEW2 = QM_PMUX_SLEW_4MA_DRIVER;
	QM_PMUX_SLEW3 = QM_PMUX_SLEW_4MA_DRIVER;
#endif
	/*
	 * On Quark D2000, all pins are 12 mA by default; this should be fine
	 * for now.
	 */
}

/*
 * System clock settings
 */
static __inline__ void clock_setup(void)
{
#if (HAS_HYB_XTAL)
	/* Apply factory settings for Crystal Oscillator stabilization
	 * These settings adjust the trimming value and the counter value
	 * for the Crystal Oscillator */
	QM_SCSS_CCU->osc0_cfg1 &= ~OSC0_CFG1_OSC0_FADJ_XTAL_MASK;
	QM_SCSS_CCU->osc0_cfg1 |=
	    (OSC0_CFG1_OSC0_FADJ_XTAL_DEFAULT << OSC0_CFG1_OSC0_FADJ_XTAL_OFFS);
	QM_SCSS_CCU->osc0_cfg0 &= ~OSC0_CFG0_OSC0_XTAL_COUNT_VALUE_MASK;
	QM_SCSS_CCU->osc0_cfg0 |= (OSC0_CFG0_OSC0_XTAL_COUNT_VALUE_DEFAULT
				   << OSC0_CFG0_OSC0_XTAL_COUNT_VALUE_OFFS);
#endif /* HAS_HYB_XTAL */

	/* Switch to 32MHz silicon oscillator */
	boot_clk_hyb_set_mode(CLK_SYS_HYB_OSC_32MHZ, CLK_SYS_DIV_1);
}

/*
 * SCSS interrupt routing initialization.
 */
static __inline__ void irq_setup(void)
{
	uint8_t i;

	/* Apply POR settings to SCSS int routing as SCSS regs are sticky */
	for (i = 0; i < QM_INTERRUPT_ROUTER_MASK_NUMREG; i++) {
		*((volatile uint32_t *)QM_INTERRUPT_ROUTER + i) =
		    QM_INTERRUPT_ROUTER_MASK_DEFAULT;
	}
}

/*
 * Shadow the ROM Version in the unprotected region of the flash.
 */
static __inline__ void shadow_rom_version(void)
{
	bl_data_t *bl_data;

	bl_data = (bl_data_t *)BL_DATA_SECTION_MAIN_ADDR;
	/*
	 * NOTE: Try to write the rom version in conjuction with the trim codes
	 * to reduce footprint and wear.
	 */
	if (bl_data->rom_version != QM_VER_ROM) {
		qm_flash_word_write(BL_DATA_FLASH_CONTROLLER,
				    BL_DATA_FLASH_REGION,
				    ((uint32_t)&bl_data->rom_version &
				     BL_DATA_ROM_VERSION_OFFSET_MASK),
				    QM_VER_ROM);
	}
}

static __inline__ void bl_data_sanitize_wrap(void)
{
#if (ENABLE_FIRMWARE_MANAGER)
	bl_data_sanitize();
#else
	boot_clk_trim_code_check_and_setup();

	/* Shadow the ROM Version in the unprotected region of the flash. */
	shadow_rom_version();
#endif
}

/*
 * Protects the flash areas where bl_data and firmware reside
 */
static __inline__ void write_protect_flash(void)
{
	uint8_t ctrl_idx;

	/* Write disable all flash controllers */
	for (ctrl_idx = 0; ctrl_idx < QM_FLASH_NUM; ctrl_idx++) {
		QM_FLASH[ctrl_idx]->ctrl |= QM_FLASH_WRITE_DISABLE_VAL;
	}
}

/*
 * Protect bl_data against read operations, but the trim codes.
 */
static __inline__ void bl_data_fpr_setup(void)
{
	QM_FLASH[BL_DATA_FLASH_CONTROLLER]->fpr_rd_cfg[QM_FPR_0] =
	    BL_FPR_CONFIG;
}

/*
 * Avoid possible data leaking when jumping to the user application by clearing
 * the general purpose registers, cache and ISRs.
 */
static __inline__ void clean_bootloader_traces(void)
{
	/* Clear all ISRs */
	idt_init();

	/* Invalidate the cache */
	__asm__ __volatile__("wbinvd");

	/* Clear general purpose registers */
	__asm__ __volatile__("movl $0, %eax\n\t"
			     "movl $0, %ebx\n\t"
			     "movl $0, %ecx\n\t"
			     "movl $0, %edx\n\t");
}

/* Set-up security context for application and boot it. */
static void secure_app_entry(void)
{
	void (*lmt_app_entry)(void) = (void *)LMT_APP_ADDR;
	extern uint32_t __esram_start[];
	extern uint32_t __esram_size[];

	/* The flash CTRL register will be set after RAM is erased, therefore we
	 * cannot use qm_flash to access it as that variable lives in esram, we
	 * use instead a stack variable as the stack frame is not erased. */
	volatile uint32_t *flash_ctrl_reg_p = &QM_FLASH[QM_FLASH_0]->ctrl;

#if (ENABLE_FLASH_WRITE_PROTECTION)
	/*
	 * Before jumping to LMT application, let's write protect the flash.
	 */
	write_protect_flash();
#endif /* ENABLE_FLASH_WRITE_PROTECTION */

	/* Read protect bl_data, but the trim codes in the main copy */
	bl_data_fpr_setup();

	/*
	 * Do some cleanup before calling the user app to avoid data leaking.
	 */
	clean_bootloader_traces();

	/* Clean-up SRAM (but not stack).*/
	memset(__esram_start, 0x0, (size_t)__esram_size);

	/* Reset the stack pointer. */
	__asm__ __volatile__("movl $__stack_start, %esp");

	/* Enable read-protection for ROM flash. */
	*flash_ctrl_reg_p |= (ROM_RD_DIS_U | ROM_RD_DIS_L);

	lmt_app_entry();
}

/*
 * C runtime initialization.
 * This will be called from rom_startup.s
 */
void rom_startup(void)
{
	extern uint32_t __bss_start[];
	extern uint32_t __data_vma[];
	extern uint32_t __data_lma[];
	extern uint32_t __data_size[];
	extern uint32_t __bss_size[];

	/* Zero out bss */
	memset(__bss_start, 0x00, (size_t)__bss_size);

	/* Copy initialised variables */
	memcpy(__data_vma, __data_lma, (size_t)__data_size);

	power_setup();
	clock_setup();
	boot_sense_jtag_probe();

	/*
	 * Check and initialize trim codes and, if FW manager is enabled, also
	 * check and sanitize boot loader partitions.
	 *
	 * NOTE: the following function may perform flash writing, however,
	 * there is no need to explicitly initialize flash controller(s) since
	 * the default configuration (i.e., write enabled and flash configured
	 * for 32 MHz) is restored at every boot (even after warm resets) and
	 * is fine.
	 */
	bl_data_sanitize_wrap();

	/* Apply trim code calibration */
	clk_trim_apply(QM_FLASH_DATA_TRIM_CODE->osc_trim_32mhz);

	/* Interrupt initialisation */
	irq_setup();
	idt_init();
	boot_aon_handle_spurious_irq();
#if (DEBUG)
	qm_int_vector_request(QM_X86_DOUBLE_FAULT_INT, double_fault_isr);
#endif
	soc_boot_init_interrupt_controller();
	__asm__ __volatile__("sti");

#if (ENABLE_FIRMWARE_MANAGER)
	fm_hook();
#endif /* ENABLE_FIRMWARE_MANAGER */

	/*
	 * Execute application on Lakemont, provided that the application has
	 * been programmed.
	 */
	if (0xffffffff != *(uint32_t *)LMT_APP_ADDR) {
		secure_app_entry();
	} else {

/* Enter FM mode when no valid application has been found.*/
#if (ENABLE_FIRMWARE_MANAGER)
		fm_secure_entry();
#endif /* ENABLE_FIRMWARE_MANAGER */
	}
}
