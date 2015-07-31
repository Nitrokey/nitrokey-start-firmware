#! /usr/bin/python

"""
stlinkv2.py - a tool to control ST-Link/V2

Copyright (C) 2012, 2013, 2015 Free Software Initiative of Japan
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
import usb
from colorama import init as colorama_init, Fore, Back, Style
from array import array

# INPUT: binary file

# Assumes only single ST-Link/V2 device is attached to computer.

CORE_ID_CORTEX_M3=0x1ba01477
CORE_ID_CORTEX_M0=0x0bb11477

CHIP_ID_STM32F103xB=0x20036410
CHIP_ID_STM32F103xE=0x10016414
# CHIP_ID_STM32F0   0x20006440
# CHIP_ID_STM32F030??  0x10006444; FSM-55

GPIOA=0x40010800
GPIOB=0x40010C00
OPTION_BYTES_ADDR=0x1ffff800
RDP_KEY_F1=0x00a5                  # Unlock readprotection
RDP_KEY_F0=0x00aa                  # Unlock readprotection
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

SPI1= 0x40013000

def uint32(v):
    return v[0] + (v[1]<<8) + (v[2]<<16) + (v[3]<<24)

prog_flash_write_body = b"\x0D\x4A\x0B\x48\x0B\x49\x09\x4C\x01\x25\x14\x26\x00\x27\x25\x61" + \
    b"\xC3\x5B\xCB\x53\xE3\x68\x2B\x42\xFC\xD1\x33\x42\x02\xD1\x02\x37\x97\x42\xF5\xD1" + \
    b"\x00\x27\x27\x61\x00\xBE\xC0\x46\x00\x20\x02\x40\x3C\x00\x00\x20"
#   .SRC_ADDR: 0x2000003C
## HERE comes: target_addr in 4-byte
#   .TARGET_ADDR
## HERE comes: size in 4-byte
#   .SIZE

def gen_prog_flash_write(addr,size):
    return prog_flash_write_body + pack("<I", addr) + pack("<I", size)

prog_option_bytes_write_body = b"\x0B\x48\x0A\x49\x08\x4A\x10\x23\x01\x24\x13\x61\x08\x80\xD0\x68" + \
     b"\x20\x42\xFC\xD1\x02\x31\xFF\x20\x08\x80\xD0\x68\x20\x42\xFC\xD1\x00\x20\x10\x61" + \
     b"\x00\xBE\xC0\x46\x00\x20\x02\x40"
## HERE comes: target_addr in 4-byte
#   .TARGET_ADDR
## HERE comes: option_bytes in 4-byte
#   .OPTION_BYTES

def gen_prog_option_bytes_write(addr,val):
    return prog_option_bytes_write_body + pack("<I", addr) + pack("<I", val)

prog_blank_check_body = b"\x04\x49\x05\x4A\x08\x68\x01\x30\x02\xD1\x04\x31\x91\x42\xF9\xD1\x00\xBE\xC0\x46" + \
                        b"\x00\x00\x00\x08"
## HERE comes: end_addr in 4-byte
# .END_ADDR

def gen_prog_blank_check(size):
    return prog_blank_check_body + pack("<I", 0x08000000 + size)


SRAM_ADDRESS=0x20000000
FLASH_BLOCK_SIZE_F1=16384       # Should be less than (20KiB - 0x0038)
FLASH_BLOCK_SIZE_F0=2048        # Should be less than (4KiB - 0x0038)
BLOCK_WRITE_TIMEOUT=80          # Increase this when you increase BLOCK_SIZE


class TimeOutError(Exception):
    def __init__(self, msg):
        self.msg = msg
    def __str__(self):
        return repr(self.msg)
    def __repr__(self):
        return "TimeoutError(" + self.msg + ")"

class OperationFailure(Exception):
    def __init__(self, msg):
        self.msg = msg
    def __str__(self):
        return repr(self.msg)
    def __repr__(self):
        return "OperationFailure(" + self.msg + ")"


class stlinkv2(object):
    def __init__(self, dev):
        if dev.idProduct == USB_VENDOR_STLINKV2:
            self.__bulkout = 2
        else:
            self.__bulkout = 1
        self.__bulkin  = 0x81

        self.__timeout = 1000   # 1 second
        conf = dev.configurations[0]
        intf_alt = conf.interfaces[0]
        intf = intf_alt[0]
        if intf.interfaceClass != 0xff: # Vendor specific
            raise ValueError("Wrong interface class.", intf.interfaceClass)
        self.__devhandle = dev.open()
        # ST-Link/V2-1 has other interfaces
        # Some other processes or kernel would use it
        # So, write access to configuration causes error
        try:
            self.__devhandle.setConfiguration(conf.value)
        except:
            pass
        self.__devhandle.claimInterface(intf.interfaceNumber)
        # self.__devhandle.setAltInterface(0)  # This was not good for libusb-win32 with wrong arg intf, new correct value 0 would be OK

    def shutdown(self):
        self.__devhandle.releaseInterface()

    def execute_get(self, cmd, res_len):
        self.__devhandle.bulkWrite(self.__bulkout, cmd, self.__timeout)
        res = self.__devhandle.bulkRead(self.__bulkin, res_len, self.__timeout)
        return res

    def execute_put(self, cmd, data=None):
        self.__devhandle.bulkWrite(self.__bulkout, cmd, self.__timeout)
        if (data):
            self.__devhandle.bulkWrite(self.__bulkout, data, self.__timeout)

    def stl_mode(self):
        v = self.execute_get(b"\xf5\x00", 2)
        return (v[1] * 256 + v[0])

    def exit_from_debug_swd(self):
        self.execute_put(b"\xf2\x21\x00")
        time.sleep(1)

    def exit_from_dfu(self):
        self.execute_put(b"\xf3\x07\x00")
        time.sleep(1)

    def exit_from_debug_swim(self):
        self.execute_put(b"\xf4\x01\x00")
        time.sleep(1)

    def enter_swd(self):
        self.execute_put(b"\xf2\x20\xa3")
        time.sleep(1)

    def get_status(self):
        v = self.execute_get(b"\xf2\x01\x00", 2)
        return (v[1] << 8) + v[0]
    # RUN:128, HALT:129

    def enter_debug(self):
        v = self.execute_get(b"\xf2\x02\x00", 2)
        return (v[1] << 8) + v[0]

    def exit_debug(self):
        self.execute_put(b"\xf2\x21\x00")

    def reset_sys(self):
        v = self.execute_get(b"\xf2\x03\x00", 2)
        return (v[1] << 8) + v[0]

    def read_memory(self, addr, length):
        return self.execute_get(b"\xf2\x07" + pack('<IH', addr, length), length)

    def read_memory_u32(self, addr):
        return uint32(self.execute_get(b"\xf2\x07" + pack('<IH', addr, 4), 4))

    def write_memory(self, addr, data):
        return self.execute_put(b"\xf2\x08" + pack('<IH', addr, len(data)), data)

    def write_memory_u32(self, addr, data):
        return self.execute_put(b"\xf2\x08" + pack('<IH', addr, 4),
                                pack('<I', data))

    def read_reg(self, regno):
        return uint32(self.execute_get(b"\xf2\x05" + pack('<B', regno), 4))

    def write_reg(self, regno, value):
        return self.execute_get(b"\xf2\x06" + pack('<BI', regno, value), 2)

    def write_debug_reg(self, addr, value):
        return self.execute_get(b"\xf2\x35" + pack('<II', addr, value), 2)

    def control_nrst(self, value):
        return self.execute_get(b"\xf2\x3c" + pack('<B', value), 2)

    def run(self):
        v = self.execute_get(b"\xf2\x09\x00", 2)
        return (v[1] << 8) + v[0]

    def get_core_id(self):
        v = self.execute_get(b"\xf2\x22\x00", 4)
        return v[0] + (v[1]<<8) + (v[2]<<16) + (v[3]<<24)

    def version(self):
        v = self.execute_get(b"\xf1", 6)
        val = (v[0] << 8) + v[1]
        return ((val >> 12) & 0x0f, (val >> 6) & 0x3f, val & 0x3f)

    # For FST-01-00 and FST-01: LED on, USB connect
    def setup_gpio(self):
        apb2enr = self.read_memory_u32(0x40021018)
        apb2enr = apb2enr | 4 | 8 | 0x1000 # Enable port A, port B, and SPI1
        self.write_memory_u32(0x40021018, apb2enr)    # RCC->APB2ENR
        self.write_memory_u32(0x4002100c, 4|8|0x1000) # RCC->APB2RSTR
        self.write_memory_u32(0x4002100c, 0)
        self.write_memory_u32(GPIOA+0x0c, 0xffffffff) # ODR
        self.write_memory_u32(GPIOA+0x04, 0x88888383) # CRH
        self.write_memory_u32(GPIOA+0x00, 0xBBB38888) # CRL
        self.write_memory_u32(GPIOB+0x0c, 0xffffffff) # ODR
        self.write_memory_u32(GPIOB+0x04, 0x88888888) # CRH
        self.write_memory_u32(GPIOB+0x00, 0x88888883) # CRL

    # For FST-01-00 and FST-01: LED on, USB disconnect
    def usb_disconnect(self):
        self.write_memory_u32(GPIOA+0x0c, 0xfffffbff) # ODR

    # For FST-01-00 and FST-01: LED off, USB connect
    def finish_gpio(self):
        self.write_memory_u32(GPIOA+0x0c, 0xfffffeff) # ODR
        self.write_memory_u32(GPIOB+0x0c, 0xfffffffe) # ODR
        apb2enr = self.read_memory_u32(0x40021018)
        apb2enr = apb2enr &  ~(4 | 8 | 0x1000)
        self.write_memory_u32(0x40021018, apb2enr)    # RCC->APB2ENR

    def spi_flash_init(self):
        self.write_memory_u32(SPI1+0x00, 0x0004); # CR1 <= MSTR
        i2scfgr = self.read_memory_u32(SPI1+0x1c) # I2SCFGR
        i2scfgr = i2scfgr & 0xf7ff                # 
        self.write_memory_u32(SPI1+0x1c, i2scfgr); # I2SCFGR <= SPI mode
        self.write_memory_u32(SPI1+0x10, 7);       # CRCPR <= 7
        self.write_memory_u32(SPI1+0x04, 0x04);    # CR2 <= SSOE
        self.write_memory_u32(SPI1+0x00, 0x0044);  # CR1 <= MSTR | SPE

    def spi_flash_select(self, enable):
        if enable:
            self.write_memory_u32(GPIOA+0x0c, 0xffffffef) # ODR
        else:
            self.write_memory_u32(GPIOA+0x0c, 0xffffffff) # ODR

    def spi_flash_sendbyte(self, v):
        i = 0
        while True:
            status = self.read_memory_u32(SPI1+0x08) # SR
            if status & 0x02 != 0:                   # TXE (Data Empty)
                break
            time.sleep(0.01)
            i = i + 1
            if i > 10:
                raise TimeOutError('spi_flash_sendbyte')
        self.write_memory_u32(SPI1+0x0c, v) # DR
        i = 0
        while True:
            status = self.read_memory_u32(SPI1+0x08) # SR
            if status & 0x01 != 0:                   # RXNE (Data Not Empty)
                break
            time.sleep(0.01)
            i = i + 1
            if i > 10:
                raise TimeOutError('spi_flash_sendbyte')
        v = self.read_memory_u32(SPI1+0x0c) # DR
        return v

    def spi_flash_read_id(self):
        self.spi_flash_select(True)
        self.spi_flash_sendbyte(0x9f)
        t0 = self.spi_flash_sendbyte(0xa5)
        t1 = self.spi_flash_sendbyte(0xa5)
        t2 = self.spi_flash_sendbyte(0xa5)
        self.spi_flash_select(False)
        return (t0 << 16) | (t1 << 8) | t2

    def protection(self):
        return (self.read_memory_u32(FLASH_OBR) & 0x0002) != 0

    def blank_check(self):
        prog = gen_prog_blank_check(self.flash_size)
        self.write_memory(SRAM_ADDRESS, prog)
        self.write_reg(15, SRAM_ADDRESS)
        self.run()
        i = 0
        while self.get_status() == 0x80:
            time.sleep(0.050)
            i = i + 1
            if i >= 10:
                raise TimeOutError("blank check")

        r0_value = self.read_reg(0)
        return r0_value == 0

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
                raise TimeOutError("option bytes write")

        status = self.read_memory_u32(FLASH_SR)
        self.write_memory_u32(FLASH_CR, FLASH_CR_LOCK)
        if (status & FLASH_SR_EOP) == 0:
            raise OperationFailure("option bytes write")

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
            raise OperationFailure("option bytes erase")

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
                raise TimeOutError("flash write")
        status = self.read_memory_u32(FLASH_SR)
        if (status & FLASH_SR_PGERR) != 0:
            raise OperationFailure("flash write: write to not erased part")
        if (status & FLASH_SR_WRPRTERR) != 0:
            raise OperationFailure("flash write: write to protected part")

    def flash_write(self, addr, data):
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY1)
        self.write_memory_u32(FLASH_KEYR, FLASH_KEY2)
        self.write_memory_u32(FLASH_SR, FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR)

        off = 0
        while True:
            if len(data[off:]) > self.flash_block_size:
                size = self.flash_block_size
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
            raise OperationFailure("flash erase all")

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
            raise OperationFailure("flash page erase")

    def start(self):
        mode = self.stl_mode()
        if mode == 2:
            self.exit_from_debug_swd()
        elif mode == 5:
            self.exit_from_debug_swim()
        elif mode != 1 and mode != 4:
            self.exit_from_dfu()
        new_mode = self.stl_mode()
        print("Change ST-Link/V2 mode %04x -> %04x" % (mode, new_mode))
        self.control_nrst(2)
        self.enter_swd()
        s = self.get_status()
        if s != 0x0080:
            print("Status is %04x" % s)
            self.run()
            s = self.get_status()
            if s != 0x0080:
                #                   DCB_DHCSR    DBGKEY
                self.write_debug_reg(0xE000EDF0, 0xA05F0000)
                s = self.get_status()
                if s != 0x0080:
                    raise ValueError("Status of core is not running.", s)
        mode = self.stl_mode()
        if mode != 2:
            raise ValueError("Failed to switch debug mode.", mode)

        self.core_id = self.get_core_id()
        if self.core_id == CORE_ID_CORTEX_M3:
            self.chip_stm32 = True
        elif self.core_id == CORE_ID_CORTEX_M0:
            self.chip_stm32 = False
        else:
            raise ValueError("Unknown core ID", self.core_id)
        if self.chip_stm32:
            self.rdp_key = RDP_KEY_F1
            self.flash_block_size = FLASH_BLOCK_SIZE_F1
            self.require_nrst = False
            self.external_spi_flash = True
            self.protection_feature = True
        else:
            self.rdp_key = RDP_KEY_F0
            self.flash_block_size = FLASH_BLOCK_SIZE_F0
            self.require_nrst = True
            self.external_spi_flash = False
            self.protection_feature = False
        return self.core_id

    def get_chip_id(self):
        if self.chip_stm32:
            self.chip_id = self.read_memory_u32(0xE0042000)
            self.flash_size = (self.read_memory_u32(0x1ffff7e0) & 0xffff)*1024
            if self.chip_id == CHIP_ID_STM32F103xB:
                pass
            elif self.chip_id == CHIP_ID_STM32F103xE:
                pass
            else:
                raise ValueError("Unknown chip ID", self.chip_id)
        else:
            self.chip_id = self.read_memory_u32(0x40015800)
            self.flash_size = (self.read_memory_u32(0x1ffff7cc) & 0xffff)*1024
        print("Flash size: %dKiB" % (self.flash_size/1024))
        return self.chip_id

    def get_rdp_key(self):
        return self.rdp_key

    def has_spi_flash(self):
        return self.external_spi_flash

    def has_protection(self):
        return self.protection_feature


USB_VENDOR_ST=0x0483            # 0x0483 SGS Thomson Microelectronics
USB_VENDOR_STLINKV2=0x3748      # 0x3748 ST-LINK/V2
USB_VENDOR_STLINKV2_1=0x374b    # 0x374b ST-LINK/V2_1

def stlinkv2_devices():
    busses = usb.busses()
    for bus in busses:
        devices = bus.devices
        for dev in devices:
            if dev.idVendor != USB_VENDOR_ST:
                continue
            if dev.idProduct != USB_VENDOR_STLINKV2 and dev.idProduct != USB_VENDOR_STLINKV2_1:
                continue
            yield dev

def compare(data_original, data_in_device):
    i = 0 
    for d in data_original:
        if d != data_in_device[i]:
            raise ValueError("Verify failed at:", i)
        i += 1

def open_stlinkv2():
    for d in stlinkv2_devices():
        try:
            stl = stlinkv2(d)
            return stl
        except:
            pass
    return None

def help():
    print("stlinkv2.py [-h]: Show this help message")
    print("stlinkv2.py [-e]: Erase flash ROM")
    print("stlinkv2.py [-u]: Unlock flash ROM")
    print("stlinkv2.py [-s]: Show status")
    print("stlinkv2.py [-b] [-n] [-r] [-i] FILE: Write content of FILE to flash ROM")
    print("    -b: Blank check before write (auto erase when not blank)")
    print("    -n: Don't enable read protection after write")
    print("    -r: Don't reset after write")
    print("    -i: Don't test SPI flash")


def main(show_help, erase_only, no_protect, spi_flash_check,
         reset_after_successful_write,
         skip_blank_check, status_only, unlock, data):
    if show_help or len(sys.argv) != 1:
        help()
        return 1

    stl = open_stlinkv2()
    if not stl:
        raise ValueError("No ST-Link/V2 device found.", None)

    print("ST-Link/V2 version info: %d %d %d" % stl.version())
    core_id = stl.start()

    stl.control_nrst(2)
    stl.enter_debug()
    status = stl.get_status()
    if status != 0x0081:
        print("Core does not halt, try API V2 halt.")
        #                   DCB_DHCSR    DBGKEY|C_HALT|C_DEBUGEN
        stl.write_debug_reg(0xE000EDF0, 0xA05F0003)
        status = stl.get_status()
        stl.write_debug_reg(0xE000EDF0, 0xA05F0003)
        status = stl.get_status()
        if status != 0x0081:
            raise ValueError("Status of core is not halt.", status)

    chip_id = stl.get_chip_id()

    # FST-01 chip id: 0x20036410
    print("CORE: %08x, CHIP_ID: %08x" % (core_id, chip_id))
    protection = stl.protection()
    print("Flash ROM read protection: " + ("ON" if protection else "off"))
    option_bytes = stl.option_bytes_read()
    print("Option bytes: %08x" % option_bytes)
    rdp_key = stl.get_rdp_key()
    if (option_bytes & 0xff) == rdp_key:
        ob_protection_enable = False
    else:
        ob_protection_enable = True

    if protection:
        if status_only:
            print("The MCU is now stopped.")
            return 0
        elif not unlock:
            raise OperationFailure("Flash ROM is protected")
    else:
        if not skip_blank_check:
            stl.reset_sys()
            blank = stl.blank_check()
            print("Flash ROM blank check: %s" % blank)
        else:
            blank = True
        if status_only:
            stl.reset_sys()
            stl.run()
            stl.exit_debug()
            return 0
        elif unlock and not ob_protection_enable:
            print("No need to unlock.  Protection is not enabled.")
            return 1

    if erase_only:
        if blank:
            print("No need to erase")
            return 0

    stl.setup_gpio()

    if unlock:
        if option_bytes != 0xff:
            stl.reset_sys()
            stl.option_bytes_erase()
        stl.reset_sys()
        stl.option_bytes_write(OPTION_BYTES_ADDR,rdp_key)
        stl.usb_disconnect()
        time.sleep(0.100)
        stl.finish_gpio()
        print("Flash ROM read protection disabled.  Reset the board, now.")
        return 0

    if spi_flash_check and stl.has_spi_flash():
        stl.spi_flash_init()
        id = stl.spi_flash_read_id()
        print("SPI Flash ROM ID: %06x" % id)
        if id != 0xbf254a:
            raise ValueError("bad spi flash ROM ID")

    if not blank:
        print("ERASE ALL")
        stl.reset_sys()
        stl.flash_erase_all()

    if erase_only:
        stl.usb_disconnect()
        time.sleep(0.100)
        stl.finish_gpio()
        return 0

    time.sleep(0.100)

    print("WRITE")
    stl.flash_write(0x08000000, data)

    print("VERIFY")
    data_received = array('B')
    size = len(data)
    off = 0
    while size > 0:
        if size > 1024:
            blk_size = 1024
        else:
            blk_size = size
        data_received = data_received + array('B', stl.read_memory(0x08000000+off, blk_size))
        size = size - blk_size
        off = off + blk_size
    compare(array('B', data), data_received)

    if not no_protect and stl.has_protection():
        print("PROTECT")
        stl.option_bytes_erase()
        print("Flash ROM read protection enabled.  Reset the board to enable protection.")

    if reset_after_successful_write:
        stl.control_nrst(2)
        stl.usb_disconnect()
        stl.reset_sys()
        stl.run()
        stl.exit_debug()
    else:
        stl.finish_gpio()

    stl.shutdown()
    return 0

if __name__ == '__main__':
    show_help = False
    erase_only = False
    no_protect = False
    reset_after_successful_write = True
    skip_blank_check=True
    status_only = False
    unlock = False
    data = None
    spi_flash_check = True

    while len(sys.argv) > 1:
        if sys.argv[1] == '-h':
            sys.argv.pop(1)
            show_help = True
            break
        elif sys.argv[1] == '-e':
            sys.argv.pop(1)
            erase_only = True
            skip_blank_check=False
            break
        elif sys.argv[1] == '-u':
            sys.argv.pop(1)
            unlock = True
            break
        elif sys.argv[1] == '-s':
            sys.argv.pop(1)
            status_only = True
            skip_blank_check=False
            break
        elif sys.argv[1] == '-b':
            skip_blank_check=False
        elif sys.argv[1] == '-n':
            no_protect = True
        elif sys.argv[1] == '-r':
            reset_after_successful_write = False
        elif sys.argv[1] == '-i':
            spi_flash_check = False
        else:
            filename = sys.argv[1]
            f = open(filename,'rb')
            data = f.read()
            f.close()
            if len(data) % 1:
                raise ValueError("The size of file should be even")
        sys.argv.pop(1)

    colorama_init()

    try:
        r = main(show_help, erase_only, no_protect, spi_flash_check,
                 reset_after_successful_write,
                 skip_blank_check, status_only, unlock, data)
        if r == 0:
            print(Fore.WHITE + Back.BLUE + Style.BRIGHT + "SUCCESS" + Style.RESET_ALL)
        sys.exit(r)
    except Exception as e:
        print(Back.RED + Style.BRIGHT + repr(e) + Style.RESET_ALL)
