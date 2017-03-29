Secure Programmer's Guide
#########################

.. contents::

.. sectnum::

Introduction
************

This document describes the security features provided by the bootloader of the
Intel® Quark™ Microcontroller Software Interface (QMSI). Those features can be
grouped into two main functionalities: authenticated firmware upgrades and SoC
security hardening.

Document overview
=================

Section `Security overview`_ of this document discusses the security assets and
trusted computing base of a Intel® Quark™ microcontroller. Section `Trusted
boot`_  deals with the trusted boot process.  Section `Secure firmware update`_
then deals with the secure firmware update, including an explanation of the
authentication mechanism, key management, and the format of upgrade images.
Finally, Section `Security hardening`_ deals with security hardening.

Product overview
================

Intel® Quark™ MCUs are secure, low power, x86-based MCUs designed for deeply
embedded applications. They differentiate via a set of security capabilities
enabling Intel’s customers to build solutions with a security solution best in
class for the targeted market.

.. SECTION 2

Security overview
*****************

This section provides an outline of the various assets that the platform must
protect.  Reviewing this section should help architects and designers to
identify other security requirements that should be added, or requirements that
emerge due to future design or use case changes.

All the security features provided by QMSI and its bootloader are just a
reference, the ultimate security responsibility is of ISV/OEM.

Assets
======

Assets on the platform are typically considered in one of three high level
categories:

- *Secrets*: Platform-specific or global secret values (e.g. keys) used for
  encryption and integrity protection of sensitive user data, content
  protection, authentication or other security objectives.

- *Execution Integrity*:  For various subsystems (host Root of Trust &
  remaining firmware stack, Sensor Subsystem firmware), assets in this category
  refer to avoiding introduction of malware into the platform or other
  unexpected external manipulation of the execution environment.

- *Intermediate assets*:  Platform resources that have access restrictions in
  order to protect the true assets on the platform, the secrets and the
  execution integrity.  The intermediate assets are considered in order to
  define rules for their access, to build the framework for protection of the
  true assets.

OTP memory
----------

Intel® Quark™ MCUs feature an on-die OTP-capable flash where the x86 processor
core reset vector is located. This the first code execution stage of the system
boot flow and effectively represents an hardware root of trust. The bootstrap
code is therefore meant to be located in the OTP flash.

The OTP-capable flash can be hardware locked in order to become an OTP.  **The
OTP lock must be enable before the device is deployed**, otherwise all the
security mechanism described in this document becomes void.


JTAG interface
--------------

The JTAG interface is an entry point to the trusted computing base discussed in
Section `Trusted computing base`_. When the OTP lock is enabled JTAG is
disabled meaning the ROM code cannot be modified.

Flash memory
------------

Flash memory contains the following information that must be protected:

- BL-Data (which is where authentication keys are stored)

- Application firmware

BL-Data must be protected against both reading and writing, in order to avoid
keys to be replaced or leaked by malicious code. As discussed in Section
`Security hardening`_, the bootloader sets up an FPR to protect BL-Data from
reading and disables flash writing to protect it from writing.

In general, application firmware must be protected against writing in order to
prevent attackers from changing it (for instance by exploiting some application
bug that allows for arbitrary code execution). However, a protection against
reading may be required as well, since application firmware may contain private
information (e.g., proprietary IP).

As discussed later, the bootloader disables flash writing on the entire flash
by default, thus protecting application firmware from modification. However,
the precise protection against read access is left to the user.

SRAM
----

There are also assets stored in SRAM. Specifically, the x86 portion of RAM
contains the x86 stack, the x86 global descriptor table (GDT), and the x86
interrupt descriptor table (IDT); whereas in the sensor subsystem RAM there are
the ARC stack and the ARC interrupt vector table (IVT).

The IVT on an ARC architecture includes the main reset vector location. The
Sensor Subsystem has a default IVT location which it will fetch from reset;
however, the IVT base address can be relocated by the ARC processor in kernel
mode.

Any change to the reset vector or the location of that IVT would enable an
attacker to effectively control the Sensor Subsystem boot sequence, leading to
the opportunity to bypass security measures put in place during the later boot
stages.

In the reference boot flow, the Sensor Subsystem is meant to be started by the
x86 application, which sets the ARC reset vector in the IVT, protects the IVT
with an MPR, and then activates the ARC core. However, as discussed in Section
`Enforce core segregation`_, a more secure (but less flexible) behavior is to
have the ARC started by OTP code (i.e., the bootloader).

Some application scenarios may also allow for a complete segregation between
the ARC and the x86 core (each core should not be able to access RAM and flash
memory from the other code). This is discussed in Section `Enforce core
segregation`_.

