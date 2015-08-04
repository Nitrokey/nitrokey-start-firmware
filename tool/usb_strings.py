#! /usr/bin/python

"""
usb_strings.py - a tool to dump USB string

Copyright (C) 2012, 2015 Free Software Initiative of Japan
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

import usb, sys

USB_VENDOR_FSIJ=0x234b
USB_PRODUCT_GNUK=0x0000

def gnuk_devices_by_vidpid():
    busses = usb.busses()
    for bus in busses:
        devices = bus.devices
        for dev in devices:
            if dev.idVendor != USB_VENDOR_FSIJ:
                continue
            if dev.idProduct != USB_PRODUCT_GNUK:
                continue
            yield dev

field = ['', 'Vendor', 'Product', 'Serial', 'Revision', 'Config', 'Sys', 'Board']

def main(n):
    for dev in gnuk_devices_by_vidpid():
        handle = dev.open()
        print("Device: %s" % dev.filename)
        try:
            for i in range(1,n):
                s = handle.getString(i, 512)
                print("%10s: %s" % (field[i], s.decode('UTF-8')))
        except:
            pass
        del dev

if __name__ == '__main__':
    if len(sys.argv) > 1:
        n = int(sys.argv[1])
    else:
        n = 8                   # Gnuk has eight strings
    main(n)
