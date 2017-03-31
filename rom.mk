#
# Copyright (c) 2017, Intel Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# 3. Neither the name of the Intel Corporation nor the names of its
# contributors may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL CORPORATION OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

# This file defines the 'rom' target, to build the first stage bootloader
# (a.k.a. the ROM).

# Define where object files will be put
BASE_OBJ_DIR = $(BUILD_DIR)/$(BUILD)/$(SOC)/$(OBJ)

# Add common bootstrap objects to the list of ROM objects
include $(BL_BASE_DIR)/bootstrap/boot.mk
ROM_OBJS += $(BOOT_OBJS)
# Add soc-specific bootstrap objects to the list of ROM objects
include $(BL_BASE_DIR)/bootstrap/soc/boot_soc.mk
ROM_OBJS += $(BOOT_SOC_OBJS)

### Variables
# The directory where the ROM binary will be put
ROM_BUILD_DIR = $(BUILD_DIR)/$(BUILD)/$(SOC)/rom
OBJ_DIRS += $(BUILD_DIR)/$(BUILD)/$(SOC)

ROM_LINKER_FILE ?= $(BOOT_SOC_DIR)/rom.ld

# TODO: create a soc-specific mk for this or a centralized config.mk. This will
# be responsible for 1) documenting all possible build options, 2) doing the
# "contract checking" (i.e. if quark_d2000 and ENABLE_RESTORE_CONTEXT, fail),
# 3) expanding the CFLAGS that result from the build-time options.
ifeq ($(SOC),quark_se)
  ENABLE_RESTORE_CONTEXT ?= 1
  ifeq ($(ENABLE_RESTORE_CONTEXT),0)
  CFLAGS += -DENABLE_RESTORE_CONTEXT=0
  ROM_SUFFIX_NO_RESTORE_CONTEXT = _no_restore_context
  else
  CFLAGS += -DENABLE_RESTORE_CONTEXT=1
  endif
endif

# Always include fw-manager (FM) makefile to ensure accessibility to its header
# files
include $(BL_BASE_DIR)/fw-manager/fw-manager.mk

# Compile FM code (i.e., add FM objects as rom dependencies) only if
# ENABLE_FIRMWARE_MANAGER=1.
ifneq ($(ENABLE_FIRMWARE_MANAGER),none)
  ### Enable FM mode
  ROM_OBJS += $(FM_OBJS)
  CFLAGS += -DENABLE_FIRMWARE_MANAGER=1
  # Rom file name will have a '_fm' suffix
  ifeq ($(ENABLE_FIRMWARE_MANAGER),uart)
  CFLAGS += -DENABLE_FIRMWARE_MANAGER_UART=1
  ROM_SUFFIX_FM = _fm
  endif
  ifeq ($(ENABLE_FIRMWARE_MANAGER),2nd-stage)
  CFLAGS += -DENABLE_FIRMWARE_MANAGER_2ND_STAGE=1
  CFLAGS += -DBL_HAS_2ND_STAGE=1
  ROM_SUFFIX_FM = _fm_2nd_stage
  endif
endif

# Add write protection macro
ifeq ($(ENABLE_FLASH_WRITE_PROTECTION),1)
CFLAGS += -DENABLE_FLASH_WRITE_PROTECTION=1
else
CFLAGS += -DENABLE_FLASH_WRITE_PROTECTION=0
ROM_SUFFIX_NO_FLASH_WRITE_PROTECTION = _no_flash_write_protection
endif

# Define ROM file name
# (Suffix is built on multiple lines to respect 80 chars limit)
ROM_SUFFIX := $(ROM_SUFFIX_FM)
ROM_SUFFIX := $(ROM_SUFFIX)$(FM_AUTH_SUFFIX)
ROM_SUFFIX := $(ROM_SUFFIX)$(ROM_SUFFIX_NO_RESTORE_CONTEXT)
ROM_SUFFIX := $(ROM_SUFFIX)$(ROM_SUFFIX_NO_FLASH_WRITE_PROTECTION)
ROM_NAME := $(SOC)_rom$(ROM_SUFFIX)
ROM = $(ROM_BUILD_DIR)/$(ROM_NAME).bin

# Sort ROM objects.
# At this point, all ROM objects file have been defined; sort them to ensure
# that they are passed to the linker in the same order, regardless of the Make
# version (strangely enough, the order in which objects are passed to the
# linker affects the binary).
ROM_OBJS := $(sort $(ROM_OBJS))

.PHONY: rom

rom: $(ROM)

### Link STARTUP.elf and get raw binary
$(ROM): $(ROM_OBJS) qmsi
	$(call mkdir, $(ROM_BUILD_DIR))
	$(LD) $(LDFLAGS) -Xlinker -T$(ROM_LINKER_FILE) \
		-Xlinker -A$(OUTPUT_ARCH) \
		-Xlinker --oformat$(OUTPUT_FORMAT) \
		-Xlinker -Map=$(BOOT_SOC_OBJ_DIR)/$(ROM_NAME).map \
		-o $(BOOT_SOC_OBJ_DIR)/$(ROM_NAME).elf $(ROM_OBJS) $(LDLIBS)
	$(SIZE) $(BOOT_SOC_OBJ_DIR)/$(ROM_NAME).elf
	$(OBJCOPY) --gap-fill 0xFF \
		    -O binary $(BOOT_SOC_OBJ_DIR)/$(ROM_NAME).elf $@

