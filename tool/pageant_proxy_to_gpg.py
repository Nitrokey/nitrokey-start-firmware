"""
pagent_proxy_to_gpg.py - Connect gpg-agent as Pagent

Copyright (C) 2013 Free Software Initiative of Japan
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

import os, sys, re, hashlib, binascii
from struct import *
from gpg_agent import gpg_agent
from sexp import sexp

# Assume it's only OPENPGP.3 key and it's 2048-bit

def debug(string):
    print "DEBUG: %s" % string
    sys.stdout.flush()

def get_keygrip_list(keyinfo_result):
    kl_str = keyinfo_result[0:-1] # Chop last newline
    kl = kl_str.split('\n')
    # filter by "OPENPGP.3", and only keygrip
    return [kg.split(' ')[0] for kg in kl if re.search("OPENPGP\\.3", kg)]

# Connect GPG-Agent, and get list of KEYGRIPs.
g = gpg_agent()
g.read_line()                   # Greeting message

g.send_command('KEYINFO --list --data\n')
keyinfo_result = g.get_response()
keygrip_list = get_keygrip_list(keyinfo_result)

debug(keygrip_list)

keylist = []
# For each KEYGRIP, get its PUBLIC-KEY.
for kg in keygrip_list:
    g.send_command('READKEY %s\n' % kg)
    key = sexp(g.get_response())
    # [ "public-key" [ "rsa" [ "n" MODULUS ] [ "e" EXPONENT] ] ]
    n = key[1][1][1]
    e = key[1][2][1]
    debug(binascii.hexlify(n))
    debug(binascii.hexlify(e))
    keylist.append([n, e, kg])

# FIXME: should handle all keys, not only a single key
# FIXME: should support different key size
n = keylist[0][0]
e = keylist[0][1]
keygrip = keylist[0][2]

ssh_rsa_public_blob = "\x00\x00\x00\x07ssh-rsa" + \
    "\x00\x00\x00\x03" + e + "\x00\x00\x01\x01" + n

ssh_key_comment = "key_on_gpg"  # XXX: get login from card for comment?

import win32con, win32api, win32gui, ctypes, ctypes.wintypes


# For WM_COPYDATA structure
class COPYDATA(ctypes.Structure):
    _fields_ = [ ('dwData', ctypes.wintypes.LPARAM),
                 ('cbData', ctypes.wintypes.DWORD),
                 ('lpData', ctypes.c_void_p) ]

P_COPYDATA = ctypes.POINTER(COPYDATA)

class SSH_MSG_HEAD(ctypes.BigEndianStructure):
    _pack_ = 1
    _fields_ = [ ('msg_len', ctypes.c_uint32),
                 ('msg_type', ctypes.c_byte) ]

P_SSH_MSG_HEAD = ctypes.POINTER(SSH_MSG_HEAD)

class SSH_MSG_ID_ANSWER_HEAD(ctypes.BigEndianStructure):
    _pack_ = 1
    _fields_ = [ ('msg_len', ctypes.c_uint32),
                 ('msg_type', ctypes.c_byte),
                 ('keys', ctypes.c_uint32)]

P_SSH_MSG_ID_ANSWER = ctypes.POINTER(SSH_MSG_ID_ANSWER_HEAD)

class SSH_MSG_SIGN_RESPONSE_HEAD(ctypes.BigEndianStructure):
    _pack_ = 1
    _fields_ = [ ('msg_len', ctypes.c_uint32),
                 ('msg_type', ctypes.c_byte),
                 ('sig_len', ctypes.c_uint32)]

P_SSH_MSG_SIGN_RESPONSE = ctypes.POINTER(SSH_MSG_SIGN_RESPONSE_HEAD)


FILE_MAP_ALL_ACCESS=0x000F001F

class windows_ipc_listener(object):
    def __init__(self):
        message_map = { win32con.WM_COPYDATA: self.OnCopyData }
        wc = win32gui.WNDCLASS()
        wc.lpfnWndProc = message_map
        wc.lpszClassName = 'Pageant'
        hinst = wc.hInstance = win32api.GetModuleHandle(None)
        classAtom = win32gui.RegisterClass(wc)
        self.hwnd = win32gui.CreateWindow (
            classAtom,
            "Pageant",
            0,
            0, 
            0,
            win32con.CW_USEDEFAULT, 
            win32con.CW_USEDEFAULT,
            0, 
            0,
            hinst, 
            None
        )
        debug("created: window=%08x" % self.hwnd)

    def OnCopyData(self, hwnd, msg, wparam, lparam):
        debug("WM_COPYDATA message")
        debug("  window=%08x" % hwnd)
        debug("  msg   =%08x" % msg)
        debug("  wparam=%08x" % wparam)
        pCDS = ctypes.cast(lparam, P_COPYDATA)
        debug("  dwData=%08x" % (pCDS.contents.dwData & 0xffffffff))
        debug("  len=%d" % pCDS.contents.cbData)
        mapname = ctypes.string_at(pCDS.contents.lpData)
        debug("  mapname='%s'" % ctypes.string_at(pCDS.contents.lpData))
        hMapObject = ctypes.windll.kernel32.OpenFileMappingA(FILE_MAP_ALL_ACCESS, 0, mapname)
        if hMapObject == 0:
            debug("error on OpenFileMapping")
            return 0
        pBuf = ctypes.windll.kernel32.MapViewOfFile(hMapObject, FILE_MAP_ALL_ACCESS, 0, 0, 0)
        if pBuf == 0:
            ctypes.windll.kernel32.CloseHandle(hMapObject)
            debug("error on MapViewOfFile")
            return 0
        pSshMsg = ctypes.cast(pBuf, P_SSH_MSG_HEAD)
        debug("   ssh_msg_len: %d" % pSshMsg.contents.msg_len)
        debug("   ssh_msg_type: %d" % pSshMsg.contents.msg_type)
        if pSshMsg.contents.msg_type == 11: # SSH2_AGENT_REQUEST_IDENTITIES
            blob_len = len(ssh_rsa_public_blob)
            cmnt_len = len(ssh_key_comment)
            pAns = ctypes.cast(pBuf, P_SSH_MSG_ID_ANSWER)
            pAns.contents.msg_len = 1+4+4+blob_len+4+cmnt_len
            pAns.contents.msg_type = 12 # SSH2_AGENT_IDENTITIES_ANSWER
            pAns.contents.keys = 1
            ctypes.memmove(pBuf+4+1+4, pack('>I', blob_len), 4)
            ctypes.memmove(pBuf+4+1+4+4, ssh_rsa_public_blob, blob_len)
            ctypes.memmove(pBuf+4+1+4+4+blob_len, pack('>I', cmnt_len), 4)
            ctypes.memmove(pBuf+4+1+4+4+blob_len+4, ssh_key_comment, cmnt_len)

            debug("answer is:")
            debug("   ssh_msg_len: %d" % pSshMsg.contents.msg_len)
            debug("   ssh_msg_type: %d" % pSshMsg.contents.msg_type)
        elif pSshMsg.contents.msg_type == 13: # SSH2_AGENT_SIGN_REQUEST
            req_blob_len = unpack(">I", ctypes.string_at(pBuf+5, 4))[0]
            req_blob = ctypes.string_at(pBuf+5+4, req_blob_len)
            req_data_len = unpack(">I", ctypes.string_at(pBuf+5+4+req_blob_len,4))[0]
            req_data = ctypes.string_at(pBuf+5+4+req_blob_len+4,req_data_len)
            debug("    blob_len=%d" % req_blob_len)
            debug("    data_len=%d" % req_data_len)
            hash = hashlib.sha1(req_data).hexdigest()
            debug("    hash=%s" % hash)
            g.send_command('SIGKEY %s\n' % keygrip)
            g.send_command('SETHASH --hash=sha1 %s\n' % hash)
            g.send_command('PKSIGN\n')
            sig = sexp(g.get_response())
            # [ "sig-val" [ "rsa" [ "s" "xxx" ] ] ]
            sig = sig[1][1][1]
            sig = "\x00\x00\x00\x07" + "ssh-rsa" + "\x00\x00\x01\x00" + sig # FIXME: should support different key size
            siglen = len(sig)
            debug("sig_len=%d" % siglen)
            debug("sig=%s" % binascii.hexlify(sig))
            pRes = ctypes.cast(pBuf, P_SSH_MSG_SIGN_RESPONSE)
            pRes.contents.msg_len = 1+4+siglen
            pRes.contents.msg_type = 14 # SSH2_AGENT_SIGN_RESPONSE
            pRes.contents.sig_len = siglen
            ctypes.memmove(pBuf+4+1+4, sig, siglen)
            debug("answer is:")
            debug("   ssh_msg_len: %d" % pSshMsg.contents.msg_len)
            debug("   ssh_msg_type: %d" % pSshMsg.contents.msg_type)
        else:
            exit(0)
        ctypes.windll.kernel32.UnmapViewOfFile(pBuf)
        ctypes.windll.kernel32.CloseHandle(hMapObject)
        debug("   ssh_msg: done")
        return 1

l = windows_ipc_listener()
win32gui.PumpMessages()
