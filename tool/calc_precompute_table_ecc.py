from ecdsa import curves, ecdsa
G = ecdsa.generator_secp256k1
# G = ecdsa.generator_256

def print_nG(n):
   nG = n*G
   nGx_str = "%064x" % nG.x()
   nGy_str = "%064x" % nG.y()
   print256(nGx_str)
   print256(nGy_str)
   print

def print256(s):
   print("0x%s, 0x%s, 0x%s, 0x%s," % (s[56:64], s[48:56], s[40:48], s[32:40]))
   print("0x%s, 0x%s, 0x%s, 0x%s" %  (s[24:32], s[16:24], s[8:16], s[0:8]))
   print


for i in range(1,16):
  n = (i & 1) + (i & 2) * 0x8000000000000000L + (i & 4) * 0x40000000000000000000000000000000L + (i & 8) * 0x200000000000000000000000000000000000000000000000L
  print "%064x" % n
  print_nG(n)

for i in range(1,16):
  n = (i & 1) + (i & 2) * 0x8000000000000000L + (i & 4) * 0x40000000000000000000000000000000L + (i & 8) * 0x200000000000000000000000000000000000000000000000L
  n = n * 0x100000000L
  print "%064x" % n
  print_nG(n)