VR and oscillator registers
---------------------------

The SoC provides a set of memory-mapped registers for configuring the
integrated Voltage Regulators (VR) and Oscillators. These determine voltage
supply to the rest of the SoC as well as the speed at which the SoC operates.
The configuration can be locked via a specific register.

Tampering with the configuration of either the VRs or the oscillators may lead
to permanent denial of service.  However, the reference bootloader does not
lock the configuration since different application scenarios require different
configurations. Application developers are recommended to change the bootloader
to make it set up and lock the configuration they need. Specifically, this
change should be added to the routine setting up the application security
context (see the boot flow in Section `Boot flow`_ for more details).

Trusted computing base
======================

The Trusted Computing Base (TCB) denotes the set of components that must be
trusted in order for the overall platform and application to be secure.

For both Intel® Quark™ microcontroller D2000 and Intel® Quark™ SE
microcontroller C1000 the TCB includes the Lakemont (LMT) core, the on-die SRAM
and the on-die flash. For Intel® Quark™ SE microcontroller C1000 the TCB also
includes the Sensor Subsystem (including the pattern matching engine).

.. SECTION 3

Trusted boot
************

This section describes the ROM/OTP boot stage, which is part of the TCB and
would be immutable in production (once the OTP/JTAG is locked).

The current boot flow supports the secure firmware upgrade feature described in
Section `Secure firmware update`_ and sets up the security hardening described
in Section `Security hardening`_.

ISV/OEM can extend the boot flow to improve/customize the security hardening or
to build other security schemes, like secure boot, on top of it.

Boot flow
=========

The bootloader flow begins with the initialization of the x86 core, which
includes loading the Global Descriptor Table (GDT) and entering 32bit protected
mode.

Then, in case of Intel® Quark™ SE microcontroller C1000, the bootstrap code
checks if the SoC is returning from sleep. If so, the application security
context is set-up and the x86 application execution is resumed. The
*application security* context consists in:

- enabling flash write protection,

- setting up an FPR to read-protect BL-Data (so that no agent can modify it),
  and

- setting up an MPR to read/write-protect the GDT and IDT of the x86 core
  (which becomes the only agent allowed to modify it)

If, instead, the SoC is not resuming from sleep, the bootloader continues the
normal boot process by initializing the RAM (clearing `.bss` and loading
`.data`) and setting up primary peripherals (power and clock configuration).

Next, the bootloader checks the status of the `JTAG_PROBE_PIN`: if it is
grounded the bootloader simply waits until it is ungrounded. This is used to
un-brick a device with firmware that is preventing JTAG from working correctly.
Note that this step becomes useless in production mode when JTAG access is
disabled by locking the OTP.

The next step is to check and optionally sanitize BL-data:

- If this is the first boot (and therefore BL-Data is blank), the bootloader
  initializes its persistent metadata by creating two identical copies of
  BL-Data in flash.

- If this is a subsequent boot, the bootloader verifies the integrity of the
  two copies of BL-Data:

    * If one of the two is corrupted, it is restored using the content of the
      other copy.

    * If both are corrupted, the bootloader enters a faulty state consisting in
      an infinite loop (since this case can happen only in case of a hardware
      fault or a security attack).

Then, the bootloader initializes the Interrupt Description Table (IDT) and
enables interrupts.

Next the bootloader sets up the default memory violation policy, which consists
in triggering a warm reset.

Then, the bootloader checks if Firmware Management (FM) mode is requested,
i.e., the FM pin is grounded or the FM sticky bit of General Purpose Sticky
register 0 (`GPS0`) is set. If so, the bootloader sets up FM security context
and enters FM mode. The FM security context setup consists in setting up an MPR
and an FPR to restrict RAM and Flash access to the x86 core only.

If FM mode is not requested, the bootloader checks if the x86 application is
present (by checking if the first double word is different from 0xFFFFFFFF). If
the application is present, the bootloader sets up the application security
context and jump to the application. If the x86 application returns, the x86
core enters an infinite loop, while the state of the sensor subsystem is not
modified.

If no application is present, the bootloader enters FM mode (after setting up
the FM security context described above).

Secure firmware update
**********************

The bootloader provides a Firmware Management (FM) feature that allows
application firmware to be updated via UART or USB (see the firmware manager
user guide for more information).

The FM feature can be compiled with authentication support (enabled by
default). When authentication is enabled, firmware upgrades can be done only
using signed images: the Firmware Manager rejects any image that is unsigned or
that is signed with the wrong firmware authentication key. Additionally, when
authentication is enabled, the bootloader also provides a mechanism for setting
and updating authentication keys.

