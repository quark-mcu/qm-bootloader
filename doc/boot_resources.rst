BOOTLOADER RESOURCES
####################

.. contents::

The bootloader uses dedicated resources to guarantee its flow. Hardware
designers and application developers should pay attention how they use the
resources to avoid unexpected behavior. The bootloader will not use the
resources with a compile option if they are not set.

Overview
--------

+------------------+---------------+----------------+------------------+
| Resource         | Quark D2000   | Quark SE C1000 | Notes            |
+==================+===============+================+==================+
|                  | | 0x00200000  | | 0x4002F000   |                  |
| Flash            | | 0x00201000  | | 0x40030000   | Reserved         |
+------------------+---------------+----------------+------------------+
| JTAG probe       | GPIO_13       | GPI0_15        | Do not ground    |
+------------------+---------------+----------------+------------------+
| Sleep register   | N/A           | GPS1           | Reserved         |
+------------------+---------------+----------------+------------------+
| UART PORT        | Uart 0        | Uart 1         | Available to app |
+------------------+---------------+----------------+------------------+
| FM register      | GPS0 bit 0    | GPS0 bit 0     | Reserved         |
+------------------+---------------+----------------+------------------+
| FM gpio pin      | GPIO_2        | AON 1          | Do not ground    |
+------------------+---------------+----------------+------------------+


Gpio pins
*********

The bootloader uses the following pins:

* JTAG probe pin:
        - Quark SE C1000:       ``GPIO_15``
        - Quark D2000:          ``GPIO_13``

        This pin is used to put the bootloader into unbrick mode. Unbrick mode
        can be used when the JTAG is not able to connect to the device during
        runtime.

* Firmware management pin:  [Compile option: ``ENABLE_FIRMWARE_MANAGER=1``]
        - Quark SE C1000:       ``AON 0``
        - Quark D2000:    ``GPIO_2``

        This pin is used to start the bootloader in the firmware management mode
        (FM).

**Gpio pins used by the bootloader should not be grounded during the boot process!**

Sticky registers
****************

The bootloader uses the following sticky registers:

* Firmware manager:  [Compile option: ``ENABLE_FIRMWARE_MANAGER=1``]
        - All SoC's:    ``GPS0 bit 0``

        This register is used to start the bootloader in the firmware management
        mode (FM). **This bit is reserved for the bootloader, the application
        should not use this bit!**

Peripherals
***********

The following peripherals are used by the bootloader:

* Firmware manager:  [Compile option: ``ENABLE_FIRMWARE_MANAGER=1``]
        - Quark Se C1000:       ``Uart1``
        - Quark D2000:          ``Uart0``

The uart will be used to program the device by the Firmware manager. The
uart can be freely used by the application.

Time constraints
****************

The bootloader guarantees a boot-time smaller than 10ms during normal boot.
The boot time could increase during the first boot or when the Firmware
management session is active.

Flash
*****

A part of the flash of the device is reserved for the bootloader data (BL-data).

+------------------+--------------------------+-------+----------------+
| SoC              | BL-data                  | Size  | Flash region   |
+==================+==========================+=======+================+
| Quark D2000      | 0x00200000 - 0x00201000  | 4096  | Data           |
+------------------+--------------------------+-------+----------------+
| Quark SE C1000   | 0x4002F000 - 0x40030000  | 4096  | System flash 0 |
+------------------+--------------------------+-------+----------------+
