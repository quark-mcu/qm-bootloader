BOOTLOADER RESOURCES
####################

.. contents::

The bootloader uses dedicated resources to guarantee its flow. Hardware
designers and application developers should pay attention how they use the
resources to avoid unexpected behavior. The bootloader will not use resources
associated with a specific compile option if such an option is not set. Some
used resources are freed before starting the application, while others remain
reserved.

Overview
--------

+------------------+---------------+----------------+------------------+
| Resource         | Quark D2000   | Quark SE C1000 | Notes            |
+==================+===============+================+==================+
| Flash            | | 0x00200000  | | 0x4002F000   |                  |
| (for BL-Data)    | | 0x00201000  | | 0x40030000   | Reserved         |
+------------------+---------------+----------------+------------------+
| Sleep storage    | N/A           | | 0xA8013FDC   | Reserved         |
| (in RAM)         |               | | 0xA8013FE0   |                  |
+------------------+---------------+----------------+------------------+
| FM register      | GPS0 bit 0    | GPS0 bit 0     | Reserved         |
+------------------+---------------+----------------+------------------+
| Sleep register   | N/A           | GPS0 bit 1     | x86 restore bit  |
|                  | N/A           | GPS0 bit 2     | arc restore bit  |
+------------------+---------------+----------------+------------------+
| JTAG probe       | GPIO_13       | GPI0_15        | Do not ground    |
+------------------+---------------+----------------+------------------+
| FM GPIO pin      | GPIO_2        | AON_GPIO_4     | Do not ground    |
+------------------+---------------+----------------+------------------+
| UART port        | Uart 0        | Uart 1         | Available to app |
+------------------+---------------+----------------+------------------+
| USB controller   | N/A           | USB 0          | Available to app |
+------------------+---------------+----------------+------------------+
| Flash protection | FPR 0         | FPR 0          | Locked           |
| (for BL-Data)    |               |                |                  |
+------------------+---------------+----------------+------------------+
| SRAM protection  | FPR 0         | FPR 0          | Locked           |
| (for IDT and GDT)|               |                |                  |
+------------------+---------------+----------------+------------------+

GPIO pins
*********

The bootloader uses the following pins:

* JTAG probe pin:
    - Quark SE C1000:       ``GPIO_15``
    - Quark D2000:          ``GPIO_13``

  This pin is used to put the bootloader into recovery mode. Recovery mode can
  be used when the JTAG is not able to connect to the device during runtime.

* FM pin:  [Compile option: ``ENABLE_FIRMWARE_MANAGER=[uart|2nd-stage]``]
    - Quark SE C1000:       ``AON_GPIO_4``
    - Quark D2000:    ``GPIO_2``

  This pin is used to start the bootloader in Firmware Management (FM) mode.

.. warning:: GPIO pins used by the bootloader should not be grounded during the
             boot process.

Sticky registers
****************

The bootloader uses the following sticky registers:

* Resume from sleep:  [Compile option: ``ENABLE_RESTORE_CONTEXT=1``]
    - Quark SE C1000:    ``GPS0 bit1 and the 4 bytes in esram_shared``

  The bootloader will use the sticky register to handle resuming the
  application from a sleep power state as well as the 4 bytes of the
  common RAM defined as a section esram_restore_info will be used to
  save the restore trap address.

* FM sticky bit:  [Compile option: ``ENABLE_FIRMWARE_MANAGER=[uart|2nd-stage]``]
    - All SoCs:    ``GPS0 bit 0``

  This register is used to start the bootloader in Firmware Management (FM)
  mode.

.. warning:: The application must not use the sticky registers used by the
             bootloader.

Peripherals
***********

The following peripherals are used by the bootloader:

* FM UART:  [Compile option: ``ENABLE_FIRMWARE_MANAGER=uart``]
    - Quark SE C1000:       ``UART1``
    - Quark D2000:          ``UART0``

