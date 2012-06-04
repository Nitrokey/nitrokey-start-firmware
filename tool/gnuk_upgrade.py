#! /usr/bin/python

"""
gnuk_upgrade.py - a tool to upgrade firmware of Gnuk Token

Copyright (C) 2012 Free Software Initiative of Japan
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

class regnual:
    def __init__(self, dev):
        conf = dev.configurations[0]
        intf_alt = conf.interfaces[0]
        intf = intf_alt[0]
        if intf.interfaceClass != 0xff:
            raise ValueError, "Wrong interface class"
        self.__devhandle = dev.open()
        try:
            self.__devhandle.setConfiguration(conf)
        except:
            pass
        self.__devhandle.claimInterface(intf)
        self.__devhandle.setAltInterface(intf)

    def mem_info(self):
        mem = self.__devhandle.controlMsg(requestType = 0xc0, request = 0,
                                          value = 0, index = 0, buffer = 8,
                                          timeout = 10000)
        start = ((mem[3]*256 + mem[2])*256 + mem[1])*256 + mem[0]
        end = ((mem[7]*256 + mem[6])*256 + mem[5])*256 + mem[4]
        return (start, end)

    def download(self, start, data):
        addr = start
        addr_end = (start + len(data)) & 0xffffff00
        i = (addr - 0x08000000) / 0x100
        j = 0
        print "start %08x" % addr
        print "end   %08x" % addr_end
        while addr < addr_end:
            print "# %08x: %d: %d : %d" % (addr, i, j, 256)
            self.__devhandle.controlMsg(requestType = 0x40, request = 1,
                                        value = 0, index = 0,
                                        buffer = data[j*256:j*256+256],
                                        timeout = 10000)
            crc32code = crc32(data[j*256:j*256+256], 0xffffffff)
            res = self.__devhandle.controlMsg(requestType = 0xc0, request = 2,
                                              value = 0, index = 0, buffer = 4,
                                              timeout = 10000)
            r_value = ((res[3]*256 + res[2])*256 + res[1])*256 + res[0]
            if (crc32code ^ r_value) != 0xffffffff:
                print "failure"
            self.__devhandle.controlMsg(requestType = 0x40, request = 3,
                                        value = i, index = 0,
                                        buffer = None,
                                        timeout = 10000)
            time.sleep(0.010)
            res = self.__devhandle.controlMsg(requestType = 0xc0, request = 2,
                                              value = 0, index = 0, buffer = 4,
                                              timeout = 10000)
            r_value = ((res[3]*256 + res[2])*256 + res[1])*256 + res[0]
            if r_value == 0:
                print "failure"
            i = i+1
            j = j+1
            addr = addr + 256
        residue = len(data) % 256
        if residue != 0:
            print "# %08x: %d : %d" % (addr, i, residue)
            self.__devhandle.controlMsg(requestType = 0x40, request = 1,
                                        value = 0, index = 0,
                                        buffer = data[j*256:],
                                        timeout = 10000)
            crc32code = crc32(data[j*256:].ljust(256,chr(255)), 0xffffffff)
            res = self.__devhandle.controlMsg(requestType = 0xc0, request = 2,
                                              value = 0, index = 0, buffer = 4,
                                              timeout = 10000)
            r_value = ((res[3]*256 + res[2])*256 + res[1])*256 + res[0]
            if (crc32code ^ r_value) != 0xffffffff:
                print "failure"
            self.__devhandle.controlMsg(requestType = 0x40, request = 3,
                                        value = i, index = 0,
                                        buffer = None,
                                        timeout = 10000)
            time.sleep(0.010)
            res = self.__devhandle.controlMsg(requestType = 0xc0, request = 2,
                                              value = 0, index = 0, buffer = 4,
                                              timeout = 10000)
            r_value = ((res[3]*256 + res[2])*256 + res[1])*256 + res[0]
            if r_value == 0:
                print "failure"

    def protect(self):
        self.__devhandle.controlMsg(requestType = 0x40, request = 4,
                                    value = 0, index = 0, buffer = None,
                                    timeout = 10000)
        time.sleep(0.100)
        res = self.__devhandle.controlMsg(requestType = 0xc0, request = 2,
                                          value = 0, index = 0, buffer = 4,
                                          timeout = 10000)
        r_value = ((res[3]*256 + res[2])*256 + res[1])*256 + res[0]
        if r_value == 0:
            print "protection failure"

    def finish(self):
        self.__devhandle.controlMsg(requestType = 0x40, request = 5,
                                    value = 0, index = 0, buffer = None,
                                    timeout = 10000)

    def reset_device(self):
        try:
            self.__devhandle.reset()
        except:
            pass

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

    def reset_device(self):
        try:
            self.__devhandle.reset()
        except:
            pass

    def stop_gnuk(self):
        self.__devhandle.releaseInterface()
        self.__devhandle.setConfiguration(0)
        return

    def mem_info(self):
        mem = self.__devhandle.controlMsg(requestType = 0xc0, request = 0,
                                          value = 0, index = 0, buffer = 8,
                                          timeout = 10)
        start = ((mem[3]*256 + mem[2])*256 + mem[1])*256 + mem[0]
        end = ((mem[7]*256 + mem[6])*256 + mem[5])*256 + mem[4]
        return (start, end)

    def download(self, start, data):
        addr = start
        addr_end = (start + len(data)) & 0xffffff00
        i = (addr - 0x20000000) / 0x100
        j = 0
        print "start %08x" % addr
        print "end   %08x" % addr_end
        while addr < addr_end:
            print "# %08x: %d : %d" % (addr, i, 256)
            self.__devhandle.controlMsg(requestType = 0x40, request = 1,
                                        value = i, index = 0,
                                        buffer = data[j*256:j*256+256],
                                        timeout = 10)
            i = i+1
            j = j+1
            addr = addr + 256
        residue = len(data) % 256
        if residue != 0:
            print "# %08x: %d : %d" % (addr, i, residue)
            self.__devhandle.controlMsg(requestType = 0x40, request = 1,
                                        value = i, index = 0,
                                        buffer = data[j*256:],
                                        timeout = 10)

    def execute(self, last_addr):
        i = (last_addr - 0x20000000) / 0x100
        o = (last_addr - 0x20000000) % 0x100
        self.__devhandle.controlMsg(requestType = 0x40, request = 2,
                                    value = i, index = o, buffer = None,
                                    timeout = 10)

    def icc_get_result(self):
        msg = self.__devhandle.bulkRead(self.__bulkin, 1024, self.__timeout)
        if len(msg) < 10:
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

    def cmd_select_openpgp(self):
        cmd_data = iso7816_compose(0xa4, 0x04, 0x0c, "\xD2\x76\x00\x01\x24\x01")
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, sw
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError, ("%02x%02x" % (sw[0], sw[1]))

    def cmd_external_authenticate(self, signed):
        cmd_data = iso7816_compose(0x82, 0x00, 0x00, signed[0:128], cls=0x10)
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, sw
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError, ("%02x%02x" % (sw[0], sw[1]))
        cmd_data = iso7816_compose(0x82, 0x00, 0x00, signed[128:])
        sw = self.icc_send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError, sw
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError, ("%02x%02x" % (sw[0], sw[1]))

    def cmd_get_challenge(self):
        cmd_data = iso7816_compose(0x84, 0x00, 0x00, '')
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

def ccid_devices():
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

USB_VENDOR_FSIJ=0x234b
USB_PRODUCT_GNUK=0x0000

def gnuk_devices():
    busses = usb.busses()
    for bus in busses:
        devices = bus.devices
        for dev in devices:
            if dev.idVendor != USB_VENDOR_FSIJ:
                continue
            if dev.idProduct != USB_PRODUCT_GNUK:
                continue
            yield dev

def to_string(t):
    result = ""
    for c in t:
        result += chr(c)
    return result

from subprocess import check_output

def gpg_sign(keygrip, hash):
    result = check_output(["gpg-connect-agent",
                           "SIGKEY %s" % keygrip,
                           "SETHASH --hash=sha1 %s" % hash,
                           "PKSIGN", "/bye"])
    signed = ""
    while True:
        i = result.find('%')
        if i < 0:
            signed += result
            break
        hex_str = result[i+1:i+3]
        signed += result[0:i]
        signed += chr(int(hex_str,16))
        result = result[i+3:]

    pos = signed.index("D (7:sig-val(3:rsa(1:s256:") + 26
    signed = signed[pos:-7]
    if len(signed) != 256:
        raise ValueError, binascii.hexlify(signed)
    return signed

crctab = [ 0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc,
           0x17c56b6b, 0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f,
           0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a,
           0x384fbdbd, 0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
           0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8,
           0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
           0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e,
           0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
           0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84,
           0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d, 0xd4326d90, 0xd0f37027,
           0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022,
           0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
           0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077,
           0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c,
           0x2e003dc5, 0x2ac12072, 0x128e9dcf, 0x164f8078, 0x1b0ca6a1,
           0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
           0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb,
           0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
           0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d,
           0x40d816ba, 0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
           0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f,
           0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044,
           0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050, 0xe9362689,
           0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
           0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683,
           0xd1799b34, 0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59,
           0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c,
           0x774bb0eb, 0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
           0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53, 0x251d3b9e,
           0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
           0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48,
           0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
           0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2,
           0xe6ea3d65, 0xeba91bbc, 0xef68060b, 0xd727bbb6, 0xd3e6a601,
           0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604,
           0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
           0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6,
           0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad,
           0x81b02d74, 0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7,
           0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
           0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd,
           0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
           0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b,
           0x0fdc1bec, 0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
           0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654, 0xc5a92679,
           0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12,
           0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676, 0xea23f0af,
           0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
           0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5,
           0x9e7d9662, 0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06,
           0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03,
           0xb1f740b4 ]

def UNSIGNED(n):
    return n & 0xffffffff

# (1) Alas, zlib.crc32 (== binascii.crc32) uses same polynomial, but in a
#     different way of endian-ness.
# (2) POSIX cksum command uses same calculation, but the initial value
#     is different.
def crc32(bytestr, crc):
    for b in bytestr:
        idx = (crc>>24)^ord(b)
        crc = UNSIGNED((crc << 8)) ^ crctab[idx]
    return UNSIGNED(~crc)

def main(keygrip, data_regnual, data_upgrade):
    l = len(data_regnual)
    if (l & 0x03) != 0:
        data_regnual = data_regnual.ljust(l + 4 - (l & 0x03), chr(0))
    crc32code = crc32(data_regnual, 0xffffffff)
    print "CRC32: %04x\n" % crc32code
    data_regnual += pack('<I', crc32code)
    for (dev, config, intf) in ccid_devices():
        try:
            icc = gnuk_token(dev, config, intf)
            print "Device: ", dev.filename
            print "Configuration: ", config.value
            print "Interface: ", intf.interfaceNumber
            break
        except:
            icc = None
    if icc.icc_get_status() == 2:
        raise ValueError, "No ICC present"
    elif icc.icc_get_status() == 1:
        icc.icc_power_on()
    icc.cmd_select_openpgp()
    challenge = icc.cmd_get_challenge()
    signed = gpg_sign(keygrip, binascii.hexlify(to_string(challenge)))
    icc.cmd_external_authenticate(signed)
    icc.stop_gnuk()
    mem_info = icc.mem_info()
    print "%08x:%08x" % mem_info
    print "Downloading flash upgrade program..."
    icc.download(mem_info[0], data_regnual)
    print "Run flash upgrade program..."
    icc.execute(mem_info[0] + len(data_regnual) - 4)
    #
    time.sleep(3)
    icc.reset_device()
    del icc
    icc = None
    #
    print "Wait 3 seconds..."
    time.sleep(3)
    # Then, send upgrade program...
    reg = None
    for dev in gnuk_devices():
        try:
            reg = regnual(dev)
            print "Device: ", dev.filename
            break
        except:
            pass
    mem_info = reg.mem_info()
    print "%08x:%08x" % mem_info
    print "Downloading the program"
    reg.download(mem_info[0], data_upgrade)
    reg.protect()
    reg.finish()
    reg.reset_device()
    return 0


if __name__ == '__main__':
    keygrip = sys.argv[1]
    filename_regnual = sys.argv[2]
    filename_upgrade = sys.argv[3]
    f = open(filename_regnual)
    data_regnual = f.read()
    f.close()
    print "%s: %d" % (filename_regnual, len(data_regnual))
    f = open(filename_upgrade)
    data_upgrade = f.read()
    f.close()
    print "%s: %d" % (filename_upgrade, len(data_upgrade))
    main(keygrip, data_regnual, data_upgrade[4096:])
