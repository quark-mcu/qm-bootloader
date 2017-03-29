Bootloader: Firmware Managment User Guide
#########################################

.. contents::

Overview
********

The Intel® Quark™ supports Firmware Management (FM) over the following
connections:

- SE C1000  USB         Linux/Windows
- SE C1000  UART        Linux/Windows
- D2000     UART        Linux/Windows

Hardware Setup
**************

The hardware setup consists of three main steps:

* Connecting the board to the computer using one of the supported connections
  (UART, USB).

* Connecting the FM GPIO pin to ground.

* Connecting power supply to the board.

The FM GPIO pin is the pin used to put the device into FM mode. The device has
to be reset while the GPIO is connected to ground.

Read the `resources documentation <boot_resources.rst>`__ for more
information on the hardware set-up.


FM GPIO pins
============

+----------------------------------+---------------------+----------+--------+
| BOARD                            | FM-PIN              |  UART    | BUTTON |
+==================================+=====================+==========+========+
| Intel® Quark™ SE Microcontroller |                     |          |        |
| C1000 Development Kit            | J14.43 (AON_GPIO_4) | UART 1   | SW1    |
+----------------------------------+---------------------+----------+--------+
| Intel® Quark™ Microcontroller    |                     |          |        |
| D2000 Development Kit            | J4.6 (GPIO_2)       | UART 0   | SW2    |
+----------------------------------+---------------------+----------+--------+


Software Setup
**************

Installing dfu-util
===================

A different variant of dfu-util must be used depending on the used connection.

Installing UART version
-----------------------

*dfu-util-qda* is the tool needed to flash QFU images over UART to the target
or manage the device. You can get the software from
https://github.com/quark-mcu/qm-dfu-util, follow the build instructions in
the `readme file
<https://github.com/quark-mcu/qm-dfu-util/blob/master/README.rst>`__.

Installing USB version
----------------------

*dfu-util* is the tool needed to flash QFU images over USB to the target or
manage the device.

* On Debian and Ubuntu systems you can install dfu-util by typing::

    sudo apt-get install dfu-util

* On Fedora the command line command to install dfu-util is::

    sudo dnf install dfu-util

dfu-util can also be installed manually. Please follow the instructions on
http://dfu-util.sourceforge.net/.

Enabling FM with authentication functionality in the bootloader
===============================================================

In order to use the Firmware Management (FM) functionality on the target
platform, you must install a ROM with such functionality enabled. To do so,
perform the following steps:

* Get the latest TinyCrypt code from https://github.com/01org/tinycrypt.
* Checkout tag v0.2.6 and export the following environment variable::

    export TINYCRYPT_SRC_DIR=/PATH/TO/SOURCE/tinycrypt

.. warning:: It is highly recommended to perform a flash mass erase before
             flashing the rom.

Enabling UART functionality
---------------------------

* Compile the ROM code:

  Enable the Firmware Manager for UART::

    make rom SOC=quark_se ENABLE_FIRMWARE_MANAGER=uart

* Flash the new rom image to the target::

    python $(QMSI_SRC_DIR)/tools/jflash/jflash.py -r quarkse_dev ./build/release/quark_se/rom/quark_se_rom_fm_hmac.bin

Enabling USB functionality
--------------------------

A second stage bootloader is required, in order to use the USB dfu-util.

* Compile the ROM code:

  Enable the second stage bootloader::

    make rom SOC=quark_se ENABLE_FIRMWARE_MANAGER=2nd-stage

* Flash the new rom image to the target::

    python $(QMSI_SRC_DIR)/tools/jflash/jflash.py -r quarkse_dev ./build/release/quark_se/rom/quark_se_rom_fm_2nd_stage.bin

* Compile the 2nd stage bootloader::

    make -C 2nd-stage

* Flash the 2nd stage bootloder to address `0x4005b000`::

    python $(QMSI_SRC_DIR)/tools/jflash/jflash.py -u quarkse_dev ./2nd-stage/release/quark_se/x86/bin/2nd_stage_usb_hmac.bin

Writing a new key to the device
===============================

