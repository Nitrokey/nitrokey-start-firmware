#! /usr/bin/python

import sys
from dfuse import *

dev, config, intf = get_device()
dfu = DFU_STM32(dev, config, intf)
print dfu.ll_get_string(intf.iInterface)
s = dfu.ll_get_status()
dfu.ll_clear_status()
s = dfu.ll_get_status()
print s
dfu.dfuse_set_address_pointer(int(sys.argv[1], 16))
s = dfu.ll_get_status()
dfu.ll_clear_status()
s = dfu.ll_get_status()
dfu.ll_clear_status()
s = dfu.ll_get_status()
print s
block = dfu.dfuse_read_memory()
count = 0
for d in block:
    print "%02x" % d,
    if count & 0x0f == 0x0f:
        print
    count += 1
dfu.ll_clear_status()
s = dfu.ll_get_status()
