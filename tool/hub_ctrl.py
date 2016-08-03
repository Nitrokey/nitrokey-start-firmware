#! /usr/bin/python

"""
hub_ctrl.py - a tool to control port power/led of USB hub

Copyright (C) 2006, 2011, 2016 Free Software Initiative of Japan

Author: NIIBE Yutaka  <gniibe@fsij.org>

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

from __future__ import print_function
import usb

USB_RT_HUB		=	(usb.TYPE_CLASS | usb.RECIP_DEVICE)
USB_RT_PORT		=	(usb.TYPE_CLASS | usb.RECIP_OTHER)
USB_PORT_FEAT_RESET 	=	4
USB_PORT_FEAT_POWER 	=	8
USB_PORT_FEAT_INDICATOR =       22
USB_DIR_IN		=	0x80		 # device to host

COMMAND_SET_NONE  = 0
COMMAND_SET_LED   = 1
COMMAND_SET_POWER = 2

HUB_LED_GREEN     = 2

def find_hubs(listing, verbose, busnum=None, devnum=None, hub=None):
    number_of_hubs_with_feature = 0
    hubs = []
    busses = usb.busses()
    if not busses:
        raise ValueError("can't access USB")

    for bus in busses:
        devices = bus.devices
        for dev in devices:
            if dev.deviceClass != usb.CLASS_HUB:
                continue

            printout_enable = 0
            if (listing
                or (verbose
                    and ((bus.dirname == busnum and dev.devnum == devnumd)
                         or hub == number_of_hubs_with_feature))):
                printout_enable = 1

            uh = dev.open()

            desc = None
            try:
                # Get USB Hub descriptor
                desc = uh.controlMsg(requestType = USB_DIR_IN | USB_RT_HUB, 
                                     request = usb.REQ_GET_DESCRIPTOR,
                                     value = usb.DT_HUB << 8,
                                     index = 0, buffer = 1024, timeout = 1000)
            finally:
                del uh

            if not desc:
                continue

            # desc[3] is lower byte of wHubCharacteristics
            if (desc[3] & 0x80) == 0 and (desc[3] & 0x03) >= 2:
                # Hub doesn't have features of controling port power/indicator
                continue

            if printout_enable:
                print("Hub #%d at %s:%03d" % (number_of_hubs_with_feature,
                                              bus.dirname, dev.devnum))
                if (desc[3] & 0x03) == 0:
                    print(" INFO: ganged power switching.")
                elif (desc[3] & 0x03) == 1:
                    print(" INFO: individual power switching.")
                elif (desc[3] & 0x03) == 2 or (desc[3] & 0x03) == 3:
                    print(" WARN: no power switching.")

                if (desc[3] & 0x80) == 0:
                    print(" WARN: Port indicators are NOT supported.")

            hubs.append({ 'busnum' : bus.dirname, 'devnum' : dev.devnum,
                          'indicator_support' : (desc[3] & 0x80) == 0x80,
                          'dev' : dev, 'num_ports' : desc[2] })
            number_of_hubs_with_feature += 1

    return hubs

def hub_port_status(handle, num_ports):
    print(" Hub Port Status:")
    for i in range(num_ports):
        port = i + 1
        status = handle.controlMsg(requestType = USB_RT_PORT | usb.ENDPOINT_IN, 
                                   request = usb.REQ_GET_STATUS,
                                   value = 0,
                                   index = port, buffer = 4,
                                   timeout = 1000)

        print("   Port %d: %02x%02x.%02x%02x" % (port, status[3], status[2],
                                                 status[1], status[0]),)
        if status[1] & 0x10:
            print(" indicator", end='')
        if status[1] & 0x08:
            print(" test" , end='')
        if status[1] & 0x04:
            print(" highspeed", end='')
        if status[1] & 0x02:
            print(" lowspeed", end='')
        if status[1] & 0x01:
            print(" power", end='')

        if status[0] & 0x10:
            print(" RESET", end='')
        if status[0] & 0x08:
            print(" oc", end='')
        if status[0] & 0x04:
            print(" suspend", end='')
        if status[0] & 0x02:
            print(" enable", end='')
        if status[0] & 0x01:
            print(" connect", end='')

        print()

import sys

COMMAND_SET_NONE  = 0
COMMAND_SET_LED   = 1
COMMAND_SET_POWER = 2
HUB_LED_GREEN  = 2

def usage(progname):
    print("""Usage: %s [{-h HUBNUM | -b BUSNUM -d DEVNUM}]
          [-P PORT] [{-p [VALUE]|-l [VALUE]}]
