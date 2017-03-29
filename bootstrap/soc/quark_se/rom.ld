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

ENTRY(_start)

MEMORY
{
	otp_lock (r)	: ORIGIN = 0xFFFFE000, LENGTH = 4
	/*
	 * ROM unused space before manufacturing data:
	 * 512 (entire ROM data area) - 4 (opt lock size) - 16 (used ROM data).
	 */
	rom_extra(r)	: ORIGIN = 0xFFFFE004, LENGTH = 512 - 4 - 16
	/* ROM manufacturing data (just 16 bytes for trim codes) */
	rom_data (r)	: ORIGIN = 0xFFFFE1F0, LENGTH = 16
	/* ROM: 8K - reset vector - previous space (512 bytes) */
	rom (r)		: ORIGIN = 0xFFFFE200, LENGTH = 8K - 16 - 512
	reset_vector (r): ORIGIN = 0xFFFFFFF0, LENGTH = 16
	/* Sensor Subsystem vector table (68 entries) */
	sensor_vectors (rw) : ORIGIN = 0xA8000000, LENGTH = 68
	/*
	 * ESRAM: half RAM size - stack - IDT
	 *
	 * TODO: try with full RAM size (80K), since ARC is off while the
	 * 2nd-stage bootloader runs.
	 */
	esram (rw)	: ORIGIN = 0xA800A000, LENGTH = 40K - 1K - 0x220
	/*
	 * STACK: 1K - restore_info (4 bytes) - GDT (32 bytes)
	 *
	 */
	stack (rw)	: ORIGIN = 0xA80139E0, LENGTH = 1K - 0x4 - 0x20
	/* Interrupt descriptor table (IDT) (68 gates * 8 bytes) */
	esram_idt (rw)	: ORIGIN = 0xA8013DBC, LENGTH = 0x220
	/* Shared RAM */
	esram_restore_info (rw) : ORIGIN = 0xA8013FDC, LENGTH = 0x4
	esram_gdt (rw)  : ORIGIN = 0xA8013FE0, LENGTH = 0x20
}

/* Sensor Subsystem reset vector definition */
__sensor_reset_vector = ORIGIN(sensor_vectors);

/* Stack definition */
__stack_start = ORIGIN(stack) + LENGTH(stack);
__stack_end = ORIGIN(stack);
__stack_size = LENGTH(stack);

/* RAM definition */
__esram_start = ORIGIN(esram);
__esram_size = LENGTH(esram);

/* IDT definition */
__idt_start = ORIGIN(esram_idt);
__idt_end = __idt_start + LENGTH(esram_idt);

/* GDT in RAM definition */
__gdt_ram_start = ORIGIN(esram_gdt);
__gdt_ram_end = __gdt_ram_start + LENGTH(esram_gdt);

/* Shared RAM definition */
__x86_restore_info = ORIGIN(esram_restore_info);

SECTIONS
{
	/* Reserve area for otp_lock and manufacturing data */
	. = ORIGIN(otp_lock);
	.otp_lock :
	{
		/*
		 * We must put some content in this section in order to have
		 * the binary image start from the OTP start address (since
		 * objcopy starts the binary image from the first non-empty
		 * section).
		 *
		 * Note: unfortunately the 'size' command sees this section as
		 * 'data' and therefore erroneously increases reported size for
		 * 'data' by 1 byte.
		 */
		BYTE(0xFF)
	} >otp_lock
	/* No need to define .rom_data; objcopy fills empty space with 0xFF */

	. = ORIGIN(reset_vector);
	.reset_vector :
	{
		*(.text.entry)
		*reset_vector.s.o (.text)
	} >reset_vector

	. = ORIGIN(rom_extra);
	/*
	 * Put all TinyCrypt SHA256 functions into a special section in
	 * rom_extra. They will almost fill rom_extra since their total size
	 * when compiled with -Os is 468 bytes.
	 * Note: when compiled with -Og, their total size is 828 bytes, so they
	 * will not fit in this section; however, this is not currently a
	 * problem since a ROM compiled with the Firmware Manager in DEBUG mode
	 * is going to overflow anyway.
	 */
	.text_tc_sha256 :
	{
		*(.text.tc_sha256*)
	} >rom_extra

	. = ORIGIN(rom);
	.text :
	{
		*(.text.entry)
		*(.text.last)
		*(.text.*)
	} >rom

	.rodata :
	{
		*(.rdata*)
		*(.rodata*) /* .rodata* sections (constants, strings, etc.) */
	} >rom

	.data :
	{
		*(.data*)
	} >esram  AT>rom

	.bss :
	{
		*(.bss*)
		*(COMMON)
	} >esram  AT>esram

	/* Symbols for C runtime init code. */
	__data_lma = LOADADDR(.data);
	__data_vma = ADDR(.data);
	__data_size = SIZEOF(.data);
	__bss_start = ADDR(.bss);
	__bss_size = SIZEOF(.bss);

	.comment 0 : { *(.comment) }
}
