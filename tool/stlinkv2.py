#! /usr/bin/python

"""
stlinkv2.py - a tool to control ST-Link/V2

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

from struct import *
import sys, time

# INPUT: binary file

# Assumes only single ST-Link/V2 device is attached to computer.

import usb

GPIOA=0x40010800
OPTION_BYTES_ADDR=0x1ffff800
RDP_KEY=0x00a5                  # Unlock readprotection
FLASH_BASE_ADDR=0x40022000

FLASH_KEYR=    FLASH_BASE_ADDR+0x04
FLASH_OPTKEYR= FLASH_BASE_ADDR+0x08
FLASH_SR=      FLASH_BASE_ADDR+0x0c
FLASH_CR=      FLASH_BASE_ADDR+0x10
FLASH_AR=      FLASH_BASE_ADDR+0x14
FLASH_OBR=     FLASH_BASE_ADDR+0x1c

FLASH_KEY1=0x45670123
FLASH_KEY2=0xcdef89ab

FLASH_SR_BSY=      0x0001
FLASH_SR_PGERR=    0x0004
FLASH_SR_WRPRTERR= 0x0010
FLASH_SR_EOP=      0x0020

FLASH_CR_PG=     0x0001
FLASH_CR_PER=    0x0002
FLASH_CR_MER=    0x0004
FLASH_CR_OPTPG=  0x0010
FLASH_CR_OPTER=  0x0020
FLASH_CR_STRT=   0x0040
FLASH_CR_LOCK=   0x0080
FLASH_CR_OPTWRE= 0x0200


def uint32(v):
    return v[0] + (v[1]<<8) + (v[2]<<16) + (v[3]<<24)

## HERE comes: "movw r2,#SIZE" instruction
prog_flash_write_body = "\x0A\x48" + "\x0B\x49" + \
    "\x08\x4C" + "\x01\x25" + "\x14\x26" + "\x00\x27" + "\x25\x61" + \
    "\xC3\x5B" + "\xCB\x53" + "\xE3\x68" + "\x2B\x42" + "\xFC\xD1" + \
    "\x33\x42" + "\x02\xD1" + "\x02\x37" + "\x97\x42" + "\xF5\xD1" + \
    "\x00\x27" + "\x27\x61" + "\x00\xBE" +  "\x00\x20\x02\x40" + \
    "\x38\x00\x00\x20"
#   .SRC_ADDR: 0x20000038
## HERE comes: target_addr in 4-byte
#   .TARGET_ADDR

def gen_prog_flash_write(addr,size):
    return pack("<BBBB", (0x40 | (size&0xf000)>>12), (0xf2 | (size&0x0800)>>9),
                (size & 0x00ff), (0x02 | ((size&0x0700) >> 4))) + \
                prog_flash_write_body + pack("<I", addr)


## HERE comes: "movw r0,#VAL" instruction
prog_option_bytes_write_body = "\x06\x49" + "\x05\x4A" + "\x10\x23" + \
    "\x01\x24" + "\x13\x61" + "\x08\x80" + "\xD0\x68" + "\x20\x42" + \
    "\xFC\xD1" + "\x00\x20" + "\x10\x61" + "\x00\xBE" + "\x00\x20\x02\x40"
## HERE comes: target_addr in 4-byte
#   .TARGET_ADDR

def gen_prog_option_bytes_write(addr,val):
    return pack("<BBBB", (0x40 | (val&0xf000)>>12), (0xf2 | (val&0x0800)>>9),
                (val & 0x00ff), (0x00 | ((val&0x0700) >> 4))) + \
                prog_option_bytes_write_body + pack("<I", addr)


SRAM_ADDRESS=0x20000000
BLOCK_SIZE=16384                # Should be less than (20KiB - 0x0038)
BLOCK_WRITE_TIMEOUT=80          # Increase this when you increase BLOCK_SIZE

# This class only supports Gnuk (for now) 
class stlinkv2(object):
    def __init__(self, dev):
        self.__bulkout = 2
        self.__bulkin  = 0x81

        self.__timeout = 1000   # 1 second
        conf = dev.configurations[0]
        intf_alt = conf.interfaces[0]
        intf = intf_alt[0]
        if intf.interfaceClass != 0xff: # Vendor specific
            raise ValueError, "Wrong interface class"
        self.__devhandle = dev.open()
        try:
            self.__devhandle.setConfiguration(conf)
        except:
            pass
        self.__devhandle.claimInterface(intf)
        self.__devhandle.setAltInterface(intf)

    def execute_get(self, cmd, res_len):
        self.__devhandle.bulkWrite(self.__bulkout, cmd, self.__timeout)
        res = self.__devhandle.bulkRead(self.__bulkin, res_len, self.__timeout)
        return res

    def execute_put(self, cmd, data=None):
        self.__devhandle.bulkWrite(self.__bulkout, cmd, self.__timeout)
        if (data):
            self.__devhandle.bulkWrite(self.__bulkout, data, self.__timeout)

    def stl_mode(self):
        v = self.execute_get("\xf5\x00", 2)
        return (v[1] * 256 + v[0])

    def exit_from_dfu(self):
        self.__devhandle.bulkWrite(self.__bulkout, "\xf3\x07", self.__timeout)
        time.sleep(1)

    def enter_swd(self):
        self.__devhandle.bulkWrite(self.__bulkout, "\xf2\x20\xa3", self.__timeout)
        time.sleep(1)

    def get_status(self):
        v = self.execute_get("\xf2\x01\x00", 2)
        return (v[1] << 8) + v[0]
    # RUN:128, HALT:129

    def enter_debug(self):
        v = self.execute_get("\xf2\x02\x00", 2)
        return (v[1] << 8) + v[0]

    def exit_debug(self):
        self.execute_put("\xf2\x21\x00")

    def reset_sys(self):
        v = self.execute_get("\xf2\x03\x00", 2)
        return (v[1] << 8) + v[0]

    def read_memory(self, addr, length):
        return self.execute_get("\xf2\x07" + pack('<IH', addr, length), length)

    def read_memory_u32(self, addr):
        return uint32(self.execute_get("\xf2\x07" + pack('<IH', addr, 4), 4))

    def write_memory(self, addr, data):
        return self.execute_put("\xf2\x08" + pack('<IH', addr, len(data)), data)

    def write_memory_u32(self, addr, data):
        return self.execute_put("\xf2\x08" + pack('<IH', addr, 4),
                                pack('<I', data))

    def read_reg(self, regno):
        return uint32(self.execute_get("\xf2\x05" + pack('<B', regno), 4))

    def write_reg(self, regno, value):
        return self.execute_get("\xf2\x06" + pack('<BI', regno, value), 2)

    def run(self):
        v = self.execute_get("\xf2\x09\x00", 2)
        return (v[1] << 8) + v[0]

    def core_id(self):
        v = self.execute_get("\xf2\x22\x00", 4)
        return v[0] + (v[1]<<8) + (v[2]<<16) + (v[3]<<24)

    # For FST-01-00
    def setup_led(self):
        apb2enr = self.read_memory_u32(0x40021018)
        apb2enr = apb2enr | 4   # Enable port A
        self.write_memory_u32(0x40021018, apb2enr)
        self.write_memory_u32(0x4002100c, 4)
        self.write_memory_u32(0x4002100c, 0)
        self.write_memory_u32(GPIOA+0x0c, 0xffffffff) # ODR
        self.write_memory_u32(GPIOA+0x04, 0x88888883) # CRH

    # For FST-01-00
    def blink_led(self):
        self.write_memory_u32(GPIOA+0x0c, 0xfffffeff) # ODR
        time.sleep(0.5)
        self.write_memory_u32(GPIOA+0x0c, 0xffffffff) # ODR
        time.sleep(0.5)
        self.write_memory_u32(GPIOA+0x0c, 0xfffffeff) # ODR
        time.sleep(0.5)
        self.write_memory_u32(GPIOA+0x0c, 0xffffffff) # ODR

    def protection(self):
        return (self.read_memory_u32(FLASH_OBR) & 0x0002) != 0

    def option_bytes_read(self):
        return self.read_memory_u32(OPTION_BYTES_ADDR)

    def option_bytes_write(self,addr,val):
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY1)
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY2)
	self.write_memory_u32(FLASH_SR, FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR)

        self.write_memory_u32(FLASH_OPTKEYR, FLASH_KEY1)
        self.write_memory_u32(FLASH_OPTKEYR, FLASH_KEY2)

        prog = gen_prog_option_bytes_write(addr,val)
        self.write_memory(SRAM_ADDRESS, prog)
        self.write_reg(15, SRAM_ADDRESS)
        self.run()
        i = 0
        while self.get_status() == 0x80:
            time.sleep(0.050)
            i = i + 1
            if i >= 10:
                print "ERROR: option bytes write timeout"
                break

        status = self.read_memory_u32(FLASH_SR)
        self.write_memory_u32(FLASH_CR, FLASH_CR_LOCK)
        if (status & FLASH_SR_EOP) == 0:
            print "ERROR: option bytes write error"

    def option_bytes_erase(self):
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY1)
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY2)
	self.write_memory_u32(FLASH_SR, FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR)

        self.write_memory_u32(FLASH_OPTKEYR, FLASH_KEY1)
        self.write_memory_u32(FLASH_OPTKEYR, FLASH_KEY2)

	self.write_memory_u32(FLASH_CR, FLASH_CR_OPTER)
	self.write_memory_u32(FLASH_CR, FLASH_CR_STRT | FLASH_CR_OPTER)

        i = 0
        while True:
            status = self.read_memory_u32(FLASH_SR)
            if (status & FLASH_SR_BSY) == 0:
                break
            i = i + 1
            if i >= 1000:
                break

        self.write_memory_u32(FLASH_CR, FLASH_CR_LOCK)
	if (status & FLASH_SR_EOP) == 0:
            raise ValueError, "option bytes erase failed"

    def flash_write_internal(self, addr, data, off, size):
        prog = gen_prog_flash_write(addr,size)
        self.write_memory(SRAM_ADDRESS, prog+data[off:off+size])
        self.write_reg(15, SRAM_ADDRESS)
        self.run()
        i = 0
        while self.get_status() == 0x80:
            time.sleep(0.050)
            i = i + 1
            if i >= BLOCK_WRITE_TIMEOUT:
                print "ERROR: flash write timeout"
                break
        status = self.read_memory_u32(FLASH_SR)
        if (status & FLASH_SR_PGERR) != 0:
            print "ERROR: write to a location that was not erased"
        if (status & FLASH_SR_WRPRTERR) != 0:
            print "ERROR: write to a location that was write protected"

    def flash_write(self, addr, data):
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY1)
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY2)
	self.write_memory_u32(FLASH_SR, FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR)

        off = 0
        while True:
            if len(data[off:]) > BLOCK_SIZE:
                size = BLOCK_SIZE
                self.flash_write_internal(addr, data, off, size)
                off = off + size
                addr = addr + size
            else:
                size = len(data[off:])
                self.flash_write_internal(addr, data, off, size)
                break

        self.write_memory_u32(FLASH_CR, FLASH_CR_LOCK)

    def flash_erase_all(self):
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY1)
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY2)
	self.write_memory_u32(FLASH_SR, FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR)

	self.write_memory_u32(FLASH_CR, FLASH_CR_MER)
	self.write_memory_u32(FLASH_CR, FLASH_CR_STRT | FLASH_CR_MER)

        i = 0
        while True:
            status = self.read_memory_u32(FLASH_SR)
            if (status & FLASH_SR_BSY) == 0:
                break
            i = i + 1
            time.sleep(0.050)
            if i >= 100:
                break

        self.write_memory_u32(FLASH_CR, FLASH_CR_LOCK)

	if (status & FLASH_SR_EOP) == 0:
            raise ValueError, "flash erase all failed"

    def flash_erase_page(self, addr):
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY1)
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY2)

	self.write_memory_u32(FLASH_SR, FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR)

	self.write_memory_u32(FLASH_CR, FLASH_CR_PER)
	self.write_memory_u32(FLASH_AR, addr)
	self.write_memory_u32(FLASH_CR, FLASH_CR_STRT | FLASH_CR_PER)

        i = 0
        while True:
            status = self.read_memory_u32(FLASH_SR)
            if (status & FLASH_SR_BSY) == 0:
                break
            i = i + 1
            if i >= 1000:
                break

        self.write_memory_u32(FLASH_CR, FLASH_CR_LOCK)

	if (status & FLASH_SR_EOP) == 0:
            raise ValueError, "flash page erase failed"

    def start(self):
        mode = self.stl_mode()
        if mode == 2:
            return
        elif mode != 1:
            self.exit_from_dfu()
            mode = self.stl_mode()
            print "Change mode to: %04x" % mode
        self.enter_swd()
        s = self.get_status()
        print "Status: %04x" % s
        if self.stl_mode() != 2:
            raise ValueError, "Failed to switch debug mode"


USB_VENDOR_ST=0x0483            # 0x0483 SGS Thomson Microelectronics
USB_VENDOR_STLINKV2=0x3748      # 0x3748 ST-LINK/V2

def stlinkv2_devices():
    busses = usb.busses()
    for bus in busses:
        devices = bus.devices
        for dev in devices:
            if dev.idVendor != USB_VENDOR_ST:
                continue
            if dev.idProduct != USB_VENDOR_STLINKV2:
                continue
            yield dev

def compare(data_original, data_in_device):
    i = 0 
    for d in data_original:
        if ord(d) != data_in_device[i]:
            raise ValueError, "verify failed at %08x" % i
        i += 1

if __name__ == '__main__':
    erase = False
    if sys.argv[1] == '-e':
        erase = True
        sys.argv.pop(1)

    no_protect = False
    if sys.argv[1] == '-n':
        no_protect = True
        sys.argv.pop(1)

    status_only = False
    unlock = False
    if sys.argv[1] == '-u':
        unlock = True
        sys.argv.pop(1)
    elif sys.argv[1] == '-s':
        status_only = True
    else:
        filename = sys.argv[1]
        f = open(filename)
        data = f.read()
        f.close()

    for d in stlinkv2_devices():
        try:
            stl = stlinkv2(d)
            print "Device: ", d.filename
            break
        except:
            pass
    stl.start()
    core_id = stl.core_id()
    chip_id = stl.read_memory_u32(0xE0042000)
    # FST-01 chip id: 0x20036410
    print "CORE: %08x, CHIP_ID: %08x" % (core_id, chip_id)
    print "Flash ROM read protection:",
    protection = stl.protection()
    if protection:
        print "ON"
        if status_only:
            print "The MCU is now stopped.  No way to run by STLink/V2.  Please reset the board"
            exit (0)
        elif not unlock:
            print "Please unlock flash ROM protection, at first.  By invoking with -u option"
            exit(1)
    else:
        print "off"
        if status_only:
            stl.reset_sys()
            stl.run()
            stl.exit_debug()
            exit (0)
        elif unlock:
            print "No need to unlock.  Protection is not enabled."
            exit(1)

    print "status: %02x" % stl.get_status()
    stl.enter_debug()

    if unlock:
        stl.reset_sys()
        stl.option_bytes_write(OPTION_BYTES_ADDR,RDP_KEY)
        print "Flash ROM read protection disabled.  Reset the board, now."
        exit(0)

    print "option bytes: %08x" % stl.option_bytes_read()

    if erase:
        print "ERASE ALL"
        stl.reset_sys()
        stl.flash_erase_all()
        time.sleep(0.100)

    print "WRITE"
    stl.flash_write(0x08000000, data)

    print "VERIFY"
    data_received = ()
    size = len(data)
    off = 0
    while size > 0:
        if size > 1024:
            blk_size = 1024
        else:
            blk_size = size
        data_received = data_received + stl.read_memory(0x08000000+off, 1024)
        size = size - blk_size
        off = off + blk_size
    compare(data, data_received)

    if not no_protect:
        print "PROTECT"
        stl.option_bytes_erase()
        print "Flash ROM read protection enabled.  Reset the board to enable protection."
