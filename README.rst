Intel® Quark™ Bootloader
########################

Overview
********

The Intel® Quark™ bootloader supports the following SoCs:

* Intel® Quark™ D2000 Microcontroller (D2000)
* Intel® Quark™ SE Microcontroller C1000 (SE C1000)

.. contents::

Support
*******

Information and support regarding Intel® Quark™ MCUs can be found in the
following website:

http://www.intel.com/quark/mcu

Hardware Compatibility
**********************

This release has been validated with the following hardware:

* Intel® Quark™ SE Microcontroller C1000.
* Intel® Quark™ SE Microcontroller C1000 Development Platform.
* Intel® Quark™ Microcontroller D2000.
* Intel® Quark™ Microcontroller D2000 Development Platform.

External Dependencies
*********************

* The ISSM toolchain is required to build the source code. It provides the
  IAMCU toolchain (i586-intel-elfiamcu). The currently supported version is
  "2017-02-07" for Linux and "2017-01-25" for Windows.
* OpenOCD is required to flash the bootloader onto the SoC.

* The toolchain is provided within the ISSM package or
  `standalone tarballs <https://software.intel.com/en-us/articles/issm-toolchain-only-download>`_.


More info about dependencies can be found in the
`bootloader resources <doc/boot_resources.rst>`__ file.

License
*******

Please refer to `COPYING <COPYING>`_.

Organization
************
::

	.
	├── bootstrap       : Bootstrap code
	│   └── soc         : SoC-specific bootstrap code and headers
	├── doc             : Bootloader documentation
	├── fw-manager      : Firmware managment code
	│   └── dfu         : DFU over UART code
	│   └── qfm         : Quark Firmware Management (QFM) protocol code
	│   └── qfu         : Quark Firmware Upgrade (QFU) image format code
	└─  tools           : Firmware Management host-side tools

Documentation
*************

All Documentation about the bootloader can be found in the doc folder:

- `Bootloader resources <doc/boot_resources.rst>`__
- `Bootloader flow      <doc/boot_flow.rst>`__
- `Firmware Manager User Guide`_
- `Firmware Manager Overview`_
- `Authenticated Firmware Upgrade`_

Building
********

The build system is based on the make tool.
Make sure you have downloaded the toolchain as described in
`External Dependencies`_.

Environment
===========

