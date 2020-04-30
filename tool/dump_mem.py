#! /usr/bin/python

"""
dump_mem.py - dump memory with DfuSe for STM32 Processor.

Copyright (C) 2010 Free Software Initiative of Japan
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

import sys
from dfuse import *

dev, config, intf = get_device()
dfu = DFU_STM32(dev, config, intf)
print(dfu.ll_get_string(intf.iInterface))
s = dfu.ll_get_status()
dfu.ll_clear_status()
s = dfu.ll_get_status()
print(s)
dfu.dfuse_set_address_pointer(int(sys.argv[1], 16))
s = dfu.ll_get_status()
dfu.ll_clear_status()
s = dfu.ll_get_status()
dfu.ll_clear_status()
s = dfu.ll_get_status()
print(s)
block = dfu.dfuse_read_memory()
count = 0
for d in block:
    print("%02x" % d)
    if count & 0x0f == 0x0f:
        print
    count += 1
dfu.ll_clear_status()
s = dfu.ll_get_status()
