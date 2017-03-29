#!/usr/bin/python -tt
# -*- coding: utf-8 -*-
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

"""Quark Microcontroller Firmware Update Module

This module provides classes to create and manipulate firmware images for Quark
Microcontrollers."""

from __future__ import print_function, division, absolute_import
import re
import struct
import hashlib
import hmac

_ENDIAN = "<"   # Defines the endian for struct packing. ('<'=little, '>'=big)

# The possible types of extended header.
_QFU_EXT_HDR_NONE = 0
_QFU_EXT_HDR_SHA256 = 1
_QFU_EXT_HDR_HMAC256 = 2


class QFUException(Exception):
    """QFU Exception."""

    def __init__(self, message):
        super(QFUException, self).__init__(message)


class QFUDefineParser(object):
    """A simple parser for C header files to extract #define of integers

    Note:
        We only parse simple #define macros like::

            #define DFU_ID_VENDOR (0x1200)"""

    defines = {}

    VENDOR_ID = "QFU_VENDOR_ID"
    PRODUCT_ID_DFU = "QFU_DFU_PRODUCT_ID"
    PRODUCT_ID_APP = "QFU_APP_PRODUCT_ID"
    VERSION = "QFU_VERSION"
    BLOCK_SIZE = "QFU_BLOCK_SIZE"
    SVN = "QFU_SVN"

    # Compiled regular expressions for `#define A int` or `#define A (int)`
    _re_int_line = re.compile(
        r"^\s*\#define\s+(\S+)\s+\(?(\d+)\)?")
    # Compiled regular expressions for `#define A hex` or `#define A (hex)`
    _re_hex_line = re.compile(
        r"^\s*\#define\s+(\S+)\s+\(?0x([0-9,a-f,A-F]+)\)?")

    def _check_line(self, line):
        """Search for valid defines in a line."""
        match = self._re_hex_line.match(line)
        if match:
            grp = match.groups()
            self.defines[grp[0]] = int(grp[1], 16)
            return
        match = self._re_int_line.match(line)
        if match:
            grp = match.groups()
            self.defines[grp[0]] = int(grp[1])
            return

    def __init__(self, open_file):
        """Opens and parses a C header like file for integer defines."""
        for line in open_file.readlines():
            self._check_line(line)


