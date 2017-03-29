BOOTLOADER FLOW
###############

This document describes the flow of the bootloader, from hardware boot to the
start of the application.

The bootloader has some dedicated resources, they are described in the
`Bootloader Resources document <boot_resources.rst>`_.

FLOW
****

#. Initialize x86 core:
     - Invalidate cache [Quark SE only].
     - Store Built-in-Self-Test (BIST) value in EBP.
     - Load the GDT table into the descriptor.
     - Move to 32bit protected mode.
     - Enable cache  [Quark SE only].

#. Check resume from sleep condition: [Quark SE only; compile option: ``ENABLE_RESTORE_CONTEXT=1``]
     - Resume application execution if the device was put into sleep mode. A
       soft reboot is performed when a device comes out of sleep mode. The
       bootloader checks if the 'restore bit' of ``GPS0`` sticky register is
       set.

         + ['restore bit' set]
             * Set up security hardening:
                 - Enable flash write protection. [Compile option: ``ENABLE_FLASH_WRITE_PROTECTION=1``]
                 - Set-up BL-Data FPR (``FPR 0``).
                 - Set-up IDT/GDT MPR (``MPR 0``).
             * Jump to the restored address stored in SRAM.
         + ['restore bit' not set]
                * Continue normal boot.

#. Set-up primary peripherals and registers:
     - Stack pointer set-up
     - RAM set-up
     - Power set-up
     - Clock set-up

#. Check for JTAG probe:
     - The bootloader checks if the JTAG_PROBE_PIN is asserted (grounded) and,
       if so, it waits until the pin is de-asserted (ungrounded). This is used
       to unbrick a device with firmware that is preventing JTAG from working
       correctly.

#. Initialize / sanitize Bootloader Data (BL-Data):
     - The bootloader checks if BL-Data is blank or corrupted:
         + [BL-Data blank]
             * Initialize BL-Data (including trim-code computation).
         + [One copy of BL-Data corrupted]
             * Recover BL-Data using valid copy.
         + [Both copies corrupted]
             * Enter infinite loop (unrecoverable error).
     - Sanitize partitions:
         + Check for partition marked as 'inconsistent' and erase them.

#. Set-up secondary peripherals:
     #. IRQ set-up
     #. IDT set-up
     #. Enable interrupts

#. Set memory violation policy:
     - Configure memory violation policy (for both RAM and flash) to trigger a
       warm reset.

#. Check if Firmware Management (FM) is requested: [Compile option: ``ENABLE_FIRMWARE_MANAGER=[uart|2nd-stage]``]
     - The bootloader check if the FM pin is asserted (grounded) or the FM bit
       of sticky register ``GPS0`` is set; if so, it enters FM mode.

#. Check if x86 application is present:
     - Check if the first 4 bytes of the x86 partition are different from
       0xffffffff (i.e., an application is present):

         + [Application present]
             * Set up security hardening:
                 - Enable flash write protection. [Compile option: ``ENABLE_FLASH_WRITE_PROTECTION=1``]
                 - Set-up BL-Data FPR (``FPR 0``).
                 - Set-up IDT/GDT MPR (``MPR 0``).
             * Clean-up RAM to prevent leaking private data to application:
                 - Clear x86 portion of RAM.
                 - Reset stack pointer.
             * Start x86 application:
                 - Jump to the application entry point.
         + [Application not present]
             * Start FM mode. [Compile option: ``ENABLE_FIRMWARE_MANAGER=[uart|2nd-stage]``]

#. Enter infinite loop:
     - If no x86 application is present and FM is not enabled, or the
       application returns, the bootloader enters an infinite loop.
