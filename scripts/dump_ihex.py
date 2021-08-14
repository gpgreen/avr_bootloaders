#!/usr/bin/env python3

import sys
from intelhex import IntelHex

ihex = IntelHex("Blink.ino.hex")
for start, stop in ihex.segments():
    print("Start: {} Stop: {}".format(start, stop))
    addr = start
    for i in range(start, stop, 128):
        length = 128
        if i + length > stop:
            length = stop - i
        #print(i, addr, length)
        byte_boundary = length % 4
        print("{:02x} {:02x} {:02x} 00 1".format(ord('U'), addr & 0xFF, (addr & 0xFF00) >> 8))
        print("{:02x} 00 {:02x} 00 1".format(ord('d'), length))
        bc = 0
        for j in range(length):
            print("{:02x}".format(ihex[i + j]), end=" ")
            bc += 1
            if bc == 4:
                print("1")
                bc = 0
        # make sure we do complete 4 byte spi txn's
        if byte_boundary != 0:
            for l in range(byte_boundary):
                print("00", end="")
            print("1")
        addr += 64
    # read the bytes back out
    addr = start
    for i in range(start, stop, 128):
        length = 128
        if i + length > stop:
            length = stop - i
        #print(i, addr, length)
        byte_boundary = length % 4
        print("{:02x} {:02x} {:02x} 00 1".format(ord('U'), addr & 0xFF, (addr & 0xFF00) >> 8))
        print("{:02x} 00 {:02x} 00 1".format(ord('t'), length))
        for j in range(length >> 2):
            print("00 00 00 00 1")
        addr += 64
