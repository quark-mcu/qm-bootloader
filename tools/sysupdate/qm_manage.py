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

"""qm-update: Script for Quark Microcontroller management.

Note: Need more verbose description.

::

   usage: qm_manage.py CMD [options]"""

from __future__ import print_function, division, absolute_import
import os
import sys
import argparse
import subprocess
import tempfile
import qmfmlib
import re
import collections
__version__ = "1.4"
# If you plan to use Unicode characters (e.g., ® and ™) in the following string
# please ensure that your solution works fine on a Windows console.
DESC = "Intel(R) Quark(TM) Microcontroller Firmware Management tool."
DFU_STATUS_ERR_TARGET = 1
DFU_STATUS_ERR_VENDOR = 11


class QMManageException(Exception):
    """QM Manage Exception."""

    def __init__(self, message):
        super(QMManageException, self).__init__(message)


class QMManage(object):
    """Class containing all manage functionality."""

    def __init__(self, parser):
        self.parser = parser
        self.args = None

    def _add_parser_con_arguments(self):
        """Adds parser defaults to argparse instance."""
        self.parser.add_argument(
            "-p", metavar="SERIAL_PORT", type=str, dest="port", required=False,
            help="specify the serial port to use")

        self.parser.add_argument(
            "-d", metavar="USB_DEVICE", type=str, dest="device", required=False,
            help="specify the USB device (vendor:product) to use")

        self.parser.add_argument(
            "-S", metavar="USB_SERIAL", type=str, dest="serial", required=False,
            help="specify the serial string of the USB device to use")

    def _command(self):
        """Evaluate command line arguments and decide what transport to use."""
        cmd = []
        # -p SERIAL_PORT
        if self.args.port:
            if self.args.device or self.args.serial:
                self.parser.error("cannot combine -p with -d or -S option")
            cmd.append("dfu-util-qda")
            cmd.append("-p")
            cmd.append(self.args.port)

        else:
            cmd.append("dfu-util")

            # -d VENDOR:PRODUCT
            if self.args.device:
                cmd.append("-d")
                cmd.append(self.args.device)

            if self.args.serial:
                cmd.append("-S")
                cmd.append(self.args.serial)

        if len(cmd) < 2:
            self.parser.error("no device specified. Use -p, -d or -S")

        return cmd

    def _create_temp(self, data):
        """Create a temporary data file containing data."""
        file_name = None
        try:
            file_handler = tempfile.NamedTemporaryFile("wb", delete=False)
            file_name = file_handler.name
            file_handler.write(data)
            file_handler.close()
        except IOError as error:
            self.parser.error(error)
        return file_name

    def call_tools(self, cmd):
        """Call command and return a tuple with status code and stdout."""
        return_value = collections.namedtuple('return_value', ['status', 'out'])
        sys.stdout.flush()
        p = subprocess.Popen(cmd,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
        out, err = p.communicate()
        if self.args.verbose:
            print("\n" + out)

        if err:
            return return_value(status=-1, out=out)

        # Check if we there is an error in the output
        m = re.search(r'dfuERROR.*?status\(([0-9]*)', out)
        if m:
            return return_value(status=int(m.group(1)), out=out)

        return return_value(status=0, out=out)

    def list_devices(self):
        """Perform 'list' tasks."""
        self.parser.description += " Retrieve list of connected USB devices."
        self.parser.add_argument(
            "-d", metavar="USB_DEVICE", type=str, dest="device", required=False,
            help="specify the USB device (vendor:product) to use")
        self.args = self.parser.parse_args()

        cmd = ["dfu-util", "-l"]
        # -d VENDOR:PRODUCT
        if self.args.device:
            cmd.append("-d")
            cmd.append(self.args.device)

        print("Reading USB device list...\t\t", end="")
        retv = self.call_tools(cmd)
        if retv.status:
            print("[FAIL]")
            exit(1)
        print("[DONE]")
        print(retv.out)

    def info(self):
        """Perform 'info' tasks."""

        self.parser.description += " Retrieve device information."
        self._add_parser_con_arguments()
        self.parser.add_argument("--format", choices=['text', 'json'],
                                 help="presentation format [default: text]")
        self.args = self.parser.parse_args()
        cmd = self._command()

        # Prepare sys info request.
        request = qmfmlib.QFMRequest(qmfmlib.QFMRequest.REQ_SYS_INFO).content
        image = qmfmlib.DFUImage()
        data = image.add_suffix(request)

        # Write temp output file. This file will be passed on to dfu-utils-qda.
        file_name = self._create_temp(data)
        # Download request to device. Using alternate setting 0 (QFM).
        print("Requesting system information...\t", end="")
        retv = self.call_tools(cmd + ["-D", file_name, "-a", "0"])

        if retv.status:
            print("[FAIL]")
            os.remove(file_name)
            exit(1)
        print("[DONE]")
        os.remove(file_name)

        # Create and delete a temporary file. This is done to check the file
        # permissions of the output file we give dfu-utils-qda to store the
        # result of our requested response.
        file_name = self._create_temp("")
        os.remove(file_name)

        # Upload response from device. Using alternate setting 0 (QFM).
        print("Reading system information...\t\t", end="")
        retv = self.call_tools(cmd + ["-U", file_name, "-a", "0"])

        if retv.status:
            print("[FAIL]")
            exit(1)

        print("[DONE]")
        in_file = open(file_name, "rb")
        response = qmfmlib.QFMResponse(in_file.read())
        in_file.close()
        os.remove(file_name)

        if not response.cmd == qmfmlib.QFMResponse.RESP_SYS_INFO:
            print("Error: Invalid response.")
            exit(1)

        # Parse and Present Sys-Info data.
        info = qmfmlib.QFMSysInfo(response.content)

        if self.args.format == "json":
            print(info.info_json())
        else:
            print(info.info_string())

    def erase(self):
        """Perform 'erase' tasks."""
        self.parser.description += "Erase all applications on the device."
        self._add_parser_con_arguments()
        self.args = self.parser.parse_args()
        cmd = self._command()

        request = qmfmlib.QFMRequest(qmfmlib.QFMRequest.REQ_APP_ERASE).content
        image = qmfmlib.DFUImage()
        data = image.add_suffix(request)

        # Write output file.
        file_name = self._create_temp(data)
        print("Erasing all application data...\t\t", end="")
        retv = self.call_tools(cmd + ["-D", file_name, "-a", "0"])

        if retv.status:
            print("[FAIL]")
            os.remove(file_name)
            if retv.status == DFU_STATUS_ERR_TARGET:
                print("Application erase is not supported by the device.")
            else:
                print("Unknown error.")
                if not self.args.verbose:
                    print("Run in verbose mode for more info.")
            exit(1)

        print("[DONE]")
        os.remove(file_name)

    def set_key(self, key_type):
        self._add_parser_con_arguments()

        self.parser.add_argument("new_key_file", metavar="key",
            type=argparse.FileType('rb'),
            help="specify the new key file")

        self.parser.add_argument(
           "--curr-fw-key", dest="curr_fw_key_file", metavar="CURRENT_FW_KEY",
            type=argparse.FileType('rb'),
            required=False, help="specify the current fw key file")

        self.parser.add_argument(
           "--curr-rv-key", dest="curr_rv_key_file", metavar="CURRENT_RV_KEY",
            type=argparse.FileType('rb'),
            required=False, help="specify the current revocation key file")

        self.args = self.parser.parse_args()
        cmd = self._command()

        # Read the content of the new key file
        self.args.new_key_file.seek(0, os.SEEK_END)
        # Check if the size is correct
        if self.args.new_key_file.tell() != 32:
            self.args.new_key_file.close()
            raise QMManageException("Incorrect length of the new key")

        self.args.new_key_file.seek(0, os.SEEK_SET)
        new_key_content = self.args.new_key_file.read()

        # Read the content of the current fw key file
        if self.args.curr_fw_key_file:
            self.args.curr_fw_key_file.seek(0, os.SEEK_END)
            # Check if the size is correct
            if self.args.curr_fw_key_file.tell() != 32:
                self.args.curr_fw_key_file.close()
                raise QMManageException("Incorrect length of the current fw  \
                                        key")

            self.args.curr_fw_key_file.seek(0, os.SEEK_SET)
            curr_fw_key_content = self.args.curr_fw_key_file.read()
        else:
            curr_fw_key_content = ""

        # Read the content of the current revocation key file
        if self.args.curr_rv_key_file:
            self.args.curr_rv_key_file.seek(0, os.SEEK_END)
            # Check if the size is correct
            if self.args.curr_rv_key_file.tell() != 32:
                self.args.curr_rv_key_file.close()
                raise QMManageException("Incorrect length of the current rv  \
                                        key")

            self.args.curr_rv_key_file.seek(0, os.SEEK_SET)
            curr_rv_key_content = self.args.curr_rv_key_file.read()
        else:
            curr_rv_key_content = ""

        request = qmfmlib.QFMSetKey(new_key_content,
                                    curr_fw_key_content,
                                    curr_rv_key_content,
                                    key_type).content

        image = qmfmlib.DFUImage()
        data = image.add_suffix(request)
        # Write output file.
        file_name = self._create_temp(data)
        print("Programming new device key...\t\t", end="")
        retv = self.call_tools(cmd + ["-D", file_name, "-a", "0"])
        if retv.status != 0:
            print("[FAIL]")

            if retv.status == DFU_STATUS_ERR_TARGET:
                print("key provisioning is not supported by the device.")
            elif retv.status == DFU_STATUS_ERR_VENDOR:
                print("Key verification failed.")
            else:
                print("Unknown error.")
                if not self.args.verbose:
                    print("Run in verbose mode for more info.")

            os.remove(file_name)
            exit(1)

        print("[DONE]")
        os.remove(file_name)
        self.args.new_key_file.close()

        if self.args.curr_fw_key_file:
            self.args.curr_fw_key_file.close()

    def set_rv_key(self):
        self.set_key(qmfmlib.QFMRequest.REQ_SET_RV_KEY)

    def set_fw_key(self):
        self.set_key(qmfmlib.QFMRequest.REQ_SET_FW_KEY)


def main():
    """The main function."""
    version = "qm_manage {version}".format(version=__version__)

    if "--version" in sys.argv:
        print (version)
        exit(0)

    choices_desc = "possible commands:\n" + \
                   "  set-fw-key    set the HMAC fw key used for firmware \
                                    authentication\n" + \
                   "  set-rv-key    set the HMAC rv key used for firmware \
                                    authentication\n" + \
                   "  erase         erase all applications\n" + \
                   "  info          retrieve device information\n" + \
                   "  list          retrieve list of connected devices"
    _parser = argparse.ArgumentParser(
        description=DESC,
        epilog=choices_desc,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    # Note: Version is not parsed by argparse if --version is not added.
    _parser.add_argument('--version', action='version', version=version)
    _parser.add_argument("cmd", help="run specific command",
                         choices=['set-fw-key', 'set-rv-key', 'info', 'erase',
                                  'list'])
    group = _parser.add_mutually_exclusive_group()
    group.add_argument("-q", "--quiet", action="store_true",
                       help="suppress non-error messages")
    group.add_argument("-v", "--verbose", action="count",
                       help="increase verbosity")

    # parse for command only
    args = _parser.parse_args(sys.argv[1:2])

    manager = QMManage(_parser)
    if args.cmd == "info":
        manager.info()
        exit(0)

    if args.cmd == "list":
        manager.list_devices()
        exit(0)

    if args.cmd == "erase":
        manager.erase()
        exit(0)

    if args.cmd == "set-rv-key":
        manager.set_rv_key()
        exit(0)

    if args.cmd == "set-fw-key":
        manager.set_fw_key()
        exit(0)

    exit(1)

if __name__ == "__main__":
    main()
