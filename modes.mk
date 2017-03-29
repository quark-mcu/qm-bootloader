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

# List supported modes
SUPPORTED_SOCS = quark_se \
		 quark_d2000

SUPPORTED_BUILDS = debug \
		   release

SUPPORTED_FM_MODE_quark_se = none \
			     uart \
			     2nd-stage

SUPPORTED_FM_MODE_quark_d2000 = none \
				uart

SUPPORTED_CSTD = c99 \
		 c90

SUPPORTED_FM_AUTH = 0 \
		    1

# Option to enable/disable flash write protection
ENABLE_FLASH_WRITE_PROTECTION ?= 1
SUPPORTED_ENABLE_FLASH_WRITE_PROTECTION = 0 \
					  1
$(info ENABLE_FLASH_WRITE_PROTECTION = $(ENABLE_FLASH_WRITE_PROTECTION))
ifeq ($(strip $(filter $(ENABLE_FLASH_WRITE_PROTECTION),\
				$(SUPPORTED_ENABLE_FLASH_WRITE_PROTECTION))), )
$(call support_error,ENABLE_FLASH_WRITE_PROTECTION,\
		     $(SUPPORTED_ENABLE_FLASH_WRITE_PROTECTION))
endif

# Exceptions
ifeq ($(ENABLE_FIRMWARE_MANAGER),uart)
ifeq ($(BUILD),debug)
$(error "Cannot combine (first-stage) Firmware Management over UART with \
	debug build due to footprint constraints.")
endif
endif
