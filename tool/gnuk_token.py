"""
gnuk_token.py - a library for Gnuk Token

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

from struct import *
import string
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

def list_to_string(l):
    return string.join([chr(c) for c in l], '')

# This class only supports Gnuk (for now) 
class gnuk_token(object):
    def __init__(self, device, configuration, interface):
        """
        __init__(device, configuration, interface) -> None
        Initialize the device.
        device: usb.Device object.
        configuration: configuration number.
        interface: usb.Interface object representing the interface and altenate setting.
        """
        if interface.interfaceClass != CCID_CLASS:
            raise ValueError("Wrong interface class")
        if interface.interfaceSubClass != CCID_SUBCLASS:
            raise ValueError("Wrong interface sub class")
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

    def get_string(self, num):
        return self.__devhandle.getString(num, 512)

    def increment_seq(self):
        self.__seq = (self.__seq + 1) & 0xff

    def reset_device(self):
        try:
            self.__devhandle.reset()
        except:
            pass

    def release_gnuk(self):
        self.__devhandle.releaseInterface()

    def icc_get_result(self):
        msg = self.__devhandle.bulkRead(self.__bulkin, 1024, self.__timeout)
        if len(msg) < 10:
            print msg
            raise ValueError("icc_get_result")
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
        self.increment_seq()
        status, chain, data = self.icc_get_result()
        # XXX: check chain, data
        return status

    def icc_power_on(self):
        msg = icc_compose(0x62, 0, 0, self.__seq, 0, "")
        self.__devhandle.bulkWrite(self.__bulkout, msg, self.__timeout)
        self.increment_seq()
        status, chain, data = self.icc_get_result()
        # XXX: check status, chain
        self.atr = list_to_string(data) # ATR
        return self.atr

    def icc_power_off(self):
        msg = icc_compose(0x63, 0, 0, self.__seq, 0, "")
        self.__devhandle.bulkWrite(self.__bulkout, msg, self.__timeout)
        self.increment_seq()
        status, chain, data = self.icc_get_result()
        # XXX: check chain, data
        return status

    def icc_send_data_block(self, data):
        msg = icc_compose(0x6f, len(data), 0, self.__seq, 0, data)
        self.__devhandle.bulkWrite(self.__bulkout, msg, self.__timeout)
        self.increment_seq()
        return self.icc_get_result()

    def icc_send_cmd(self, data):
        status, chain, data_rcv = self.icc_send_data_block(data)
        if chain == 0:
            while status == 0x80:
                status, chain, data_rcv = self.icc_get_result()
            return data_rcv
        elif chain == 1:
            d = data_rcv
            while True:
                msg = icc_compose(0x6f, 0, 0, self.__seq, 0x10, "")
                self.__devhandle.bulkWrite(self.__bulkout, msg, self.__timeout)
                self.increment_seq()
                status, chain, data_rcv = self.icc_get_result()
                # XXX: check status
                d += data_rcv
                if chain == 2:
                    break
                elif chain == 3:
                    continue
                else:
                    raise ValueError("icc_send_cmd chain")
            return d
        else:
            raise ValueError("icc_send_cmd")

    def cmd_get_response(self, expected_len):
        result = []
        while True:
            cmd_data = iso7816_compose(0xc0, 0x00, 0x00, '') + pack('>B', expected_len)
            response = self.icc_send_cmd(cmd_data)
            result += response[:-2]
            sw = response[-2:]
            if sw[0] == 0x90 and sw[1] == 0x00:
                return list_to_string(result)
            elif sw[0] != 0x61:
                raise ValueError("%02x%02x" % (sw[0], sw[1]))
            else:
                expected_len = sw[1]

    def cmd_verify(self, who, passwd):
        cmd_data = iso7816_compose(0x20, 0x00, 0x80+who, passwd)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_read_binary(self, fileid):
        cmd_data = iso7816_compose(0xb0, 0x80+fileid, 0x00, '')
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_write_binary(self, fileid, data, is_update):
        count = 0
        data_len = len(data)
        if is_update:
            ins = 0xd6
        else:
            ins = 0xd0
        while count*256 < data_len:
            if count == 0:
                if len(data) < 128:
                    cmd_data0 = iso7816_compose(ins, 0x80+fileid, 0x00, data[:128])
                    cmd_data1 = None
                else:
                    cmd_data0 = iso7816_compose(ins, 0x80+fileid, 0x00, data[:128], 0x10)
                    cmd_data1 = iso7816_compose(ins, 0x80+fileid, 0x00, data[128:256])
            else:
                if len(data[256*count:256*count+128]) < 128:
                    cmd_data0 = iso7816_compose(ins, count, 0x00, data[256*count:256*count+128])
                    cmd_data1 = None
                else:
                    cmd_data0 = iso7816_compose(ins, count, 0x00, data[256*count:256*count+128], 0x10)
                    cmd_data1 = iso7816_compose(ins, count, 0x00, data[256*count+128:256*(count+1)])
            sw = self.icc_send_cmd(cmd_data0)
            if len(sw) != 2:
                raise ValueError("cmd_write_binary 0")
            if not (sw[0] == 0x90 and sw[1] == 0x00):
                raise ValueError("cmd_write_binary 0", "%02x%02x" % (sw[0], sw[1]))
            if cmd_data1:
                sw = self.icc_send_cmd(cmd_data1)
                if len(sw) != 2:
                    raise ValueError("cmd_write_binary 1", sw)
                if not (sw[0] == 0x90 and sw[1] == 0x00):
                    raise ValueError("cmd_write_binary 1", "%02x%02x" % (sw[0], sw[1]))
            count += 1

    def cmd_select_openpgp(self):
        cmd_data = iso7816_compose(0xa4, 0x04, 0x0c, "\xD2\x76\x00\x01\x24\x01")
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, sw
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_get_data(self, tagh, tagl):
        cmd_data = iso7816_compose(0xca, tagh, tagl, "")
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, sw
        if sw[0] == 0x90 and sw[1] == 0x00:
            return ""
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_change_reference_data(self, who, data):
        cmd_data = iso7816_compose(0x24, 0x00, 0x80+who, data)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_put_data(self, tagh, tagl, content):
        cmd_data = iso7816_compose(0xda, tagh, tagl, content)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_put_data_odd(self, tagh, tagl, content):
        cmd_data0 = iso7816_compose(0xdb, tagh, tagl, content[:128], 0x10)
        cmd_data1 = iso7816_compose(0xdb, tagh, tagl, content[128:])
        sw = self.icc_send_cmd(cmd_data0)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        sw = self.icc_send_cmd(cmd_data1)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_reset_retry_counter(self, how, data):
        cmd_data = iso7816_compose(0x2c, how, 0x00, data)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_pso(self, p1, p2, data):
        cmd_data = iso7816_compose(0x2a, p1, p2, data)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if sw[0] == 0x90 and sw[1] == 0x00:
            return ""
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_pso_longdata(self, p1, p2, data):
        cmd_data0 = iso7816_compose(0x2a, p1, p2, data[:128], 0x10)
        cmd_data1 = iso7816_compose(0x2a, p1, p2, data[128:])
        sw = self.icc_send_cmd(cmd_data0)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        sw = self.icc_send_cmd(cmd_data1)
        if len(sw) != 2:
            raise ValueError(sw)
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_internal_authenticate(self, data):
        cmd_data = iso7816_compose(0x88, 0, 0, data)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if sw[0] == 0x90 and sw[1] == 0x00:
            return ""
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_genkey(self, keyno):
        if keyno == 1:
            data = '\xb6\x00'
        elif keyno == 2:
            data = '\xb8\x00'
        else:
            data = '\xa4\x00'
        cmd_data = iso7816_compose(0x47, 0x80, 0, data)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if sw[0] == 0x90 and sw[1] == 0x00:
            return ""
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        pk = self.cmd_get_response(sw[1])
        return (pk[9:9+256], pk[9+256+2:9+256+2+3])

    def cmd_get_public_key(self, keyno):
        if keyno == 1:
            data = '\xb6\x00'
        elif keyno == 2:
            data = '\xb8\x00'
        else:
            data = '\xa4\x00'
        cmd_data = iso7816_compose(0x47, 0x81, 0, data)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        pk = self.cmd_get_response(sw[1])
        return (pk[9:9+256], pk[9+256+2:9+256+2+3])

    def cmd_put_data_remove(self, tagh, tagl):
        cmd_data = iso7816_compose(0xda, tagh, tagl, "")
        sw = self.icc_send_cmd(cmd_data)
        if sw[0] != 0x90 and sw[1] != 0x00:
            raise ValueError, ("%02x%02x" % (sw[0], sw[1]))

    def cmd_put_data_key_import_remove(self, keyno):
        if keyno == 1:
            keyspec = "\xb6\x00"      # SIG
        elif keyno == 2:
            keyspec = "\xb8\x00"      # DEC
        else:
            keyspec = "\xa4\x00"      # AUT
        cmd_data = iso7816_compose(0xdb, 0x3f, 0xff, "\x4d\x02" +  keyspec)
        sw = self.icc_send_cmd(cmd_data)
        if sw[0] != 0x90 and sw[1] != 0x00:
            raise ValueError, ("%02x%02x" % (sw[0], sw[1]))

def compare(data_original, data_in_device):
    if data_original == data_in_device:
        return True
    raise ValueError("verify failed")

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

def get_gnuk_device():
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
    if not icc:
        raise ValueError("No ICC present")
    status = icc.icc_get_status()
    if status == 0:
        pass                    # It's ON already
    elif status == 1:
        icc.icc_power_on()
    else:
        raise ValueError("Unknown ICC status", status)
    return icc