This section describes both the authenticated firmware upgrade feature and the
key update functionality, highlighting those aspects that have security
implications.

Overview
========

Image authentication mechanism
------------------------------

Images are signed using a 256 bit hash-based message authentication code
(HMAC-SHA-256). This is a symmetric-key algorithm that generates a SHA-256
keyed hash by combining the image with an authentication key that is shared
between the device and the host.

Since on-DIE flash is part of the SoC TCB and usually write protected, the
image is authenticated during firmware upgrade but not at every boot. It must
also be noted that images are not encrypted.

Authentication keys must be generated by the ISV/OEM and set to the devices
before deployment. The ISV/OEM also have to store keys somewhere, since they
will be needed for signing upgrade images. The security of the key storage
system is responsibility of the ISV/OEM. More information about authentication
keys can be found in Section `Authentication keys`_.

Image metadata
--------------

Each image has an associated security version number (SVN) that the image
creator must specify. A device can be updated only with an image having a SVN
greater than the SVN of the currently installed image, or the previously
installed image in the case that the application firmware has been deleted.

Authentication keys
-------------------

The firmware manager makes use of two kinds of key: the firmware key and the
revocation key. The firmware key is used to authenticate both firmware images
and key updates. The revocation key is used to authenticate key updates, in
addition to a firmware key (in other words, key updates are double signed,
using both the firmware key and the revocation key).

Both keys must be 32 bytes long.

Both keys can be updated. During either a firmware key update or a revocation
key update, the new key is signed using both current keys (i.e., the keys
currently installed in the device). The purpose of the revocation key is to
provide a recovery option using a key that can be stored offline, as it is not
required during usual operation. The reason for authenticating key updates with
both keys is to reduce the risk of an attacker compromising either the
revocation or firmware key (single point of failure).

It is important to note that keys updates are authenticated, but not encrypted.
This means updates must be done by a trusted agent and using a secure channel,
as discussed in Section `Key update security`_.

First-time provisioning is a special case of the key update process and is
discussed in the next paragraph.

First-time provisioning
-----------------------

Authentication keys must be set before a device is deployed. The provisioning
mechanism consists of first setting the revocation key and then setting the
firmware key. The firmware key cannot be set until the revocation key has been
set.

Since the device is un-provisioned, a default key is used to sign the key
updates, in absence of revocation and firmware keys.

The default key is publicly known and used purely for convenience of initial
provisioning and to minimize implementation footprint on the device side.
Devices should never be deployed un-provisioned, since anybody may use the
default key to set their authentication keys thus taking control of them.

To enforce key-provisioning, the firmware upgrade functionality is disabled
until both keys are set.

Key management
==============

Key assignment scheme
---------------------

There are multiple possibilities for key assignment schemes.

One possibility is to assign each device its own firmware key and revocation
key. This is the best solution in terms of security. However, it may be
impractical as an upgrade to a class of devices would require a different
upgrade image for each device, i.e., the same image signed with each device
firmware key. In this scheme the vendor must have a way to match each device to
its firmware key and revocation key.

Another possibility is to assign one firmware key to a class of devices with
different revocation keys. This would allow a class of devices to be upgraded
using a single upgrade image. However, this also means that if the common
firmware key is leaked then firmware on all devices using this key can be
replaced with malicious code. Key update requests would still need to be signed
with the device specific revocation key, thus preventing attackers to take full
control of the devices by changing their keys. In this scheme the vendor must
have a way to match each device to only its revocation key.

Yet another possibility is to assign one firmware key and one revocation key to
a class of devices. This is similar to the above mentioned scheme. It allows a
class of devices to be upgraded using a single upgrade image, and also allows
keys to be updated across a class of devices using a single key update request.
However, this also means that, as in the previous case, if the common firmware
key is leaked then firmware on all devices using this key can be replaced with
malicious code. Additionally, unlike the previous case, if the common
revocation key is leaked together with the common firmware key then the keys on
all devices in the class can be changed and an attacker can take complete
control of all devices in this class.

Other schemes (e.g., the same firmware key across different classes of devices)
are not recommended as they increase security risks without adding any real
value.

In the rest of this document, the second scheme (single firmware key per device
class and different revocation key for each device) is taken as a reference,
since it provides a reasonable compromise between flexibility (i.e., upgrade
images that work on all the devices of a specific class) and security (i.e.,
reducing the risk of having authentication keys of the entire class
compromised).

Anyway, ISV/OEMs are responsible of choosing the scheme that best suits their
application scenarios and requirements.

