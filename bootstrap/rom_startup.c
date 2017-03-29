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

#include "boot.h"
#include "boot_clk.h"
#include "flash_layout.h"
#include "interrupt/idt.h"
#include "power_states.h"
#include "qm_interrupt.h"
#include "qm_pinmux.h"
#include "soc_boot.h"
#include "qm_mpr.h"
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

/* Stringify macros. */
#define _STR(x) #x
#define STR(x) _STR(x)

/* Factory settings for Crystal Oscillator */
/* 7.45 pF load cap for Crystal */
#define OSC0_CFG1_OSC0_FADJ_XTAL_DEFAULT (0x4)
/* Crystal count value set to 5375 */
#define OSC0_CFG0_OSC0_XTAL_COUNT_VALUE_DEFAULT (0x2)
/*
 * Lower bound for BL-Data FPR protection.
 *
 * The FPR starts after the first kB of BL-Data Main since the first kB
 * contains information (such as trim codes) that must be accessible by the
 * application.
 *
 * Note: QM_FPR_GRANULARITY = 1kB for both Quark SE C1000 and Quark D2000.
 */
#define BL_DATA_FPR_LOW_BOUND                                                  \
	((BL_DATA_SECTION_MAIN_PAGE * QM_FLASH_PAGE_SIZE_BYTES +               \
	  QM_FPR_GRANULARITY) /                                                \
	 QM_FPR_GRANULARITY)
/*
 * Upper bound for BL-Data protection.
 *
 * The FPR ends at the end of the BL-Data Backup page.
 *
 * Note: Upper bound computation is different for Quark SE C1000 and Quark
 * D2000:
 *
 * - On Quark D2000, we must specify the last kB we want to protect; for
 *   instance, if we want to protect the first kB of flash (i.e., address range
 *   from 0 to 0x3FF), we must specify 0 for both lower and upper bound.
 *
 * - On Quark SE C1000, we must specify the first kB we do not want to protect;
 *   for instance, if we want to protect the first kB of flash (i.e., address
 *   range from 0 to 0x3FF), we must specify 0 as the lower bound and 1 as the
 *   upper bound.
 */
#if (QUARK_SE)
#define BL_DATA_FPR_UP_BOUND                                                   \
	(((BL_DATA_SECTION_BACKUP_PAGE + 1) * QM_FLASH_PAGE_SIZE_BYTES) /      \
	 QM_FPR_GRANULARITY)
#else
#define BL_DATA_FPR_UP_BOUND                                                   \
	(((BL_DATA_SECTION_BACKUP_PAGE + 1) * QM_FLASH_PAGE_SIZE_BYTES - 1) /  \
	 QM_FPR_GRANULARITY)
#endif

#define BL_DATA_ROM_VERSION_OFFSET_MASK 0x3FFFF

/* FPR settings: enabled and locked for bl_data, no agent allowed. */
#define BL_FPR_CONFIG                                                          \
	((QM_FPR_LOCK_ENABLE << QM_FPR_ENABLE_OFFSET) |                        \
	 (BL_DATA_FPR_LOW_BOUND |                                              \
	  (BL_DATA_FPR_UP_BOUND << QM_FPR_UPPER_BOUND_OFFSET)) |               \
	 (QM_FPR_LOCK))

/*
 * MPR 0 configuration for Lakemont's stack + IDT + GDT:
 * - Address range: Last 1Kb from SRAM.
 * - Allow access only to LMT (i.e., DMA, ARC and USB agents can't access it).
 * - MPR enabled and locked.
 */
#define LAKEMONT_MPR_CONFIG                                                    \
	((QM_MPR_EN_LOCK_MASK) |                                               \
	 (QM_SRAM_MPR_AGENT_MASK_HOST << QM_MPR_RD_EN_OFFSET) |                \
	 (QM_SRAM_MPR_AGENT_MASK_HOST << QM_MPR_WR_EN_OFFSET) |                \
	 ((RAM_SIZE_KB - 1) << QM_FPR_UPPER_BOUND_OFFSET) | (RAM_SIZE_KB - 1))

static __inline__ void set_up_mpr(void)
{
	/* Configure MPR to disable access by arc and dma. */
	QM_MPR->mpr_cfg[0] = LAKEMONT_MPR_CONFIG;
}

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
 * System clock settings.
 */
static __inline__ void clock_setup(void)
{
#if (HAS_HYB_XTAL)
	/*
	 * Apply factory settings for Crystal Oscillator stabilization
	 * These settings adjust the trimming value and the counter value
	 * for the Crystal Oscillator.
	 */
	QM_SCSS_CCU->osc0_cfg1 &= ~OSC0_CFG1_OSC0_FADJ_XTAL_MASK;
	QM_SCSS_CCU->osc0_cfg1 |=
	    (OSC0_CFG1_OSC0_FADJ_XTAL_DEFAULT << OSC0_CFG1_OSC0_FADJ_XTAL_OFFS);
	QM_SCSS_CCU->osc0_cfg0 &= ~OSC0_CFG0_OSC0_XTAL_COUNT_VALUE_MASK;
	QM_SCSS_CCU->osc0_cfg0 |= (OSC0_CFG0_OSC0_XTAL_COUNT_VALUE_DEFAULT
				   << OSC0_CFG0_OSC0_XTAL_COUNT_VALUE_OFFS);
#endif /* HAS_HYB_XTAL */

	/* Switch to 32MHz silicon oscillator. */
	boot_clk_hyb_set_mode(CLK_SYS_HYB_OSC_32MHZ, CLK_SYS_DIV_1);
}

/*
 * SCSS interrupt routing initialization.
 */
