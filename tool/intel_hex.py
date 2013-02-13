"""
intel_hex.py - Intel Hex file reader.

Copyright (C) 2010 Free Software Initiative of Japan
Author: NIIBE Yutaka <gniibe@fsij.org>

You can use/distribute/modify/etc. this for any purpose.
"""

import binascii

class intel_hex(object):
    def __init__(self, filename):
        self.start_address = 0
        self.address = 0
        self.memory = {}
        self.lineno = 0
        file = open(filename, 'r')
        for line in file:
            self.lineno += 1
            if self.parse_line(line):
                break
        file.close()
        self.pack()

    def pack(self):
        memory = {}
        prev_addr = 0
        prev_data_len = 0
        for addr in sorted(self.memory.keys()):
            data = self.memory[addr]
            if addr == prev_addr + prev_data_len:
                memory[prev_addr] += data
                prev_data_len += len(data)
            else:
                memory[addr] = data
                prev_addr = addr
                prev_data_len = len(data)
	self.memory = memory

    def calc_checksum(self, byte_count, offset, type_code, data):
        s = byte_count
        s += (offset >> 8)
        s += offset & 0xff
        s += type_code
        for d in data:
            s += (ord(d) & 0xff)
        s &= 0xff
        if s != 0:
            s = 256 - s
        return s

    def add_data(self, count, offset, data):
        address = self.address + offset
        try:
            self.memory[address]
        except:
            pass
        else:
            raise ValueError, "data overwritten (%d)" % self.lineno
        self.memory[address] = data

    def parse_line(self, line):
        if line[0] != ':':
            raise ValueError, "invalid line (%d)" % self.lineno
        count = int(line[1:3], 16)
        offset = int(line[3:7], 16)
        type_code = int(line[7:9], 16)
        data = binascii.unhexlify(line[9:(9+count*2)])
        check_sum = int(line[(9+count*2):], 16)
        if check_sum != self.calc_checksum(count, offset, type_code, data):
            raise ValueError, "invalid checksum (%d)" % self.lineno
        if type_code == 0x00:
            self.add_data(count, offset, data)
            return 0
        elif type_code == 0x01:
            return 1
        elif type_code == 0x04:
            if count != 2:
                raise ValueError, "invalid count (%d): (%d) Expected 2" \
                    % (self.lineno, count)
            self.address = ((ord(data[0])&0xff)<<24) + ((ord(data[1])&0xff)<<16)
            return 0
        elif type_code == 0x05:
            if count != 4:
                raise ValueError, "invalid count (%d): (%d) Expected 4" \
                    % (self.lineno, count)
            self.start_address \
                = ((ord(data[0])&0xff)<<24) + ((ord(data[1])&0xff)<<16) \
                + ((ord(data[2])&0xff)<<8) + ((ord(data[3])&0xff))
            return 0
        else:
            raise ValueError, "invalid type code (%d): (%d)" \
                % (self.lineno, type_code)