* FM USB:  [Compile option: ``ENABLE_FIRMWARE_MANAGER=2nd-stage`` (and 2nd-stage bootloader programmed)]
    - Quark SE C1000:       ``USB0``
    - Quark D2000:          n/a

Both the FM UART and USB can be freely used by the application.

Time constraints
****************

The bootloader guarantees a boot-time smaller than 10ms during normal boot.
The boot time could increase during the first boot or after a Firmware
Management session.

Flash resources
***************

The bootloader is located in the OTP flash, but it also uses other portions of
the flash (e.g., to store metadata).

BL-Data flash section
=====================

A part of the flash of the device is reserved for the bootloader data (BL-data).

+------------------+--------------------------+-------+----------------+
| SoC              | BL-Data address range    | Size  | Flash region   |
+==================+==========================+=======+================+
| Quark D2000      | 0x00200000 - 0x00201000  | 4kB   | Data           |
+------------------+--------------------------+-------+----------------+
| Quark SE C1000   | 0x4002F000 - 0x40030000  | 4kB   | System flash 0 |
+------------------+--------------------------+-------+----------------+

2nd-stage flash partition
=========================

When the 2nd-stage bootloader is enabled, a portion of flash is reserved for
the 2nd-stage and therefore is not available to the application.

+------------------+--------------------------+-------+----------------+
| SoC              | 2nd-stage address range  | Size  | Flash region   |
+==================+==========================+=======+================+
| Quark D2000      | N/A                      | N/A   | N/A            |
+------------------+--------------------------+-------+----------------+
| Quark SE C1000   | 0x4005b000 - 0x40060000  | 20kB  | System flash 1 |
+------------------+--------------------------+-------+----------------+

Memory protection
*****************

Flash protection
================

Part of the aforementioned BL-Data flash section is available for the firmware
to be read, while the other part is private for the bootloader itself.

To enforce this, before jumping to the application, the bootloader sets up a
Flash Protection Region (FPR) to read-protect the private portion of BL-Data.
This FPR is locked so it cannot be disabled or reused by the firmware. Trying
to read a protected flash address will trigger a warm reset.

+----------------+-------+------------------+-------------------------+------+
| SoC            | FPR   | Flash controller | Protected range         | Size |
+================+=======+==================+=========================+======+
| Quark D2000    | FPR_0 | Controller 0     | 0x00200400 - 0x00201000 | 3kB  |
+----------------+-------+------------------+-------------------------+------+
| Quark SE C1000 | FPR_0 | Controller 0     | 0x4002F400 - 0x40030000 | 3kB  |
+----------------+-------+------------------+-------------------------+------+

SRAM protection
===============

The bootloader also sets up a Memory Protection Region (MPR) to protect
the portion of SRAM containing system data that is critical to the proper
functioning of the SoC. Specifically, the MPR protects the x86 GDT and IDT
against read/write access by agents different from the x86 core. This is a
security hardening feature that limits the damage an attacker can do by
hijacking some other agents (like ARC or DMA). The MPR is locked and cannot be
reused by the application.

.. note:: Since MPR granularity is 1kB and the combined size of GDT and IDT is
          smaller than 1kB, the MPR also protects part of the stack (which in
          the default RAM layout is located before the IDT). The main
          consequence for application developers is that they cannot use DMA
          to access local variables on the stack.

+----------------+-------+-------------------------+------+------------------+
| SoC            | MPR   | Protected range         | Size | Protected data   |
+================+=======+=========================+======+==================+
| Quark D2000    | MPR_0 | 0x00281C00 - 0x00282000 | 1kB  | IDT + GDT + part |
|                |       |                         |      | of stack         |
+----------------+-------+-------------------------+------+------------------+
| Quark SE C1000 | MPR_0 | 0xA8013C00 - 0xA8014000 | 1kB  | IDT + GDT + part |
|                |       |                         |      | of stack + x86   |
|                |       |                         |      | restore info     |
+----------------+-------+-------------------------+------+------------------+
