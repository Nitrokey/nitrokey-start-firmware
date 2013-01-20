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

    def send_command(self, cmd):
        self.sock.send(cmd)

    def get_response(self):
        response = ""
        while True:
            while True:
                l = self.read_line()
                if l[0] != '#':
                    break
            if l[0] == 'D':
                response += l[2:]
            elif l[0] == 'O' and l[1] == 'K':
                if response != "":
                    return response
                else:
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
g = gpg_agent()
print g.read_line()
g.send_command("KEYINFO --list --data\n")
print g.get_response()
g.send_command("READKEY 5D6C89682D07CCFC034AF508420BF2276D8018ED\n")
r = g.get_response()
import binascii
print binascii.hexlify(r)
print g.close()
