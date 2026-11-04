import sys; sys.path.insert(0,'tools/ed/facts/checks/args')
from _r import *
x=room(153)
def hexd(a,n):
    for i in range(a,a+n,16):
        ln=x[i:i+16]; print('%06x'%i,ln.hex(' '),''.join(chr(c) if 32<=c<127 else '.' for c in ln))
hexd(0x530f4-128,256)
print()
hexd(0x2c5b2-80,160)