Key generation mechanism
------------------------

As just discussed, the reference key assignment scheme requires the ISV/OEM to
generate a revocation key for each device and maintain the mapping between each
device and its revocation keys.

A typical approach to achieve this is to use a master-slave keying scheme, in
which each group or class of devices is associated with a master revocation key
(RVm), which is never disclosed, but just used to generate individual slave
keys which are distributed to the devices. Slave revocation keys (RV) can be
generated as follows: :math:`RV=HMAC(RVm, deviceId)`

Key storage
-----------

The revocation key and the firmware key should be chosen independently from a
true random source and should be stored separately. The purpose of this is to
reduce the likelihood of both keys being leaked as that would result in the
system being completely compromised (i.e., an attacker would be able to change
both keys and take full control of the device.)

Note: when only one key is leaked, the leaking of the firmware key is more
critical than the leaking of the revocation key, since the leaked firmware key
can be used by an attacker to update devices with malicious firmware, while the
revocation key alone does not allow the attacker to update the keys.

Key update security
-------------------

Key updates are not encrypted and so keys can be exposed during key updates.

Updates over USB and UART are safe as long as the communication link is secure.
For instance updates over long serial cables can pose a security risk since
eavesdropping may be possible.

Remote updates (e.g. via gateways wired to the devices) also pose a security
risk because of the possibility of eavesdropping. If ISVs/OEMs want to
implement secure remote upgrade functionality, they are responsible for fully
assessing the security implication of their extension to the current solution.

Finally, if ISVs/OEMs do not trust their end users, delegating key updates to
them is generally not secure, because users may try to sniff the unencrypted
keys during the update.

Upgrade images
==============

Upgrade firmware images use a Intel® Quark™ Microcontroller image format. The
image is divided into equal sized blocks. The first block contains the image
header and following blocks contain the raw firmware image.

The header contains common information for processing the image such as the
vendor ID, product ID, and the partition number (identifying where the image is
meant to be flashed). The base header can be followed by an extended header
which contains information for image verification and authentication.

The extended header contains a security version number (SVN), a SHA256 hash of
each image block and a 256 bit hash-based message authentication code
(HMAC-SHA-256). The SVN is discussed in Section `Image metadata`_. The hashes
are used to verify the firmware image. The HMAC is calculated using the
firmware key and the hash of the entire header including the hashes of each
image block.

Upgrade images are unencrypted. This means that if they are made publicly
available intellectual property (IP) may be exposed.

It is important that the SVN is updated when the image fixes a security bug
present in the previous version of the application. Not updating the SVN number
allows attackers to roll-back the previous firmware in order to exploit its
security issues.

Security hardening
******************

As briefly discussed in Section `Boot flow`_, the bootloader sets up some HW
security features in order to harden the security of both the Firmware
Management (FM) feature and the application code.

This section describes in more detail the protection put in place by the
bootloader and it also provides some suggestion on how application developers
can improve it based on their application use case.

Flash write protection
======================

Flash writes are disabled for the entire flash.

This prevents bootloader data, which includes authentication keys, from being
overwritten. It also ensures that only the firmware manager can modify the
flash content, which means that even if there is a bug in an application
allowing an attacker to run arbitrary code, that code cannot be permanently
installed.

However, this protection limits the applications ability to use the flash, for
example, no data logging is possible. Therefore, a compilation flag is provided
to disabled it (specifically, customers will have to recompile the bootloader
with ENABLE_FLASH_WRITE_PROTECTION=0).

However, doing so, poses great security risks as it allows malicious code to
change the keys stored in bootloader data. Additionally, attackers may conceive
advanced attack strategies that exploits the write capability to guess the
keys, thus leading to a key leakage that may compromise an entire class of
devices (depending on the key assignment scheme used).

Read-protected flash regions
============================

Read access to the bootloader data (BL-Data) region of flash is disabled by the
bootloader. This is done by using FPR0 and granting read access to no agents.
Application code should not try to access BL-Data flash region as to do so will
trigger a memory violation event, resulting in a warm reset.

Sensor Subsystem Code Protection Region
=======================================
The Sensor Subsystem exposes a register (`SS_CFG.PROT_RANGE`) that disables any
ARC load or store operation to the specified SRAM region. During the boot
process, this register is set and locked to an ineffective state, in order to
prevent malicious code to use it as a DoS vector against ARC.

The locking is done using register `CFG_LOCK.PROT_RANGE_LOCK`.

Memory violation policy
=======================

