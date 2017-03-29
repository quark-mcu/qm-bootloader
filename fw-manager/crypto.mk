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

ifeq ($(TINYCRYPT_SRC_DIR),)
$(error TINYCRYPT_SRC_DIR is not defined)
endif
$(info TINYCRYPT_SRC_DIR = $(TINYCRYPT_SRC_DIR))

### Variables
CRYPT_LIB_SRC_DIR = $(TINYCRYPT_SRC_DIR)/lib/source
CRYPT_LIB_INC_DIR = $(TINYCRYPT_SRC_DIR)/lib/include
CRYPT_SOURCES = $(wildcard $(CRYPT_LIB_SRC_DIR)/*.c)
CRYPT_OBJ_DIR = $(FM_OBJ_DIR)/tinycrypt
CRYPT_OBJS = $(addprefix $(CRYPT_OBJ_DIR)/,$(notdir $(CRYPT_SOURCES:.c=.o)))

### Flags
CFLAGS += -I$(CRYPT_LIB_INC_DIR)

### Build C files
$(CRYPT_OBJ_DIR)/%.o: $(CRYPT_LIB_SRC_DIR)/%.c
	$(call mkdir, $(CRYPT_OBJ_DIR))
	$(CC) $(CFLAGS) -c -o $@ $<

### Add TinyCrypt objects to the list of DM objects
ifeq ($(ENABLE_FIRMWARE_MANAGER_AUTH),1)
# Define the suffix to be used in binaries (1st-stage and 2nd-stage) compiled
# with ENABLE_FIRMWARE_MANAGER_AUTH=1
FM_AUTH_SUFFIX = _hmac
CFLAGS+= -DENABLE_FIRMWARE_MANAGER_AUTH=1
FM_OBJS += $(CRYPT_OBJS)
endif
