"""
card_reader.py - a library for smartcard reader

Copyright (C) 2016  Free Software Initiative of Japan
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

import usb.core
from struct import pack
from usb.util import find_descriptor, claim_interface, get_string, \
    endpoint_type, endpoint_direction, \
    ENDPOINT_TYPE_BULK, ENDPOINT_OUT, ENDPOINT_IN

# USB class, subclass, protocol
CCID_CLASS = 0x0B
CCID_SUBCLASS = 0x00
CCID_PROTOCOL_0 = 0x00

def ccid_compose(msg_type, seq, slot=0, rsv=0, param=0, data=b""):
    return pack('<BiBBBH', msg_type, len(data), slot, seq, rsv, param) + data

IFSC=254

class CardReader(object):
    def __init__(self, dev):
        """
        __init__(dev) -> None
        Initialize the DEV of CCID.
        device: usb.core.Device object.
        """

        cfg = dev.get_active_configuration()
        intf = find_descriptor(cfg, bInterfaceClass=CCID_CLASS,
                               bInterfaceSubClass=CCID_SUBCLASS,
                               bInterfaceProtocol=CCID_PROTOCOL_0)
        if intf is None:
            raise ValueError("Not a CCID device")

        claim_interface(dev, intf)

        for ep in intf:
            if endpoint_type(ep.bmAttributes) == ENDPOINT_TYPE_BULK and \
               endpoint_direction(ep.bEndpointAddress) == ENDPOINT_OUT:
               self.__bulkout = ep.bEndpointAddress
            if endpoint_type(ep.bmAttributes) == ENDPOINT_TYPE_BULK and \
               endpoint_direction(ep.bEndpointAddress) == ENDPOINT_IN:
               self.__bulkin = ep.bEndpointAddress

        assert len(intf.extra_descriptors) == 54
        assert intf.extra_descriptors[1] == 33

        if (intf.extra_descriptors[42] & 0x02):
            # Short APDU level exchange
            self.__use_APDU = True
        elif (intf.extra_descriptors[42] & 0x04):
            # Short and extended APDU level exchange
            self.__use_APDU = True
        elif (intf.extra_descriptors[42] & 0x01):
            # TPDU level exchange
            self.__use_APDU = False
        else:
            raise ValueError("Unknown exchange level")

        # Check other bits???
        #       intf.extra_descriptors[40]
        #       intf.extra_descriptors[41]

        self.__dev = dev
        self.__timeout = 10000
        self.__seq = 0

    def get_string(self, num):
        return get_string(self.__dev, num)

    def increment_seq(self):
        self.__seq = (self.__seq + 1) & 0xff

    def reset_device(self):
        try:
            self.__dev.reset()
        except:
            pass

    def ccid_get_result(self):
        msg = self.__dev.read(self.__bulkin, 1024, self.__timeout)
        if len(msg) < 10:
            print(msg)
            raise ValueError("ccid_get_result")
        msg_type = msg[0]
        data_len = msg[1] + (msg[2]<<8) + (msg[3]<<16) + (msg[4]<<24)
        slot = msg[5]
        seq = msg[6]
        status = msg[7]
        error = msg[8]
        chain = msg[9]
        data = msg[10:]
        # XXX: check msg_type, data_len, slot, seq, error
        return (status, chain, data.tobytes())

    def ccid_get_status(self):
        msg = ccid_compose(0x65, self.__seq)
        self.__dev.write(self.__bulkout, msg, self.__timeout)
        self.increment_seq()
        status, chain, data = self.ccid_get_result()
        # XXX: check chain, data
        return status

    def ccid_power_on(self):
        msg = ccid_compose(0x62, self.__seq, rsv=1) # Vcc=5V
        self.__dev.write(self.__bulkout, msg, self.__timeout)
        self.increment_seq()
        status, chain, data = self.ccid_get_result()
        # XXX: check status, chain
        self.atr = data
        #
        if self.__use_APDU == False:
            # TPDU reader configuration
            pps = b"\xFF\x11\x18\xF6"
            ret_pps = self.ccid_send_data_block(pps)
            #
            param = b"\x18\x10\xFF\x75\x00\xFE\x00"
            # ^--- This shoud be adapted by ATR string, see update_param_by_atr
            msg = ccid_compose(0x6d, self.__seq, rsv=0x1, data=param)
            self.__dev.write(self.__bulkout, msg, self.__timeout)
            self.increment_seq()
            ret_param = self.ccid_get_result()
            # Send an S-block
            sblk = b"\x00\xC1\x01\xFE\x3E"
            ret_sblk = self.ccid_send_data_block(sblk)
        return self.atr

    def ccid_power_off(self):
        msg = ccid_compose(0x63, self.__seq)
        self.__dev.write(self.__bulkout, msg, self.__timeout)
        self.increment_seq()
        status, chain, data = self.ccid_get_result()
        # XXX: check chain, data
        return status

    def ccid_send_data_block(self, data):
        msg = ccid_compose(0x6f, self.__seq, data=data)
        self.__dev.write(self.__bulkout, msg, self.__timeout)
        self.increment_seq()
        return self.ccid_get_result()

    def ccid_send_cmd(self, data):
        status, chain, data_rcv = self.ccid_send_data_block(data)
        if chain == 0:
            while status == 0x80:
                status, chain, data_rcv = self.ccid_get_result()
            return data_rcv
        elif chain == 1:
            d = data_rcv
            while True:
                msg = ccid_compose(0x6f, self.__seq, param=0x10)
                self.__dev.write(self.__bulkout, msg, self.__timeout)
                self.increment_seq()
                status, chain, data_rcv = self.ccid_get_result()
                # XXX: check status
                d += data_rcv
                if chain == 2:
                    break
                elif chain == 3:
                    continue
                else:
                    raise ValueError("ccid_send_cmd chain")
            return d
        else:
            raise ValueError("ccid_send_cmd")

    def send_cmd(self, cmd):
        if self.__use_APDU:
            return self.ccid_send_cmd(cmd)
        # TPDU
        raise ValueError("TPDU not implemented yet")

class find_class(object):
    def __init__(self, usb_class):
        self.__class = usb_class
    def __call__(self, device):
        if device.bDeviceClass == self.__class:
            return True
        for cfg in device:
            intf = find_descriptor(cfg, bInterfaceClass=self.__class)
            if intf is not None:
                return True
        return False

def get_ccid_device():
    ccid = None
    dev_list = usb.core.find(find_all=True, custom_match=find_class(CCID_CLASS))
    for dev in dev_list:
        try:
            ccid = CardReader(dev)
            print("CCID device: Bus %03d Device %03d" % (dev.bus, dev.address))
            break
        except:
            pass
    if not ccid:
        raise ValueError("No CCID device present")
    status = ccid.ccid_get_status()
    if status == 0:
        # It's ON already
        atr = ccid.ccid_power_on()
    elif status == 1:
        atr = ccid.ccid_power_on()
    else:
        raise ValueError("Unknown CCID status", status)
    print(atr)
    return ccid
