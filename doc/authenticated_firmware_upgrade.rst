Authenticated firmware upgrade overview:
########################################

.. contents::

Overview
********

Using the bootloader authenticated firmware upgrade functionality the user can
control what application firmware can be installed on the device. Only signed
images can be programmed when the secure boot functionality is active. The
secure bootloader uses HMAC256 to sign the users images. The Intel® Quark™
supports secure boot over the following connections:

- SE C1000  USB         Linux/Windows
- SE C1000  UART        Linux/Windows
- D2000     UART        Linux/Windows

In order to use the secure firmware upgrade mechanism one must:

#. Build and flash the ROM and 2nd-stage bootloader with firmware manager
   authentication enabled
#. Set a new authentication key on the device
#. Create a signed image

Enabling the secure option in the bootloader
********************************************

Image authentication during firmware upgrades is enabled by default over uart.
To enable image authentication over USB, the 2nd-stage must be built with the
ENABLE_FIRMWARE_MANAGER_AUTH=1 option.

More information about the build option of the bootloader can be found in the
`Firmware manager user guide <fw-manager-user-guide.rst>`__.

Enabling UART functionality
===========================

* Compile the ROM code:

  Enable the Firmware Manager for UART, authentication will be enabled by
  default::

    make rom SOC=quark_se ENABLE_FIRMWARE_MANAGER=uart

.. note:: The SoC can be 'quark_se' or 'quark_d2000' depending on the SoC used.

* Flash the new rom image to the target::

    python $(QMSI_SRC_DIR)/tools/jflash/jflash.py -r quarkse_dev ./build/release/quark_se/rom/quark_se_rom_fm_hmac.bin

.. note:: The target board can be 'quarkse_dev' or 'd2000_dev' depending on the target board used.

Enabling USB functionality
==========================

A second stage bootloader is required, in order to use the USB dfu-util.

* Compile the ROM code:

  Enable the second stage bootloader::

    make rom SOC=quark_se ENABLE_FIRMWARE_MANAGER=2nd-stage

* Flash the new rom image to the target::

    python $(QMSI_SRC_DIR)/tools/jflash/jflash.py -r quarkse_dev ./build/release/quark_se/rom/quark_se_rom_fm_2nd_stage_hmac.bin

* Compile the 2nd stage bootloader with the ENABLE_FIRMWARE_MANAGER_AUTH option
  set to 1::

    make -C 2nd-stage ENABLE_FIRMWARE_MANAGER_AUTH=1

* Flash the 2nd stage bootloder to address `0x4005b000`::

    python $(QMSI_SRC_DIR)/tools/jflash/jflash.py -u quarkse_dev ./2nd-stage/release/quark_se/x86/bin/2nd_stage_usb_hmac.bin

Writing a new key to the device
*******************************

The firmware manager uses two keys, a firmware key and a revocation key. The
firmware key is needed to update the firmware and the revocation key is needed
to update the firmware key. The revocation key must have been written before
the firmware key can be written. It is good practice never to store these keys
in the same place.

A new revocation key can be written to the device by using the qm_manage.py
tool. A new firmware key can then be written to the device also by using
qm_manage.py. As a security precaution firmware upgrades can't be performed
until both keys have been correctly provided to the device. Once a key is set
it can be updated only if the old key is known.

The qm_manage.py tool can be used over usb or over uart. The examples below
are specific to usb but they can be adapted to work over uart simply by
replacing the '-d' option with the '-p' option and replacing the device id
with the serial port.

A new revocation key can be set using qm_manage set-rv-key. For first-time
provisions the new key and the device id must be specified. ::

        python ./tools/sysupdate/qm_manage.py set-rv-key  <RV_KEY_FILE>  -d <DEVICE_ID>

For subsequent revocation key updates the current revocation key and the
current firmware key must also be specified. ::

        python ./tools/sysupdate/qm_manage.py set-rv-key  <RV_KEY_FILE>  --curr-rv-key <CURRENT_RV_KEY_FILE>  --curr-fw-key <CURRENT_FW_KEY_FILE>  -d <DEVICE_ID>

A new firmware key can then be set using qm_manage set-fw-key. For first-time
provisions the new firmware key, the device id must be specified, and the
current revocation key must be specified. ::

        python ./tools/sysupdate/qm_manage.py set-fw-key  <FW_KEY_FILE>  --curr-rv-key <CURRENT_RV_KEY_FILE>  -d <DEVICE_ID>

For subsequent firmware key updates the current firmware key and the current
revocation key must also be specified. ::

        python ./tools/sysupdate/qm_manage.py set-fw-key  <FW_KEY_FILE>  --curr-fw-key <CURRENT_FW_KEY_FILE>  --curr-rv-key <CURRENT_RV_KEY_FILE>  -d <DEVICE_ID>

Both keys must be 32 bytes long and passed to qm-manage as a binary file.

Creating a secure image
***********************

When authenticated firmware upgrade is enabled in the bootloader, a special
signed dfu package has to be created. This can be performed by using the
qm_make_dfu.py tool.

A signed hmac256 image can be created with the following command: ::

        python ./tools/sysupdate/qm_make_dfu.py <IMAGE> --key <FW_KEY_FILE> -p <PARTITION>

The <IMGAGE> should be a valid quark image. A 32 byte binary file is expected
for the <FW_KEY_FILE>. More information about qm_make_dfu.py and its different
parameters can be found in the `Firmware manager user guide
<fw-manager-user-guide.rst>`__.

Afterwards the image can be flashed using dfu-util as normal. Information about
flashing the device can be found in the `Firmware manager user guide
<fw-manager-user-guide.rst>`__.

