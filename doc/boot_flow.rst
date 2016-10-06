BOOTLOADER FLOW
###############

This document describes the flow of the bootloader, from hw boot to the start
of the application.

The bootloader has some dedicated resources, these are described in the
`resources document <../boot_resources.rst>`_.

FLOW
****

#. Assembly start-up:
     - Invalidate cache [Quark SE only].
     - Store Built-in-Self-Test (BIST) value in EBP.
     - Load the GDT table into the descriptor.
     - Move to 32bit protected mode.
     - Enable cache  [Quark SE only].

#. Set-up primary peripherals and registers:
     - Stack pointer set-up
     - RAM set-up
     - Power set-up
     - Clock set-up:
       Check if the trim codes are stored in flash. Compute trim code
       and store them in flash if no trim codes were found.

#. JTAG probe hook:
     - The bootloader checks if the JTAG_PROBE_PIN is asserted (grounded). It
       will continue once the pin is de-asserted(ungrounded). This is used to
       unbrick a device with firmware that is preventing JTAG from working
       correctly.

#. Set-up secondary peripherals:
     #. IRQ set-up
     #. IDT set-up
     #. Set-up interrupt controller

#. Configure flash controller(s): [Compile option: ``ENABLE_FIRMWARE_MANAGER=1``]
     - Configure flash partition 0.
     - Configure flash partition 1 if the SoC is Quark SE.

#. Sanitize bootloader data: [Compile option: ``ENABLE_FIRMWARE_MANAGER=1``]
     - The bootloader checks if the meta-data partitions are not corrupted and
       fixes them if need.

#. Firmware management(FM): [Compile option: ``ENABLE_FIRMWARE_MANAGER=1``]
     - The bootloader goes into Firmware management mode if the FM enable pin
       is asserted (grounded) or when sticky register ``GPS0`` is set.

#. Start the application:
     - Start x86 application if present (the first double word is different
       from 0xffffffff).

#. Sleep:
      - When the x86 application returns, the bootloader performs different
        actions depending on the SoC.

      Quark D2000:
        * The SoC goes into deep sleep mode.

      Quark SE:
        * The x86 core is halted (C2 power state).
        * No special action is performed for the sensor core.
