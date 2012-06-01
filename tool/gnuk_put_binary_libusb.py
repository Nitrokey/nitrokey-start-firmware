#! /usr/bin/python

"""
gnuk_put_binary.py - a tool to put binary to Gnuk Token
This tool is for importing certificate, updating random number, etc.

Copyright (C) 2011, 2012 Free Software Initiative of Japan
Author: NIIBE Yutaka <gniibe@fsij.org>

This file is a part of Gnuk, a GnuPG USB Token implementation.

Gnuk is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Gnuk is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

from intel_hex import *
from struct import *
import sys, time, os, binascii, string

# INPUT: binary file

# Assume only single CCID device is attached to computer, and it's Gnuk Token

import usb

# USB class, subclass, protocol
CCID_CLASS = 0x0B
CCID_SUBCLASS = 0x00
CCID_PROTOCOL_0 = 0x00

def icc_compose(msg_type, data_len, slot, seq, param, data):
    return pack('<BiBBBH', msg_type, data_len, slot, seq, 0, param) + data

def iso7816_compose(ins, p1, p2, data, cls=0x00):
    data_len = len(data)
    if data_len == 0:
        return pack('>BBBB', cls, ins, p1, p2)
    else:
        return pack('>BBBBB', cls, ins, p1, p2, data_len) + data

# This class only supports Gnuk (for now) 
class gnuk_token:
    def __init__(self, device, configuration, interface):
        """
        __init__(device, configuration, interface) -> None
        Initialize the device.
        device: usb.Device object.
        configuration: configuration number.
        interface: usb.Interface object representing the interface and altenate setting.
        """
        if interface.interfaceClass != CCID_CLASS:
            raise ValueError, "Wrong interface class"
        if interface.interfaceSubClass != CCID_SUBCLASS:
            raise ValueError, "Wrong interface sub class"
        self.__devhandle = device.open()
        try:
            self.__devhandle.setConfiguration(configuration)
        except:
            pass
        self.__devhandle.claimInterface(interface)
        self.__devhandle.setAltInterface(interface)

        self.__intf = interface.interfaceNumber
        self.__alt = interface.alternateSetting
        self.__conf = configuration

        self.__bulkout = 1
        self.__bulkin  = 0x81

        self.__timeout = 10000
        self.__seq = 0

    def icc_get_result(self):
        msg = self.__devhandle.bulkRead(self.__bulkin, 1024, self.__timeout)
        if len(msg) < 10:
            print msg
            raise ValueError, "icc_get_result"
        msg_type = msg[0]
        data_len = msg[1] + (msg[2]<<8) + (msg[3]<<16) + (msg[4]<<24)
        slot = msg[5]
        seq = msg[6]
        status = msg[7]
        error = msg[8]
        chain = msg[9]
        data = msg[10:]
        # XXX: check msg_type, data_len, slot, seq, error
        return (status, chain, data)

    def icc_get_status(self):
        msg = icc_compose(0x65, 0, 0, self.__seq, 0, "")
        self.__devhandle.bulkWrite(self.__bulkout, msg, self.__timeout)
        self.__seq += 1
        status, chain, data = self.icc_get_result()
        # XXX: check chain, data
        return status

    def icc_power_on(self):
        msg = icc_compose(0x62, 0, 0, self.__seq, 0, "")
        self.__devhandle.bulkWrite(self.__bulkout, msg, self.__timeout)
        self.__seq += 1
        status, chain, data = self.icc_get_result()
        # XXX: check status, chain
        return data             # ATR

    def icc_power_off(self):
        msg = icc_compose(0x63, 0, 0, self.__seq, 0, "")
        self.__devhandle.bulkWrite(self.__bulkout, msg, self.__timeout)
        self.__seq += 1
        status, chain, data = self.icc_get_result()
        # XXX: check chain, data
        return status

    def icc_send_data_block(self, data):
        msg = icc_compose(0x6f, len(data), 0, self.__seq, 0, data)
        self.__devhandle.bulkWrite(self.__bulkout, msg, self.__timeout)
        self.__seq += 1
        return self.icc_get_result()

    def icc_send_cmd(self, data):
        status, chain, data_rcv = self.icc_send_data_block(data)
        if chain == 0:
            return data_rcv
        elif chain == 1:
            d = data_rcv
            while True:
                msg = icc_compose(0x6f, 0, 0, self.__seq, 0x10, "")
                self.__devhandle.bulkWrite(self.__bulkout, msg, self.__timeout)
                self.__seq += 1
                status, chain, data_rcv = self.icc_get_result()
                # XXX: check status
                d += data_rcv
                if chain == 2:
                    break
                elif chain == 3:
                    continue
                else:
                    raise ValueError, "icc_send_cmd chain"
            return d
        else:
            raise ValueError, "icc_send_cmd"

    def cmd_get_response(self, expected_len):
        cmd_data = iso7816_compose(0xc0, 0x00, 0x00, '') + pack('>B', expected_len)
        response = self.icc_send_cmd(cmd_data)
        return response[:-2]

    def cmd_verify(self, who, passwd):
        cmd_data = iso7816_compose(0x20, 0x00, 0x80+who, passwd)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, sw
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError, sw

    def cmd_read_binary(self, fileid):
        cmd_data = iso7816_compose(0xb0, 0x80+fileid, 0x00, '')
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, sw
        if sw[0] != 0x61:
            raise ValueError, ("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_write_binary(self, fileid, data):
        count = 0
        data_len = len(data)
        while count*256 < data_len:
            if count == 0:
                if len(data) < 128:
                    cmd_data0 = iso7816_compose(0xd0, 0x80+fileid, 0x00, data[:128])
                    cmd_data1 = None
                else:
                    cmd_data0 = iso7816_compose(0xd0, 0x80+fileid, 0x00, data[:128], 0x10)
                    cmd_data1 = iso7816_compose(0xd0, 0x80+fileid, 0x00, data[128:256])
            else:
                if len(data[256*count:256*count+128]) < 128:
                    cmd_data0 = iso7816_compose(0xd0, count, 0x00, data[256*count:256*count+128])
                    cmd_data1 = None
                else:
                    cmd_data0 = iso7816_compose(0xd0, count, 0x00, data[256*count:256*count+128], 0x10)
                    cmd_data1 = iso7816_compose(0xd0, count, 0x00, data[256*count:256*(count+1)])
            sw = self.icc_send_cmd(cmd_data0)
            if len(sw) != 2:
                raise ValueError, "cmd_write_binary 0"
            if not (sw[0] == 0x90 and sw[1] == 0x00):
                raise ValueError, "cmd_write_binary 0"
            if cmd_data1:
                sw = self.icc_send_cmd(cmd_data1)
                if len(sw) != 2:
                    raise ValueError, "cmd_write_binary 1"
                if not (sw[0] == 0x90 and sw[1] == 0x00):
                    raise ValueError, "cmd_write_binary 1"
            count += 1

    def cmd_update_binary(self, fileid, data):
        count = 0
        data_len = len(data)
        while count*256 < data_len:
            if count == 0:
                if len(data) < 128:
                    cmd_data0 = iso7816_compose(0xd6, 0x80+fileid, 0x00, data[:128])
                    cmd_data1 = None
                else:
                    cmd_data0 = iso7816_compose(0xd6, 0x80+fileid, 0x00, data[:128], 0x10)
                    cmd_data1 = iso7816_compose(0xd6, 0x80+fileid, 0x00, data[128:256])
            else:
                if len(data[256*count:256*count+128]) < 128:
                    cmd_data0 = iso7816_compose(0xd6, count, 0x00, data[256*count:256*count+128])
                    cmd_data1 = None
                else:
                    cmd_data0 = iso7816_compose(0xd6, count, 0x00, data[256*count:256*count+128], 0x10)
                    cmd_data1 = iso7816_compose(0xd6, count, 0x00, data[256*count:256*(count+1)])
            sw = self.icc_send_cmd(cmd_data0)
            if len(sw) != 2:
                raise ValueError, "cmd_write_binary 0"
            if not (sw[0] == 0x90 and sw[1] == 0x00):
                raise ValueError, "cmd_write_binary 0"
            if cmd_data1:
                sw = self.icc_send_cmd(cmd_data1)
                if len(sw) != 2:
                    raise ValueError, "cmd_write_binary 1"
                if not (sw[0] == 0x90 and sw[1] == 0x00):
                    raise ValueError, "cmd_write_binary 1"
            count += 1

    def cmd_select_openpgp(self):
        cmd_data = iso7816_compose(0xa4, 0x04, 0x0c, "\xD2\x76\x00\x01\x24\x01")
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, sw
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError, ("%02x%02x" % (sw[0], sw[1]))

    def cmd_get_data(self, tagh, tagl):
        cmd_data = iso7816_compose(0xca, tagh, tagl, "")
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, sw
        if sw[0] != 0x61:
            raise ValueError, ("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

def compare(data_original, data_in_device):
    i = 0 
    for d in data_original:
        if ord(d) != data_in_device[i]:
            raise ValueError, "verify failed at %08x" % i
        i += 1

def gnuk_devices():
    busses = usb.busses()
    for bus in busses:
        devices = bus.devices
        for dev in devices:
            for config in dev.configurations:
                for intf in config.interfaces:
                    for alt in intf:
                        if alt.interfaceClass == CCID_CLASS and \
                                alt.interfaceSubClass == CCID_SUBCLASS and \
                                alt.interfaceProtocol == CCID_PROTOCOL_0:
                            yield dev, config, alt

DEFAULT_PW3 = "12345678"
BY_ADMIN = 3

def main(fileid, is_update, data, passwd):
    icc = None
    for (dev, config, intf) in gnuk_devices():
        try:
            icc = gnuk_token(dev, config, intf)
            print "Device: ", dev.filename
            print "Configuration: ", config.value
            print "Interface: ", intf.interfaceNumber
            break
        except:
            pass
    if icc.icc_get_status() == 2:
        raise ValueError, "No ICC present"
    elif icc.icc_get_status() == 1:
        icc.icc_power_on()
    icc.cmd_verify(BY_ADMIN, passwd)
    if is_update:
        icc.cmd_update_binary(fileid, data)
    else:
        icc.cmd_write_binary(fileid, data)
    icc.cmd_select_openpgp()
    if fileid == 0:
        data_in_device = icc.cmd_get_data(0x00, 0x4f)
        for d in data_in_device:
            print "%02x" % d,
        print
        compare(data, data_in_device[8:])
    elif fileid >= 1 and fileid <= 4:
        data_in_device = icc.cmd_read_binary(fileid)
        compare(data, data_in_device)
    else:
        data_in_device = icc.cmd_get_data(0x7f, 0x21)
        compare(data, data_in_device)
    icc.icc_power_off()
    return 0

if __name__ == '__main__':
    passwd = DEFAULT_PW3
    if sys.argv[1] == '-p':
        from getpass import getpass
        passwd = getpass("Admin password: ")
        sys.argv.pop(1)
    if sys.argv[1] == '-u':
        is_update = True
        sys.argv.pop(1)
    else:
        is_update = False
    if sys.argv[1] == '-s':
        fileid = 0              # serial number
        filename = sys.argv[2]
        f = open(filename)
        email = os.environ['EMAIL']
        serial_data_hex = None
        for line in f.readlines():
            field = string.split(line)
            if field[0] == email:
                serial_data_hex = field[1].replace(':','')
        f.close()
        if not serial_data_hex:
            print "No serial number"
            exit(1)
        print "Writing serial number"
        data = binascii.unhexlify(serial_data_hex)
    elif sys.argv[1] == '-k':   # firmware update key
        keyno = sys.argv[2]
        fileid = 1 + int(keyno)
        filename = sys.argv[3]
        f = open(filename)
        data = f.read()
        f.close()
    else:
        fileid = 5              # Card holder certificate
        filename = sys.argv[1]
        f = open(filename)
        data = f.read()
        f.close()
        print "%s: %d" % (filename, len(data))
        print "Updating card holder certificate"
    main(fileid, is_update, data, passwd)
