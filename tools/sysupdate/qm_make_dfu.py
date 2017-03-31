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

""" qm-make-dfu: Script to convert binary files to QFU compatible images.

Usage
-----

::

   usage: qm_make_dfu.py [options] INFILE

Options
-------
::

Optional Arguments:
    -h, --help     show this help message and exit
    --version      show program's version number and exit
    -o OUTFILE     specify the output file name [default: <INFILE>.dfu]
    -q, --quiet    suppress non-error messages
    -v, --verbose  increase verbosity
    --sha256       add SHA-256 to header
    -c CFILE       specify the configuration file (C-header format)
    -p PART        target partition number [default: 0]
    --block-size   size of one dfu block [default: 2048]
This script uses C-style header files to generate QFU compatible.dfu image
files.
"""

from __future__ import print_function, division, absolute_import
import argparse
import os
import qmfmlib

__version__ = "0.1"

if __name__ == "__main__":
    # If you plan to use Unicode characters (e.g., ® and ™) in the following
    # string please ensure that your solution works fine on a Windows console.
    desc = "Intel(R) Quark(TM) Microcontroller DFU image generation tool."
    version = "%(prog)s {version}".format(version=__version__)
    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument(
        "input_file", metavar="INFILE", type=argparse.FileType('rb'),
        help="specify the input file name")
    parser.add_argument(
        '--version', action='version', version=version)
    parser.add_argument(
        "-o", default=False, metavar="OUTFILE", dest="output_file",
        help="specify the output file name [default: <INFILE>.dfu]")
    parser.add_argument(
        "--sha256", default=False, action="store_true",
        help="add SHA-256 to header")
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "-q", "--quiet", action="store_true",
        help="suppress non-error messages")
    group.add_argument(
        "-v", "--verbose", action="count",
        help="increase verbosity")
    parser.add_argument(
        "-c", metavar="CFILE", dest="config_file", type=argparse.FileType('r'),
        help="specify the configuration file (C-header format)")
    parser.add_argument(
        "-p", metavar="PART", type=int, dest="partition", required=True,
        help="target partition number [default: %(default)s]")
    parser.add_argument(
        "--block-size", metavar="SIZE", type=int, dest="block_size",
        default=None, help="dfu block size")
    parser.add_argument(
        "--soc", metavar="SOC", type=str, dest="soc",
        default="quark_se", help="Select the used target SoC[default: \
        %(default)s]" ,choices=['quark_se', 'quark_d2000'])
    parser.add_argument(
        "--key", metavar="KEY", type=argparse.FileType('r'), dest="key_file",
        help="sign the image using the specified HMAC key")
    # Configuration parameters
    parser.add_argument(
        "--vid", metavar="VID", type=int, dest="vid",
        default= None, help="The vendor id of the device")
    parser.add_argument(
        "--app-pid", metavar="APP_PID", type=int, dest="app_pid",
        default= None, help="The product id of the application")
    parser.add_argument(
        "--dfu-pid", metavar="DFU_PID", type=int, dest="dfu_pid",
        default= None, help="The product id of the bootloader")
    parser.add_argument(
        "--app-version", metavar="APP_VERSION", type=int, dest="app_version",
        default= None, help="The version of the application")
    parser.add_argument(
        "--svn", metavar="SVN", type=int, dest="svn", default= None,
        help="The security version number")

    args = parser.parse_args()

    if not args.output_file:
        args.output_file = args.input_file.name + ".dfu"

    # Prepare QFU Header
    header = qmfmlib.QFUHeader()
    header.partition_id = int(args.partition)
    header.padding = True

    # Populate header information from configuration file. This will overwrite
    # previous defined parameters.
    if args.config_file:
        try:
            header.set_from_file(args.config_file)
            args.config_file.close()
        except IOError as error:
            parser.error(error)
    header.overwrite_config_parameters(args)
    # Create image data for output file.
    data = None
    try:
        image = qmfmlib.QFUImage()
        add_sha256 = args.sha256
        if args.key_file:
            args.key_file.seek(0, os.SEEK_SET)
            key_data = args.key_file.read()
            args.key_file.close()
        else:
            key_data = None

        # Read input file size.
        file_content = args.input_file.read()
        data = image.make(header, file_content, key_data, add_sha256)

        args.input_file.close()
    except IOError as error:
        parser.error(error)

    dfu_image = qmfmlib.DFUImage()
    data = dfu_image.add_suffix(data)

    if args.verbose > 0:
        header.print_info(parser.prog + ": ")

    # Write output file.
    try:
        fh = open(args.output_file, "wb")
        fh.write(data)
        fh.close()
        if not args.quiet:
            print("%s: %s written" % (parser.prog, args.output_file))
    except IOError as error:
        parser.error(error)