static __inline__ void irq_setup(void)
{
	uint8_t i;

	/* Apply POR settings to SCSS int routing as SCSS regs are sticky. */
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
	 * NOTE: Try to write the ROM version in conjunction with the trim codes
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
/*
 * When FM mode is enabled, bl-data initialization is done by
 * bl_data_sanitize(), while when FM mode is disabled, we have to
 * initialize bl-data manually (in this case the initialization
 * consists in computing and storing trim-codes and shadowing the ROM
 * version).
 */
#if (ENABLE_FIRMWARE_MANAGER)
	bl_data_sanitize();
#else
	/*
	 * Check if trim codes are present in BL-Data, if not compute and store
	 * them.
	 */
	boot_clk_trim_code_check_and_setup();

	/* Shadow the ROM Version in bl-data (if not already present). */
	shadow_rom_version();
#endif
}

/*
 * Set violation policy for both SRAM and flash to 'warm reset'.
 */
static __inline__ void set_violation_policy(void)
{
	int i;
	volatile uint32_t *const int_flash_controller_mask =
	    &QM_INTERRUPT_ROUTER->flash_mpr_0_int_mask;

	/* Make halt interrupts trigger a reset. */
	QM_SCSS_PMU->p_sts &= ~QM_P_STS_HALT_INTERRUPT_REDIRECTION;

	/* Enable halt interrupts for SRAM controller. */
	QM_IR_UNMASK_HALTS(QM_INTERRUPT_ROUTER->sram_mpr_0_int_mask);

	/* Enable halt interrupts for every flash controllers.*/
	for (i = QM_FLASH_0; i < QM_FLASH_NUM; i++) {
		QM_IR_UNMASK_HALTS(int_flash_controller_mask[i]);
	}
	/*
	 * Note: at this point, for extra security, we should set the
	 * LOCK_HOST_HALT_MASK bit in the LOCK_INT_MASK_REG register, in order
	 * to lock the halt masks fields that we just set. However, doing so
	 * will lock the host processor halt mask fields for every peripheral,
	 * thus preventing the application from unmasking other halt interrupts
	 * if needed.
	 *
	 * Therefore, we do not enable the lock. Application developers are
	 * recommended to change this function by unmasking all the halt
	 * interrupts they need and then locking the mask configuration.
	 */
}

/*
 * Write-protect the flash areas where BL-Data and firmware reside.
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
 * Protect bl_data against read operations, with the exception of trim codes
 * and ROM version.
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
	/* Clear all ISRs. */
	idt_init();

	/* Invalidate the cache. */
	__asm__ __volatile__("wbinvd");

	/* Clear general purpose registers. */
	__asm__ __volatile__("xor %eax, %eax\n\t"
			     "xor %ebx, %ebx\n\t"
			     "xor %ecx, %ecx\n\t"
			     "xor %edx, %edx\n\t");
}

/* Set-up security context for application and boot it. */
static void secure_app_entry(void)
{
	extern uint32_t __esram_start[];
	extern uint32_t __esram_size[];

#if (ENABLE_FLASH_WRITE_PROTECTION)
	/*
	 * Before jumping to LMT application, let's write protect the flash.
	 */
	write_protect_flash();
#endif /* ENABLE_FLASH_WRITE_PROTECTION */

	/* Read protect bl_data, but the trim codes in the main copy. */
	bl_data_fpr_setup();

	/*
	 * Do some cleanup before calling the user app to avoid data leaking.
	 */
	clean_bootloader_traces();

	/* Setup MPR_0 so it protects Lakemont's stack, GDT and IDT. */
	set_up_mpr();

	/* Clean-up SRAM (but not stack).*/
	memset(__esram_start, 0x0, (size_t)__esram_size);

	/* Reset the stack pointer and clear the stack. */
	__asm__ __volatile__("movl $__stack_start, %esp");
	/* Note: stack_end is the top of the stack, i.e., the lower address. */
	__asm__ __volatile__("xor %eax, %eax\n\t"
			     "movl $__stack_end, %edi\n\t"
			     "movl $__stack_size, %ecx\n\t"
			     "rep stosb");
	/*
	 * Jump to the x86 application (must be done in assembly since we do
	 * not have a valid stack anymore).
	 */
	__asm__ __volatile__("jmp " STR(LMT_APP_ADDR) "\n\t");
}

/*
 * C runtime initialization.
 * This will be called from rom_startup.s
 */
void __attribute__((noreturn)) rom_startup(void)
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

	/* Apply trim code calibration. */
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
#if (QUARK_SE)
	/*
	 * Disable Sensor Subsystem code protection region, to prevent
	 * malicious code from using it against ARC.
	 *
	 * The code protection region (PROT_RANGE register in SS_CFG) is 0 by
	 * default, so locking the default configuration is enough.
	 *
	 * The lock persists in case of warm reset.
	 */
	QM_SCSS_PERIPHERAL->cfg_lock |= QM_SCSS_CFG_LOCK_PROT_RANGE_LOCK;
#endif
	/*
	 * Set memory violation policy. The policy for FM mode and application
	 * context is the same: trigger a warm reset.
	 *
	 * The policy is not locked, so applications can change it if required.
	 */
	set_violation_policy();
#if (ENABLE_FIRMWARE_MANAGER)
	/* Check if we must enter FM mode and if so enter it. */
	fm_hook();
#endif
	/*
	 * Execute application on Lakemont, provided that the application has
	 * been programmed.
	 */
	if (0xffffffff != *(uint32_t *)LMT_APP_ADDR) {
		secure_app_entry();
	} else {
#if (ENABLE_FIRMWARE_MANAGER)
		/* Enter FM mode if no valid application has been found.*/
		fm_secure_entry();
#endif
	}
	/* Loop infinitely to avoid the compiler complaining that we return. */
	while (1)
		;
}