class QFUImage(object):
    """Creates a QFU compatible file from a binary file."""

    def __init__(self):
        self.ext_headers = []

    def make(self, header, image_data, key=None, add_sha256=False):
        """Assembles the QFU Header and the binary data.

        Args:
            header (QFUHeader): Header containing all relevant information to
                                create the image.
            image_data (string): Input file data.
            add_sha256 (Bool): Add a sha256 hash to the header.
        Returns:
            The newly constructed binary data."""

        ext_header = QFUExtHeaderNone()
        if add_sha256:
            ext_header = QFUExtHeaderSHA256(image_data)
        elif key:
            ext_header = QFUExtHeaderHMAC256(image_data, header, key)

        data_blocks = ((len(image_data) - 1) // header.block_size) + 1
        header_blocks = ((header.SIZE + ext_header.size() - 1)
                        // header.block_size) + 1
        header.num_blocks = data_blocks + header_blocks

        header.add_extended_header(ext_header)
        # Set QFU header and DFU suffix.
        content = header.packed_qfu_header
        content += image_data
        return content


class QFUExtHeader(object):
    """Generic Extended header class."""
    def __init__(self, ext_hdr_id):
        self.content = ""
        self.hdr_id = ext_hdr_id

    def size(self):
        """Return the size of the extended header, which is a minimum of 4"""
        return 4

    def compute(self):
        pass


class QFUExtHeaderNone(QFUExtHeader):
    """None-Extended Header class. This header contains of empty 32 bits."""
    def __init__(self):
        self._struct = struct.Struct("%sHH" % _ENDIAN)
        super(QFUExtHeaderNone, self).__init__(_QFU_EXT_HDR_NONE)

    def compute(self):
        """Compute extended header content."""
        self.content = self._struct.pack(self.hdr_id, 0)

    def size(self):
        """Return the size of the extended header (4 bytes)"""
        return super(QFUExtHeaderNone, self).size()


class QFUExtHeaderSHA256(QFUExtHeader):
    """SHA256 extended header class.
    Params:
        data (`string`): Content of the binary file."""

    def __init__(self, file_content):
        self.data = file_content
        self._struct = struct.Struct("%sHH32s" % _ENDIAN)
        super(QFUExtHeaderSHA256, self).__init__(_QFU_EXT_HDR_SHA256)

    def compute(self):
        """Compute extended header content."""

        if not self.data:
            raise QFUException("No data defined for SHA256 calculation.")
        hasher = hashlib.sha256()
        hasher.update(self.data)
        self.content = self._struct.pack(self.hdr_id, 0, hasher.digest())

    def size(self):
        """Return the size of the extended hdr (4bytes + 32bytes = 36bytes)"""
        return 32 + super(QFUExtHeaderSHA256, self).size()


class QFUExtHeaderHMAC256(QFUExtHeader):
    """HMAC256 extended header class."""

    def __init__(self, data, header, key):
        self.data = data
        self.key = key
        self.svn = header.svn
        self.header = header
        self.data_blocks = ((len(data) - 1) // header.block_size) + 1
        super(QFUExtHeaderHMAC256, self).__init__(_QFU_EXT_HDR_HMAC256)

    def compute_blocks(self, block_size, block_cnt):
        """Compute the sha checksum for each block.

        Args:
            block_size (`int`): Size of each block.
            block_cnt (`int`): Number of blocks."""

        sha_blocks = ""
        block_struct = struct.Struct("%s32s" % _ENDIAN)
        # Caculate hash for all blocks
        nr_blocks = len(self.data) // block_size

        start = 0
        end = block_size
        for i in range(0, nr_blocks):
            hasher = hashlib.sha256()

            hash_data = self.data[start:end]

            hasher.update(hash_data)
            sha_blocks += block_struct.pack(hasher.digest())
            start += block_size
            end += block_size

        # Handle the last block if present.'
        if(start < len(self.data)):
            hasher = hashlib.sha256()
            hash_data = self.data[start:len(self.data)]

            hasher.update(hash_data)
            sha_blocks += block_struct.pack(hasher.digest())

        return sha_blocks

    def compute(self):
        """Compute extended header content."""

        header_struct = struct.Struct("%sHHI" % _ENDIAN)
        if not self.data:
            raise QFUException("No data defined for SHA256 calculation.")
        if not self.key:
            raise QFUException("No key defined for HMAC256 calculation.")
        # if not self.svn:
        #    raise QFUException("No Security version number defined.")

        self.content = header_struct.pack(self.hdr_id, 0, self.svn)

        self.content += self.compute_blocks(self.header.block_size,
                                            self.header.num_blocks)

        # Sign the header
        self.content += hmac.new(bytes(self.key),
                                 (bytes(self.header.get_base_header()) +
                                  bytes(self.content)),
                                 digestmod = hashlib.sha256).digest()

    def size(self):
        """Return the size of the extended header 4 bytes as usal + 4 bytes SVN
        + sha256 for each block + final hmac256."""
        return (4 + (self.data_blocks * 32) + 32 +
                super(QFUExtHeaderHMAC256, self).size())


class QFUHeader(object):
    """The class holding QFU Header and DFU Suffix information

    Attributes:
        id_vendor (int): The DFU/USB vendor id.
        id_product (int): The DFU/USB product id.
        id_product_dfu (int): The DFU specific product id.
        partition_id (int): Target partition number.
        version (int): Firmware version of this image.
        block_size (int): The DFU block size.
        num_blocks (int): The number of blocks in this image.
        ext_headers(`list`): List of extended headers.

"""
    SIZE = 20
    id_vendor = 0
    id_product = 0
    id_product_dfu = 0
    partition_id = 0
    version = 0
    block_size = None
    num_blocks = 0
    ext_headers = []
    svn = 0
    # Different structure formats. _ENDIAN defines little or big-endian.
    # H stands for uint16, I for uint32 and c for a single character.
    _header_struct = struct.Struct("%sHHHHIHH" % _ENDIAN)

    def __init__(self):
        self.ext_headers = []
        pass

    def add_extended_header(self, header):
        """Add an extended header.

        Args:
            header (`QFUExtHeader`): extended header."""
        self.ext_headers.insert(-1, header)

    def print_info(self, prefix=""):
        """Prints verbose QFU Header and information."""
        inset = " " * len(prefix)
        print("%sQFU-Header content:" % prefix)
        print("%s    Partition:   %d" % (inset, self.partition_id))
        print("%s    VID:         0x%04x" % (inset, self.id_vendor))
        print("%s    PID:         0x%04x" % (inset, self.id_product))
        print("%s    DFU PID:     0x%04x" % (inset, self.id_product_dfu))
        print("%s    Version:     %d" % (inset, self.version))
        print("%s    Block Size:  %d" % (inset, self.block_size))
        print("%s    Blocks:      %d" % (inset, self.num_blocks))

    def overwrite_config_parameters(self, args):
        """Read arguments from the command line and overwrites the config
        parameters

        Args:
            args: Command-line arguments.
        """
        if args.vid is not None:
            self.id_vendor = args.vid
        if args.app_pid is not None:
            self.id_product = args.app_pid
        if args.app_version is not None:
            self.version = args.app_version
        if args.block_size is not None:
            self.block_size = args.block_size
        if args.svn is not None:
            self.svn = args.svn
        if args.dfu_pid is not None:
            self.id_product_dfu = args.dfu_pid

        if self.block_size is None:
            if args.soc == "quark_se":
                self.block_size = 4096
            else:
                self.block_size = 2048

    def set_from_file(self, open_file):
        """Read configuration file (C-header format) and update header
        information.

        Args:
            open_file (file): An open file with read permission. The file
                              needs to contain C-header style defines."""

        conf = QFUDefineParser(open_file)

        # Map configuration to class variables.
        if QFUDefineParser.VENDOR_ID in conf.defines:
            self.id_vendor = conf.defines[QFUDefineParser.VENDOR_ID]
        if QFUDefineParser.PRODUCT_ID_APP in conf.defines:
            self.id_product = conf.defines[QFUDefineParser.PRODUCT_ID_APP]
        if QFUDefineParser.PRODUCT_ID_DFU in conf.defines:
            self.id_product_dfu = conf.defines[QFUDefineParser.PRODUCT_ID_DFU]
        if QFUDefineParser.VERSION in conf.defines:
            self.version = conf.defines[QFUDefineParser.VERSION]
        if QFUDefineParser.BLOCK_SIZE in conf.defines:
            self.block_size = conf.defines[QFUDefineParser.BLOCK_SIZE]
        if QFUDefineParser.SVN in conf.defines:
            self.svn = conf.defines[QFUDefineParser.SVN]

    def set_from_data(self, data):
        """Update header information from binary data string.

        Args:
            data (string): A string containing header packed information."""

        if data[:4] != 'QFUH':
            raise QFUException("QFUH prefix missing")
        data = data[4:]
        try:
            unpacked_data = self._header_struct.unpack(data)
            (
                self.id_vendor,
                self.id_product,
                self.id_product_dfu,
                self.partition_id,
                self.version,
                self.block_size,
                self.num_blocks,
            ) = unpacked_data

        except struct.error:
            raise QFUException("QFU Header length not valid")

    def get_base_header(self):
        """Return the base header."""

        return "QFUH" + self._header_struct.pack(*self._pack_header_tuple)

    @property
    def _pack_header_tuple(self):
        """Tuple containing the header information in a defined order."""

        return (
            self.id_vendor,
            self.id_product,
            self.id_product_dfu,
            self.partition_id,
            self.version,
            self.block_size,
            self.num_blocks,
        )

    @property
    def packed_qfu_header(self):
        """Binary representation of QFU header."""

        ret = self.get_base_header()

        # Add extended headers
        for header in self.ext_headers:
            header.compute()
            ret += header.content

        # Add padding
        ret += b'\x00' * (self.block_size - (len(ret) % self.block_size))
        return ret