The firmware manager uses two keys, a firmware key and a revocation key. The
firmware key is needed to update the firmware and the revocation key is needed
to update the firmware key. The revocation key must have been written before
the firmware key can be written. It is good practice never to store these keys
in the same place.

Both keys are 32 bytes long. They can be generated on Linux using OpenSSL. ::

        openssl rand 32 > <KEY_FILE>

A new revocation key can be written to the device by using the qm_manage.py
tool. A new firmware key can then be written to the device also by using
qm_manage.py. As a security precaution firmware upgrades can't be performed
until both keys have been correctly provided to the device. Once a key is set
it can be updated only if the old keys are known.

The qm_manage.py tool can be used over usb or over uart. The examples below
are specific to usb but they can be adapted to work over uart simply by
replacing the '-d' option with the '-p' option and replacing the device id
with the serial port.

The format of the device id is 'vendor:product', for example '8086:c100'.

A new revocation key can be set using qm_manage set-rv-key. For first-time
provisions the new key and the device id must be specified. ::

        python ./tools/sysupdate/qm_manage.py set-rv-key  <RV_KEY_FILE>  -d <DEVICE_ID>

For subsequent revocation key updates the current revocation must be specified.
The current firmware key must also be specified after the firmware key has been
changed. ::

        python ./tools/sysupdate/qm_manage.py set-rv-key  <RV_KEY_FILE>  --curr-rv-key <CURRENT_RV_KEY_FILE>  --curr-fw-key <CURRENT_FW_KEY_FILE>  -d <DEVICE_ID>

A new firmware key can then be set using qm_manage set-fw-key. For first-time
provisions the new firmware key, the device id must be specified, and the
current revocation key must be specified. ::

        python ./tools/sysupdate/qm_manage.py set-fw-key  <FW_KEY_FILE>  --curr-rv-key <CURRENT_RV_KEY_FILE>  -d <DEVICE_ID>

For subsequent firmware key updates the current firmware key and the current
revocation key must also be specified. ::

        python ./tools/sysupdate/qm_manage.py set-fw-key  <FW_KEY_FILE>  --curr-fw-key <CURRENT_FW_KEY_FILE>  --curr-rv-key <CURRENT_RV_KEY_FILE>  -d <DEVICE_ID>

Both keys must be 32 bytes long and passed to qm-manage as a binary file.


FM functionality usage
**********************

Creating and flashing a QFU image
=================================

This example shows how to use the Bootloader Firmware Management functionality
to build and flash the LED blink example application to the Intel® Quark™
Developer Kit D2000 and Intel® QuarkTM SE Microcontroller C1000 Developer
Board. User is recommended to be in a root/admin mode to be able to run
root/admin privilege command.


.. note:: To give permission to access serial port, use the following command:
   ::

        sudo usermod -a -G dialout $USER

The following are the steps to create and flash QFU image:

* While in QMSI directory, setup the software environment as explained in the
  `README <../README.rst>`__.
* Build the project: ::

        make -C examples/blinky SOC=<TARGET_SOC> TARGET=<CORE_TYPE>


.. note:: <TARGET_SOC> values can be quark_se or quark_d2000.
.. note:: <CORE_TYPE> values can be x86 for LMT core, or sensor for ARC core.
   Intel® QuarkTM Microcontroller D2000 only supports LMT core.

* Create a signed DFU image: ::

        python $QM_BOOTLOADER_DIR/tools/sysupdate/qm_make_dfu.py --soc=<TARGET_SOC> -v examples/blinky/release/quark_se/x86/bin/blinky.bin --key <FW_KEY_FILE> -p 1


.. note:: <TARGET_SOC> values can be quark_se or quark_d2000. quark_se is the
   default value if not declared.
.. note:: The -p option is used to choose the flash partition. Partition 1 is
   used by the x86 core and partition 2 is used by the Sensor Subsystem. Intel®
   QuarkTM Microcontroller D2000 supports partition 1 only.
.. note:: The -v option makes the tool output some information about the
   generated image.
.. note:: Make sure qmfmlib library is installed.
.. note:: For Windows*, replace $QM_BOOTLOADER_DIR with %QM_BOOTLOADER_DIR% .