The bootloader sets the memory violation policy for both flash and RAM to
trigger a warm reset. This is done by unmasking halt interrupts for the SRAM
and flash controllers and redirecting halt interrupts to trigger a reset.

For complete security the masking configuration for halt interrupts should be
locked, to prevent malicious code from masking them in order to disable the
warm reset. Since the lock applies to the halt interrupt configuration of every
peripheral, this is not done by the bootloader because doing so will prevent
applications from unmasking other halt interrupts they may need.

Application developers are therefore in charge of enabling the lock. They
should change the bootloader code in order to unmask all the halt interrupts
they need and then lock the configuration in there. This can be done by setting
bit `LOCK_HOST_HALT_MASK` of the `LOCK_INT_MASK_REG` register of the System
Control Subsystem.  Alternatively, if changing the bootloader is unpractical,
they can set the lock during the application initialization.

OTP read protection
===================

The bootloader does not enable read protection of OTP flash since the OTP
region of flash does not contain any private information.

If an application developer should decide to store private information in OTP,
they must read protect the OTP in the bootloader, before jumping to the
application, or as soon as the x86 application starts. This is done by setting
the `ROM_RD_DIS_U` and `ROM_RD_DIS_L` bits of the CTRL register of flash
controller 0.

`ROM_RD_DIS_L` protects the lower 4kB of OTP while `ROM_RD_DIS_U` protects the
upper 4kB. If the protection is enabled in the bootloader, the developer can
protect only half of the OTP and they must ensure that all the code to be run
after the protection setup is located in the unprotected half of the OTP
(otherwise a memory violation will be triggered).

OTP and JTAG lock
=================

The OTP lock must be enabled before the device is distributed or deployed. This
ensures that JTAG is disabled and ROM code cannot be modified.

Enforce core segregation
========================

On Intel® Quark™ SE microcontroller C1000, the bootloader code and the ARC
start-up code ensure some basic segregation between the x86 and the ARC
applications. The x86 core is the only one able to access or modify the x86 GDT
and IDT in SRAM, while the ARC core is the only one able to access the ARC IVT
in SRAM. The x86 GDT/IDT protection is set up by the bootloader using MPR0,
while the ARC IVT protection is set up by the ARC activation code using MPR1.

If allowed by their application use case (e.g., no shared memory is required
between the two cores), application developers are recommended to improve the
default core-segregation policy by completely separating the memory (both flash
and SRAM) of the two cores. This will ensure that data is not leaked from one
core to the other and that if a core is compromised it cannot be used to attack
the other one.

This can be done by setting up 2 additional MPRs and 2 additional FPRs, as
follows:

- MPR2 can be set-up to protect the x86 memory space. Only the x86 core and if
  needed DMA/USB should have access to this memory region.

    * Note: If DMA/USB access is not required, MPR0 can be reused by extending
      it to cover the entire x86 RAM region.

- MPR3 can be set-up to protect the ARC memory space. Only the ARC core and if
  needed DMA/USB should have access to this memory region.

    * Note: if DMA/USB access is not needed, MPR1 can be reused by extending it
      to cover the entire ARC RAM region.

- FPR0 on Flash controller 1 can be set-up to protect the x86 flash space. Only
  the x86 core and if needed DMA/USB should have read access to this memory
  region.

- FPR1 on Flash controller 0 can be set-up to protect the ARC flash space. Only
  the Sensor subsystem core and if needed DMA/USB should have read access to
  this memory region.

DMA should only be allowed on one of the two cores, otherwise it could be
abused to transfer memory from one cores memory space to the second cores
memory space.

Limit DMA access
================

As previously discussed, FPR and MPR should be used to limit DMA access to
those parts of flash and SRAM that are actually necessary. This will limit the
amount of damage an attacker can do by exploiting DMA-related bugs.

Stack protection
================

To improve security, the access of each processor stack should be limited to
the processor itself (i.e., DMA, USB or the other core should not have access
to it).

In order to ensure out-of-the-box compatibility with Zephyr 1.7, this is only
partially done by the bootloader. Specifically, MPR0, which protects the x86
GDT/IDT, only protects part of the x86 stack.

Covering the full x86 stack requires aligning the stack to the MPR granularity
(i.e.,1kB-alignment), in order to prevent the MPR protecting the stack from
also protecting part of the RAM used for statically allocated variables (i.e.,
the .bss and .data section).

If Zephyr 1.7 is not used, application developers are therefore recommended to
change the linker script and the bootloader code in order to extend the MPR to
cover the full x86 stack.

The ARC stack should also be protected. This can be done by moving the stack
close to the ARC IVT and extending MPR1, which is protecting it.
