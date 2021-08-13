#!/usr/bin/env python3

import sys
from intelhex import IntelHex

start_cycle = 4000000
txn_interval = 15000
interbyte_interval = 5000
interwrite_interval = 30000

ihex = IntelHex("Blink.ino.hex")
cycle = start_cycle
for start, stop in ihex.segments():
    print("Start: {} Stop: {}".format(start, stop))
    addr = start
    for i in range(start, stop, 128):
        length = 128
        if i + length > stop:
            length = stop - i
        #print(i, addr, length)
        byte_boundary = length % 4
        print("{} {:02x} {:02x} {:02x} 00 0".format(cycle, ord('U'), addr & 0xFF, (addr & 0xFF00) >> 8))
        cycle += interbyte_interval
        print("{} 00 00 00 00 1".format(cycle))
        cycle += txn_interval
        print("{} {:02x} 00 {:02x} 00 0".format(cycle, ord('d'), length))
        cycle += txn_interval - interbyte_interval
        bc = 0
        for j in range(length):
            if bc == 0:
                cycle += interbyte_interval
                print("{}".format(cycle), end=" ")
            print("{:02x}".format(ihex[i + j]), end=" ")
            bc += 1
            if bc == 4:
                print("0")
                bc = 0
        # make sure we do complete 4 byte spi txn's
        if byte_boundary != 0:
            for l in range(byte_boundary):
                print("00", end="")
            print("0")
        cycle += txn_interval
        print("{} 00 00 00 00 1 REPEAT".format(cycle))
        addr += 64
        # need a long interval between flash write page
        cycle += interwrite_interval