You must first set the IAMCU_TOOLCHAIN_DIR environment variable.
Assuming the toolchain was unpacked into *$HOME/issm_2016/* and
that you would find *i586-intel-elfiamcu-gcc* at
*$HOME/issm_2016/tools/compiler/gcc-ia/5.2.1/bin*, the variable can be set with:

``export IAMCU_TOOLCHAIN_DIR=$HOME/issm_2016/tools/compiler/gcc-ia/5.2.1/bin``

QMSI sources are needed to build bootloader code. The QMSI_SRC_DIR environment
variable must point to the QMSI folder, for instance:

``export QMSI_SRC_DIR=$HOME/qmsi``

If such a variable is not defined, the bootloader expects QMSI folder to be
named ``qmsi`` and be a sibling folder of the bootloader one.

To use the "make flash" command, the $QM_BOOTLOADER_DIR environment variable
should be set.

``export QM_BOOTLOADER_DIR=$HOME/bootloader``

You must also get the latest TinyCrypt code from
https://github.com/01org/tinycrypt. Checkout tag v0.2.6 and export the
following environment variable:

``export TINYCRYPT_SRC_DIR=/PATH/TO/SOURCE/tinycrypt``


Building options
================

The bootloader makefile supports the following build parameters:
        - SOC
        - ENABLE_FIRMWARE_MANAGER
        - ENABLE_FIRMWARE_MANAGER_AUTH
        - ENABLE_RESTORE_CONTEXT
        - ENABLE_FLASH_WRITE_PROTECTION

Before changing any build parameters you must first do a clean:

``make clean``

Target SoC selection
--------------------

Both D2000 and SE C1000 are supported. You can select them by setting the
``SOC`` variable.

To build for D2000:

``make SOC=quark_d2000``

To build for SE C1000:

``make SOC=quark_se``

On SE C1000, the ARC is not started as part of the ROM flow. It is up to user
application / RTOS to do such. In a bare metal environment, to start the ARC
the function ``sensor_activation()`` must be called. The function is provided
by QMSI in the ss_init.h header file.

Firmware Management
-------------------

ENABLE_FIRMWARE_MANAGER is used to enable firmware management inside of the
bootloader.

To disable firmware manager:

``make ENABLE_FIRMWARE_MANAGER=none``

To enable firmware manager over uart:

``make ENABLE_FIRMWARE_MANAGER=uart``

To enable firmware manager over usb:

``make ENABLE_FIRMWARE_MANAGER=2nd-stage``

In order to use the firmware manager over usb a 2nd-stage bootloader must be
flashed, more information about this can be found in `Authenticated Firmware
Upgrade`_.

By default, firmware management mode is enabled over uart.

More info on building and flashing an application using the firmware management
mode can be found in the `Firmware Manager User Guide`_.

Authenticated Firmware Management
---------------------------------

When firmware management is enabled ENABLE_FIRMWARE_MANAGEMENT_AUTH can be
used to enable authentication in the firmware manager.

To enable authentication:

``ENABLE_FIRMWARE_MANAGER_AUTH=1``

To disable authentication:

``ENABLE_FIRMWARE_MANAGER_AUTH=0``

Firmware manager authentication is enabled by default.

More information on authenticated firmware management can be found in
'Authenticated Firmware Upgrade'_.

Return from sleep
-----------------

The Quark SE has support for sleep states that power off the CPU. When a
wake event happens, the CPU starts over from the reset vector as in a normal
power on. To do so, build both the bootloader and libqmsi with
'ENABLE_RESTORE_CONTEXT=1'.

``make SOC=quark_se ENABLE_RESTORE_CONTEXT=1``

The context of the Quark D2000 is restored by the hw. For that reason,
the ENABLE_RESTORE_CONTEXT option has no effect on Quark D2000 SoC.

By default, context save and restore management is enabled on Quark SE.

Flash write protection
----------------------

By default the bootloader write-protects all the SoC flash memory to avoid any
possible modification of the firmware.

It's possible to deactivate this feature by compiling the bootloader with
'ENABLE_FLASH_WRITE_PROTECTION=0'. This, however, will render the SoC vulnerable
to malware gaining access to the firmware and overwriting it.

Flashing
========

The bootloader must be flashed on the OTP ROM flash region. Before flashing a
new bootloader, a mass erase should be performed.

For flashing the board OpenOCD must be used. You can optionally use gdb
as a front-end for OpenOCD as described below.

Assuming the toolchain was unpacked into *$HOME/issm_2016/*, this can be
done with:

``$ cd $HOME/issm_2016/tools/debugger/openocd``

For D2000, start OpenOCD with the following command:

``$ ./bin/openocd -f scripts/board/quark_d2000_onboard.cfg``

For SE C1000, start OpenOCD with the following command:

``$ ./bin/openocd -f scripts/board/quark_se_onboard.cfg``

Create a new terminal session at this point and set environment variables
accordingly. Then launch a GDB session using:

``$ gdb``

To connect to the remote port, enter the following GDB commands:

``(gdb) target remote :3333``

``(gdb) monitor clk32M 5000``

To perform a mass erase, enter the following GDB command:

``(gdb) monitor mass_erase``

For D2000, the following command is used to flash the bootloader to the device:

``(gdb) monitor load_image $PATH_TO_QM_BOOTLOADER/build/release/quark_d2000/rom/quark_d2000_rom_fm_hmac.bin 0x0``


For SE C1000, the following command is used to flash the bootloader to the
device:

``(gdb) monitor load_image $PATH_TO_QM_BOOTLOADER/build/release/quark_se/rom/quark_se_rom_fm_hmac.bin 0xFFFFE000``


.. Links
.. _`Firmware Manager User Guide`: doc/fw-manager-user-guide.rst
.. _`Firmware Manager Overview`: doc/fw-manager-overview.rst
.. _`Authenticated Firmware Upgrade`: doc/authenticated_firmware_upgrade