* If -v was added as a parameter, you get the following output: ::

        qm_make_dfu.py: QFU-Header and DFU-Suffix content:
                Partition: 1
                Vendor ID: 0
                Product ID: 0
                Version: 0
                Block Size: 2048
                Blocks: 2
                DFU CRC: 0x8741e6e7
        qm_make_dfu.py: blinky.dfu written


.. note:: To get a description of the QFU Image Creator parameters, run the
   following command: ::

        python qm_make_dfu.py --help

* Reset the board while grounding the FM GPIO pin.

* Download the DFU-image.

  - Run the following command if using UART: ::

        dfu-util-qda -D examples/blinky/release/quark_se/x86/bin/blinky.bin.dfu -p <PORT> -R -a 1

  - Run the following command if using USB: ::

        dfu-util -D examples/blinky/release/quark_se/x86/bin/blinky.bin.dfu -d <VID:PID> -R -a 1


.. note:: Run dfu-util --help for more information on the command usage.
.. note:: The -a option is used to choose flash partition. Partition 1 is used
   by the x86 core and partition 2 is used by the Sensor Subsystem. Intel®
   QuarkTM Microcontroller D2000 supports partition 1 only.
.. note:: Make sure no serial terminal is using the port while flashing the
   device. To check for connected Serial Port for Windows system, open Device
   Manager while for Linux system, run dmesg in terminal to see the connected
   Serial Port. Once Serial Port is identified, replace <PORT> with
   /dev/ttyUSBx or COMXX.
.. note:: To get a description of DFU-UTIL-QDA parameters, run
   dfu-util-qda --help, and for more information on qm_make_dfu, visit this
   GitHub* page.
.. note:: If DFU-download returns an error, redo the flashing step.

* Unground the FM GPIO pin and press the reset button to run the application.

Application Erase / System Information Retrieval
================================================

System information can be retrieved by a Python script located in the
repository's tools/sysupdate directory. This script uses the dfu-util(-qda)
binary to communicate with the device.

* Make sure qfu-util(-qda) is installed.
* Go to the tools/sysupdate directory.
* Run the python script `qm_manage.py --help` to display possible commands.

This script can also be used for application erase but only if the device has
a rom with the authentication feature disabled.

System Information
------------------

* Enter device DFU mode by resetting the device while the FM GPIO is connected
  to ground:

  * Run the following command for the UART connection::

     qm_manage.py info -p <SERIAL_INTERFACE>

  * Run the following command for the USB connection::

     qm_manage.py info -d <VENDOR_ID:PRODUCT_ID>


.. note:: By specifying the ``--format`` option, the output format can be set
          to either text (default) or json.

Erase Applications
------------------

* Enable application erase by disabling authentication in the bootloader. To do
  this, see `Disabling authentication feature in bootloader`_.

* Enter device DFU mode by resetting the device while the FM GPIO is connected
  to ground:

  - Run the following command for the UART connection::

      qm_manage.py erase -p <SERIAL_INTERFACE>

  - Run the following command for the USB connection::

      qm_manage.py erase -d <VENDOR_ID:PRODUCT_ID>


.. note:: All applications except the bootloader will be erased.

Firmware update with no authentication
**************************************

Disabling authentication feature in bootloader
==============================================

To disable the secure boot option, the bootloader has to be built with the
ENABLE_FIRMWARE_MANAGER_AUTH=0 option. Export or set the environment as
explained in Setting up Software Environment, then enter the following commands.
In the case of firmware update over UART: ::

        make SOC=<TARGET_SOC> ENABLE_FIRMWARE_MANAGER=uart ENABLE_FIRMWARE_MANAGER_AUTH=0


.. note:: For firmware update over UART, <TARGET_SOC> value can be quark_se or
   quark_d2000

In the case of firmware update over USB: ::

        make SOC=quark_se ENABLE_FIRMWARE_MANAGER=2nd-stage
        make -C 2nd-stage ENABLE_FIRMWARE_MANAGER_AUTH=0

To flash the created ROM, see `Enabling UART functionality`_ for UART or
`Enabling USB functionality`_ for USB.

Simple way (using 'make flash')
===============================

