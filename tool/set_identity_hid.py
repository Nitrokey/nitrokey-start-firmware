#! /usr/bin/python3

import hid, sys

"""
using hid library for this (with hidapi as backend) because it works
even when OS HID driver has claimed the device.
"""

vid=0x20a0
pid=0x4211

def usage():
    print("Usage: \npython3 set_identity_hid.py id")
    print("id must be 0, 1, or 2")

if __name__=="__main__":
    print("set_identity_hid.py selects identity on a nitrokey start")
    if len(sys.argv)!=2:
        print("missing identity number")
        usage();
        sys.exit(1)
    if not sys.argv[1].isdigit():
        print("identity number must be a digit")
        usage();
        sys.exit(1)
    identity=int(sys.argv[1])
    if(identity<0 or identity>2):
        print("identity must be 0, 1 or 2")
        usage();
        sys.exit(1)
    print("Trying to set identity to %d"%(identity,))
    try:
        print("Attempting to acquire HID device")
        h=hid.Device(vid,pid)
        try:
            print("Don't worry if you get a broken pipe error on the next line")
            h.send_feature_report(bytes([0x10+identity,0x00]))
            #We need the report length to be at least one byte, thus the 0x00
            #The 0x10+identity is the report ID - report IDs 0x10,0x11, and 0x12 set identity
        except:
            #this call will fail, but identity will still be set
            print("Device should now restart into new identity %d"%(identity))
            pass
    except:
        print("Could not get device")
