"""
gpg_agent.py - a library to connect gpg-agent

Copyright (C) 2013, 2015  Free Software Initiative of Japan
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

import platform, os, socket
IS_WINDOWS=(platform.system() == 'Windows')

BUFLEN=1024

class gpg_agent(object):
    def __init__(self):
        if IS_WINDOWS:
            home = os.getenv("HOME")
            if not home:
                home = os.getenv("APPDATA")
            comm_port = os.path.join(home, "gnupg", "S.gpg-agent")
            #
            f = open(comm_port, "rb", 0)
            infostr = f.read().decode('UTF-8')
            f.close()
            #
            info = infostr.split('\n', 1)
            port = int(info[0])
            nonce = info[1]
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect(("localhost", port))
            s.send(nonce)
        else:
            infostr = os.getenv("GPG_AGENT_INFO")
            info = infostr.split(':', 2)
            path = info[0]
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(path)
        self.sock = s
        self.buf_remained = b""
        self.response = None

    def read_line(self):
        line = b""
        if self.buf_remained != b"":
            chunk = self.buf_remained
        else:
            chunk = self.sock.recv(BUFLEN)
        while True:
            pos = chunk.find(b'\n')
            if pos >= 0:
                self.buf_remained = chunk[pos+1:]
                line = line + chunk[0:pos]
                return line
            else:
                line = line + chunk
                chunk = self.sock.recv(BUFLEN)

    def get_response(self):
        r = self.response
        result = b""
        while True:
            i = r.find(b'%')
            if i < 0:
                result += r
                break
            hex_str = r[i+1:i+3].decode('UTF-8')
            result += r[0:i]
            if bytes == str:
                result += chr(int(hex_str,16))
            else:
                result += bytes.fromhex(hex_str)
            r = r[i+3:]
        return result

    def send_command(self, cmd):
        self.sock.send(cmd.encode('UTF-8'))
        self.response = b""
        while True:
            while True:
                l = self.read_line()
                if l[0] != b'#'[0]:
                    break
            if l[0] == b'D'[0]:
                self.response += l[2:]
            elif l[0:2] == b'OK':
                return True
            elif l[0:3] == b'ERR':
                return False
            else:                    # XXX: S, INQUIRE, END
                return False

    def close(self):
        self.sock.send(b'BYE\n')
        bye = self.read_line()
        self.sock.close()
        return bye              # "OK closing connection"

# Test
if __name__ == '__main__':
    g = gpg_agent()
    print(g.read_line().decode('UTF-8'))
    print(g.send_command("KEYINFO --list --data\n"))
    kl_str = g.get_response().decode('UTF-8')
    kl_str = kl_str[0:-1]
    kl = kl_str.split('\n')
    import re
    kl_o3 = [kg for kg in kl if re.search("OPENPGP\\.3", kg)]
    print(kl_o3)
    kg = kl_o3[0].split(' ')[0]
    print(g.send_command("READKEY %s\n" % kg))
    r = g.get_response()
    import binascii
    print(binascii.hexlify(r).decode('UTF-8'))
    print(g.close().decode('UTF-8'))
