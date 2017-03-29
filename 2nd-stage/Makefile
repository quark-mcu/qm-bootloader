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

### 2nd Stage Bootloader makefile

### Variables
APP_NAME = 2nd_stage
APP_DIR := .
MK_BASE_DIR = $(APP_DIR)
SOC = quark_se
TARGET = x86
BL_BASE_DIR := ..

BUILD ?= release
CSTD ?= c99
ENABLE_FIRMWARE_MANAGER ?= usb
ENABLE_FIRMWARE_MANAGER_AUTH ?= 1

APP_NAME := $(APP_NAME)_$(ENABLE_FIRMWARE_MANAGER)

2ND_STAGE_BUILD_DIR = $(APP_DIR)/$(BUILD)/$(SOC)/$(TARGET)
2ND_STAGE_OBJ_DIR = $(2ND_STAGE_BUILD_DIR)/$(OBJ)
BIN_FILE = $(2ND_STAGE_BUILD_DIR)/$(BIN)/$(APP_NAME).bin
OBJ_DIRS += $(2ND_STAGE_BUILD_DIR)
GENERATED_DIRS += $(APP_DIR)/$(BUILD)
2ND_STAGE_SRCS = $(wildcard $(APP_DIR)/*.c)
2ND_STAGE_OBJS += $(addprefix $(2ND_STAGE_BUILD_DIR)/$(OBJ)/,\
		  $(notdir $(2ND_STAGE_SRCS:.c=.o)))
CFLAGS += -DPRINTF_ENABLE -DPUTS_ENABLE
CFLAGS += -DSTDOUT_UART_1 -DUART1_FTDI
CFLAGS += -DENABLE_FIRMWARE_MANAGER=1

# Add asserts for the 2nd stage booloader to improve security.
ifeq ($(BUILD),release)
CFLAGS += -DQM_CHECK_ASSERT=1 -DASSERT_ENABLE
endif

ifeq ($(ENABLE_FIRMWARE_MANAGER), usb)
CFLAGS += -DENABLE_FIRMWARE_MANAGER_USB=1
endif

ifeq ($(ENABLE_FIRMWARE_MANAGER),uart)
CFLAGS += -DENABLE_FIRMWARE_MANAGER_UART=1
endif

CFLAGS += -DBL_HAS_2ND_STAGE=1
CFLAGS += -I$(BL_BASE_DIR)/bootstrap
# Include SOC-specific bootloader headers
CFLAGS += -I$(BL_BASE_DIR)/bootstrap/soc/$(SOC)/include

LINKER_FILE := $(APP_DIR)/2nd_stage.ld

# Define BASE_OBJ_DIR for included makefiles
BASE_OBJ_DIR = $(2ND_STAGE_OBJ_DIR)

### Make includes
# TODO: Remove ROM build options from base.mk
include $(BL_BASE_DIR)/base.mk
# FM_OBJS
include $(BL_BASE_DIR)/fw-manager/fw-manager.mk
2ND_STAGE_OBJS += $(FM_OBJS)
include $(BL_BASE_DIR)/bootstrap/boot.mk
2ND_STAGE_OBJS += $(BOOT_OBJS)

# Include QMSI sys.mk, which returns SYS objects in the OBJECTS variable.
# sys.mk expects BASE_DIR to point to QMSI sources and APP_DIR to the 2nd-stage
# bootloader directory (the one in which this file is).
# TODO: try to get rid of this dependency from QMSI sources
BASE_DIR = $(QMSI_SRC_DIR)
include $(QMSI_SRC_DIR)/sys/sys.mk

ifeq ($(ENABLE_FIRMWARE_MANAGER),usb)
include $(BASE_DIR)/usb/usb_stack.mk
endif

2ND_STAGE_OBJS += $(OBJECTS)

# Add relevant suffixes to APP_NAME
APP_NAME := $(APP_NAME)$(FM_AUTH_SUFFIX)

all: $(BIN_FILE)

### Build C files in APP_DIR
$(2ND_STAGE_OBJ_DIR)/%.o: $(APP_DIR)/%.c qmsi
	$(call mkdir, $(2ND_STAGE_OBJ_DIR))
	$(CC) $(CFLAGS) -c -o $@ $<

### Link object files into APP ELF
$(BIN_FILE): $(LINKER_FILE) $(2ND_STAGE_OBJS) qmsi
	$(call mkdir, $(2ND_STAGE_BUILD_DIR)/$(BIN))
	$(LD) $(LDFLAGS) -Xlinker -T$(LINKER_FILE) \
		-Xlinker -A$(OUTPUT_ARCH) \
		-Xlinker --oformat$(OUTPUT_FORMAT) \
		-Xlinker -Map=$(2ND_STAGE_OBJ_DIR)/$(APP_NAME).map \
		-o $(2ND_STAGE_OBJ_DIR)/$(APP_NAME).elf $(2ND_STAGE_OBJS) \
		-Xlinker --start-group $(LDLIBS) -Xlinker --end-group
	$(SIZE) $(2ND_STAGE_OBJ_DIR)/$(APP_NAME).elf
	$(OBJCOPY) -O binary $(2ND_STAGE_OBJ_DIR)/$(APP_NAME).elf $@

qmsi:
	$(MAKE) -C $(QMSI_SRC_DIR) libqmsi SOC=$(SOC) TARGET=$(TARGET) BUILD=$(QMSI_BUILD)
