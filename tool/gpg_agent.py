import platform, os, socket
IS_WINDOWS=(platform.system() == 'Windows')

BUFLEN=1024

class gpg_agent(object):
    def __init__(self):
        if IS_WINDOWS:
            home = os.getenv("HOME")
            comm_port = os.path.join(home, "gnupg", "S.gpg-agent")
            #
            f = open(comm_port, "rb", 0)
            infostr = f.read()
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
        self.buf_remained = ""
        self.response = None

    def read_line(self):
        line = ""
        if self.buf_remained != "":
            chunk = self.buf_remained
        else:
            chunk = self.sock.recv(BUFLEN)
        while True:
            pos = chunk.index('\n')
            if pos >= 0:
                self.buf_remained = chunk[pos+1:]
                line = line + chunk[0:pos]
                return line
            else:
                line = line + chunk
                chunk = self.sock.recv(BUFLEN)

    def get_response(self):
        r = self.response
        result = ""
        while True:
            i = r.find('%')
            if i < 0:
                result += r
                break
            hex_str = r[i+1:i+3]
            result += r[0:i]
            result += chr(int(hex_str,16))
            r = r[i+3:]
        return result

    def send_command(self, cmd):
        self.sock.send(cmd)
        self.response = ""
        while True:
            while True:
                l = self.read_line()
                if l[0] != '#':
                    break
            if l[0] == 'D':
                self.response += l[2:]
            elif l[0] == 'O' and l[1] == 'K':
                return True
            elif l[0] == 'E' and l[1] == 'R' and l[2] == 'R':
                return False
            else:                    # XXX: S, INQUIRE, END
                return False

    def close(self):
        self.sock.send('BYE\n')
        bye = self.read_line()
        self.sock.close()
        return bye              # "OK closing connection"

# Test
if __name__ == '__main__':
    g = gpg_agent()
    print g.read_line()
    print g.send_command("KEYINFO --list --data\n")
    kl_str = g.get_response()
    kl_str = kl_str[0:-1]
    kl = kl_str.split('\n')
    import re
    kl_o3 = [kg for kg in kl if re.search("OPENPGP\\.3", kg)]
    print kl_o3
    kg = kl_o3[0].split(' ')[0]
    print g.send_command("READKEY %s\n" % kg)
    r = g.get_response()
    import binascii
    print binascii.hexlify(r)
    print g.close()
