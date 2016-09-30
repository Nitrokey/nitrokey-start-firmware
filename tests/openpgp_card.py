"""
openpgp_card.py - a library for OpenPGP card

Copyright (C) 2011, 2012, 2013, 2015, 2016
              Free Software Initiative of Japan
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

from struct import pack

def iso7816_compose(ins, p1, p2, data, cls=0x00, le=None):
    data_len = len(data)
    if data_len == 0:
        if not le:
            return pack('>BBBB', cls, ins, p1, p2)
        else:
            return pack('>BBBBB', cls, ins, p1, p2, le)
    else:
        if not le:
            return pack('>BBBBB', cls, ins, p1, p2, data_len) + data
        else:
            return pack('>BBBBB', cls, ins, p1, p2, data_len) \
                + data + pack('>B', le)

class OpenPGP_Card(object):
    def __init__(self, reader):
        """
        __init__(reader) -> None
        Initialize a OpenPGP card with a CardReader.
        reader: CardReader object.
        """

        self.__reader = reader

    def cmd_get_response(self, expected_len):
        result = b""
        while True:
            cmd_data = iso7816_compose(0xc0, 0x00, 0x00, b'') + pack('>B', expected_len)
            response = self.__reader.send_cmd(cmd_data)
            result += response[:-2]
            sw = response[-2:]
            if sw[0] == 0x90 and sw[1] == 0x00:
                return result
            elif sw[0] != 0x61:
                raise ValueError("%02x%02x" % (sw[0], sw[1]))
            else:
                expected_len = sw[1]

    def cmd_verify(self, who, passwd):
        cmd_data = iso7816_compose(0x20, 0x00, 0x80+who, passwd)
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_read_binary(self, fileid):
        cmd_data = iso7816_compose(0xb0, 0x80+fileid, 0x00, b'')
        sw = self.__reader.send_cmd(cmd_data)
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
            sw = self.__reader.send_cmd(cmd_data0)
            if len(sw) != 2:
                raise ValueError("cmd_write_binary 0")
            if not (sw[0] == 0x90 and sw[1] == 0x00):
                raise ValueError("cmd_write_binary 0", "%02x%02x" % (sw[0], sw[1]))
            if cmd_data1:
                sw = self.__reader.send_cmd(cmd_data1)
                if len(sw) != 2:
                    raise ValueError("cmd_write_binary 1", sw)
                if not (sw[0] == 0x90 and sw[1] == 0x00):
                    raise ValueError("cmd_write_binary 1", "%02x%02x" % (sw[0], sw[1]))
            count += 1

    def cmd_select_openpgp(self):
        cmd_data = iso7816_compose(0xa4, 0x04, 0x0c, b"\xD2\x76\x00\x01\x24\x01")
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_get_data(self, tagh, tagl):
        cmd_data = iso7816_compose(0xca, tagh, tagl, b"")
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if sw[0] == 0x90 and sw[1] == 0x00:
            return b""
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_change_reference_data(self, who, data):
        cmd_data = iso7816_compose(0x24, 0x00, 0x80+who, data)
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_put_data(self, tagh, tagl, content):
        cmd_data = iso7816_compose(0xda, tagh, tagl, content)
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_put_data_odd(self, tagh, tagl, content):
        cmd_data0 = iso7816_compose(0xdb, tagh, tagl, content[:128], 0x10)
        cmd_data1 = iso7816_compose(0xdb, tagh, tagl, content[128:])
        sw = self.__reader.send_cmd(cmd_data0)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        sw = self.__reader.send_cmd(cmd_data1)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_reset_retry_counter(self, how, data):
        cmd_data = iso7816_compose(0x2c, how, 0x00, data)
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return True

    def cmd_pso(self, p1, p2, data):
        cmd_data = iso7816_compose(0x2a, p1, p2, data)
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if sw[0] == 0x90 and sw[1] == 0x00:
            return b""
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_pso_longdata(self, p1, p2, data):
        cmd_data0 = iso7816_compose(0x2a, p1, p2, data[:128], 0x10)
        cmd_data1 = iso7816_compose(0x2a, p1, p2, data[128:])
        sw = self.__reader.send_cmd(cmd_data0)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        sw = self.__reader.send_cmd(cmd_data1)
        if len(sw) != 2:
            raise ValueError(sw)
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_internal_authenticate(self, data):
        cmd_data = iso7816_compose(0x88, 0, 0, data)
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if sw[0] == 0x90 and sw[1] == 0x00:
            return b""
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_genkey(self, keyno):
        if keyno == 1:
            data = b'\xb6\x00'
        elif keyno == 2:
            data = b'\xb8\x00'
        else:
            data = b'\xa4\x00'
        cmd_data = iso7816_compose(0x47, 0x80, 0, data)
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if sw[0] == 0x90 and sw[1] == 0x00:
            return b""
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        pk = self.cmd_get_response(sw[1])
        return (pk[9:9+256], pk[9+256+2:9+256+2+3])

    def cmd_get_public_key(self, keyno):
        if keyno == 1:
            data = b'\xb6\x00'
        elif keyno == 2:
            data = b'\xb8\x00'
        else:
            data = b'\xa4\x00'
        cmd_data = iso7816_compose(0x47, 0x81, 0, data)
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        elif sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        pk = self.cmd_get_response(sw[1])
        return (pk[9:9+256], pk[9+256+2:9+256+2+3])

    def cmd_put_data_remove(self, tagh, tagl):
        cmd_data = iso7816_compose(0xda, tagh, tagl, b"")
        sw = self.__reader.send_cmd(cmd_data)
        if sw[0] != 0x90 and sw[1] != 0x00:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))

    def cmd_put_data_key_import_remove(self, keyno):
        if keyno == 1:
            keyspec = b"\xb6\x00"      # SIG
        elif keyno == 2:
            keyspec = b"\xb8\x00"      # DEC
        else:
            keyspec = b"\xa4\x00"      # AUT
        cmd_data = iso7816_compose(0xdb, 0x3f, 0xff, b"\x4d\x02" +  keyspec)
        sw = self.__reader.send_cmd(cmd_data)
        if sw[0] != 0x90 and sw[1] != 0x00:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))

    def cmd_get_challenge(self):
        cmd_data = iso7816_compose(0x84, 0x00, 0x00, '')
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if sw[0] != 0x61:
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        return self.cmd_get_response(sw[1])

    def cmd_external_authenticate(self, keyno, signed):
        cmd_data = iso7816_compose(0x82, 0x00, keyno, signed[0:128], cls=0x10)
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
        cmd_data = iso7816_compose(0x82, 0x00, keyno, signed[128:])
        sw = self.__reader.send_cmd(cmd_data)
        if len(sw) != 2:
            raise ValueError(sw)
        if not (sw[0] == 0x90 and sw[1] == 0x00):
            raise ValueError("%02x%02x" % (sw[0], sw[1]))