""" % progname, file=sys.stderr)

def exit_with_usage(progname):
    usage(progname)
    exit(1)

if __name__ == '__main__':
    busnum = None
    devnum = None
    listing = False
    verbose = False
    hub = None
    port = 1
    cmd = COMMAND_SET_NONE

    if len(sys.argv) == 1:
        listing = True
    else:
        try:
            while len(sys.argv) >= 2:
                option = sys.argv[1]
                sys.argv.pop(1)
                if option == '-h':
                    if busnum != None or devnum != None:
                        exit_with_usage(sys.argv[0])
                    hub = int(sys.argv[1])
                    sys.argv.pop(1)
                elif option == '-b':
                    busnum = int(sys.argv[1])
                    sys.argv.pop(1)
                elif option == '-d':
                    devnum = int(sys.argv[1])
                    sys.argv.pop(1)
                elif option == '-P':
                    port = int(sys.argv[1])
                    sys.argv.pop(1)
                elif option == '-l':
                    if cmd != COMMAND_SET_NONE:
                        exit_with_usage(sys.argv[0])
                    if len(sys.argv) > 1:
                        value = int(sys.argv[1])
                        sys.argv.pop(1)
                    else:
                        value = HUB_LED_GREEN
                    cmd = COMMAND_SET_LED
                elif option == '-p':
                    if cmd != COMMAND_SET_NONE:
                        exit_with_usage(sys.argv[0])
                    if len(sys.argv) > 1:
                        value = int(sys.argv[1])
                        sys.argv.pop(1)
                    else:
                        value = 0
                        cmd = COMMAND_SET_POWER
                elif option == '-v':
                    verbose = True
                    if len(sys.argv) == 1:
                        listing = True
                else:
                    exit_with_usage(sys.argv[0])
        except:
            exit_with_usage(sys.argv[0])

    if ((busnum != None and devnum == None)
        or (busnum == None and devnum != None)):
        exit_with_usage(sys.argv[0])

    if hub == None and busnum == None:
        hub = 0                 # Default hub = 0

    if cmd == COMMAND_SET_NONE:
        cmd = COMMAND_SET_POWER

    hubs = find_hubs(listing, verbose, busnum, devnum, hub)
    if len(hubs) == 0:
        print("No hubs found.", file=sys.stderr)
        exit(1)
    if listing:
        exit(0)

    if hub == None:
        for h in hubs:
            if h['busnum'] == busnum and h['devnum'] == devnum:
                dev_hub = h['dev']
                nports = h['num_ports']
    else:
        dev_hub = hubs[hub]['dev']
        nports = hubs[hub]['num_ports']

    uh = dev_hub.open()
    if cmd == COMMAND_SET_POWER:
        feature = USB_PORT_FEAT_POWER
        index = port
        if value:
            request = usb.REQ_SET_FEATURE
        else:
            request = usb.REQ_CLEAR_FEATURE
    else:
        request = usb.REQ_SET_FEATURE
        feature = USB_PORT_FEAT_INDICATOR
        index = (value << 8) | port
    if verbose:
        print("Send control message (REQUEST=%d, FEATURE=%d, INDEX=%d) " % (request, feature, index))

    uh.controlMsg(requestType = USB_RT_PORT, request = request, value = feature,
                  index = index, buffer = None, timeout = 1000)
    if verbose:
        hub_port_status(uh,nports)

    del uh