* Reset the device while connecting the FM GPIO to ground.
* Compile, upload and run the example app.
* Change to the QMSI directory::

    cd <PATH_TO_QMSI>

  - For the UART connection::

      make -C <APP_DIR> flash SOC=<SOC> TARGET=<TARGET> SERIAL_PORT=<SERIAL_INTERFACE>

  - For the USB connection::

      make -C <APP_DIR> flash SOC=<SOC> TARGET=<TARGET> USB_DEVICE=<VENDOR_ID:PRODUCT_ID>

The SoC can be ``quark_se`` or ``quark_d2000`` depending on the used soc. The
target can be ``x86`` or ``sensor`` depending on the used core.


.. note:: 'make flash' only supports unauthenticated flashing

Make flash example
------------------

This example will show how to build and flash the blinky example for the
Quark SE C1000 x86 core. For UART, the used serial device is assumed to be
``/dev/tty0``; while for USB, the device Vendor ID and Product ID are assumed
to be ``8086`` and ``C100`` respectively.

* Change the directory to the QMSI base directory::

    cd $QMSI_SRC_DIR

- For the UART connection::

    make -C examples/blinky flash SOC=quark_se TARGET=x86 SERIAL_PORT=/dev/tty0

- For the USB connection::

    make -C examples/blinky flash SOC=quark_se TARGET=x86 USB_DEVICE=8086:C100

Step by step process (without make flash)
=========================================

* Change to the QMSI directory::

    cd <PATH_TO_QMSI>

* Build the project::

    make -C <APP_DIR> SOC=<SOC> TARGET=<TARGET>

The soc can be quark_se or quark_d2000 depending on the used soc. The target can
be x86 or sensor depending on the used core.

* Create the dfu file::

    python ./tools/sysupdate/qm_make_dfu.py -v <BINARY_FILE> -p <PARTITION>

The ``-p`` option is used to choose the flash partition. Partition 1 is used
by the x86 core and partition 2 is used by the Sensor Subsystem.

The ``-v`` option makes the tool output some information about the generated
image.

The output DFU image will have the same name of the binary file with the
``.dfu`` extension appended.

* Reset the device while connecting the FM GPIO to ground.
* Download the image.

  - Using a UART connection::

      dfu-util-qda -D <DFU_IMAGE> -p <SERIAL_INTERFACE> -R -a <PARTITION>

  - Using a USB connection::

      dfu-util -D <DFU_IMAGE> -d <VENDOR_ID:PRODUCT_ID> -R -a <PARTITION>

The ``-a`` option is used to choose the flash partition. Partition 1 is used
by the x86 core and partition 2 is used by the Sensor Subsystem.

The ``-R`` option will reset the device after the download is finished.

Step by step example
--------------------

This example will show how to build and flash the blinky example for the Quark
SE C1000 x86 core.  For UART, the used serial device is assumed to be
``/dev/tty0``; while for USB, the device Vendor ID and Product ID are assumed
to be ``8086`` and ``C100`` respectively.

* Change the directory to the QMSI base directory::

    cd $QMSI_SRC_DIR

* Build the project::

    make -C examples/blinky SOC=quark_se TARGET=x86

* Create the dfu file::

    python ./tools/sysupdate/qm_make_dfu.py -v examples/blinky/release/quark_se/x86/bin/blinky.bin -p 1

* You should get the following output if you use the -v option::

    qm_make_dfu.py: QFU-Header and DFU-Suffix content:
          Partition:   1
          Vendor ID:   0
          Product ID:  0
          Version:     0
          Block Size:  2048
          Blocks:      2
          DFU CRC:     0x8741e6e7
    qm_make_dfu.py: blinky.dfu written

``blinky.dfu`` is your generated QFU image.

* Reset the device while connecting the FM GPIO to ground.
* Download the image.

  - Using a  UART connection::

      dfu-util-qda -D examples/blinky/release/quark_se/x86/bin/blinky.bin.dfu -p /dev/tty0 -R -a 1

  - Using a USB connection::

      dfu-util -D examples/blinky/release/quark_se/x86/bin/blinky.bin.dfu -d 8086:C100 -R -a 1


.. note:: The path of the binary may differ when building a D2000 or a
          Sensor Subsystem image.
