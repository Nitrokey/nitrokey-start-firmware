#! /usr/bin/python

"""
gnuk_update_binary.py - a tool to put binary to Gnuk Token
This tool is for importing certificate, updating random number, etc.

Copyright (C) 2011 Free Software Initiative of Japan
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
import sys, time, struct

# INPUT: binary file

# Assume only single CCID device is attached to computer, and it's Gnuk Token

import usb

# USB class, subclass, protocol
CCID_CLASS = 0x0B
CCID_SUBCLASS = 0x00
CCID_PROTOCOL_0 = 0x00

def icc_compose(msg_type, data_len, slot, seq, param, data):
    return pack('<BiBBBH', msg_type, data_len, slot, seq, 0, param) + data

def iso7816_compose(ins, p1, p2, data):
    cls = 0x00 
    data_len = len(data)
    if data_len == 0:
        return pack('>BBBB', cls, ins, p1, p2)
    else:
        return pack('>BBBBBh', cls, ins, p1, p2, 0, data_len) + data

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

        self.__bulkout = 2
        self.__bulkin  = 0x81

        self.__timeout = 10000
        self.__seq = 0


    def __del__(self):
        try:
            self.__devhandle.releaseInterface()
            del self.__devhandle
        except:
            pass

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

    def cmd_verify(self, who, passwd):
        cmd_data = iso7816_compose(0x20, 0x00, 0x80+who, passwd)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, "cmd_verify"
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError, "cmd_verify"

    def cmd_update_binary(self, fileid, data):
        count = 0
        data_len = len(data)
        while count*256 < data_len:
            if count == 0:
                cmd_data = iso7816_compose(0xd6, 0x80+fileid, 0x00, data[:256])
            else:
                cmd_data = iso7816_compose(0xd6, count, 0x00, data[256*count:256*(count+1)])
            sw = self.icc_send_cmd(cmd_data)
            if len(sw) != 2:
                raise ValueError, "cmd_update_binary"
            if not (sw[0] == 0x90 and sw[1] == 0x00):
                raise ValueError, "cmd_update_binary"
            count += 1

    def cmd_select_openpgp(self):
        cmd_data = iso7816_compose(0xa4, 0x04, 0x0c, "\xD2\x76\x00\x01\x24\x01")
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, "cmd_select_openpgp"
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError, "cmd_select_openpgp"

    def cmd_get_data(self, tagh, tagl):
        cmd_data = iso7816_compose(0xca, tagh, tagl, "")
        result = self.icc_send_cmd(cmd_data)
        sw = result[-2:]
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError, "cmd_get_data"
        return result[:-2]

def compare(data_original, data_in_device):
    i = 0 
    for d in data_original:
        if ord(d) != data_in_device[i]:
            raise ValueError, "verify failed at %08x" % i
        i += 1

def get_device():
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
                            return dev, config, alt
    raise ValueError, "Device not found"

def main(filename):
    f = open(filename)
    data = f.read()
    f.close()
    print "%s: %d" % (filename, len(data))
    data += "\x90\x00"
    dev, config, intf = get_device()
    print "Device: ", dev.filename
    print "Configuration: ", config.value
    print "Interface: ", intf.interfaceNumber
    icc = gnuk_token(dev, config, intf)
    if icc.icc_get_status() == 2:
        raise ValueError, "No ICC present"
    elif icc.icc_get_status() == 1:
        icc.icc_power_on()
    icc.cmd_verify(3, "12345678")
    icc.cmd_update_binary(0, data)
    icc.cmd_select_openpgp()
    data = data[:-2]
    data_in_device = icc.cmd_get_data(0x7f, 0x21)
    compare(data, data_in_device)
    icc.icc_power_off()
    return 0

if __name__ == '__main__':
    main(sys.argv[1])
