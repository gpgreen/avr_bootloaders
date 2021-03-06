# avr_bootloaders
Bootloaders for AVR

## atmega_spi_bootloader.c

This bootloader uses peripheral SPI to communicate. The SPI
transactions are in 4 byte units. The controlling MCU sends the 4-byte
sequences. The SCK frequency is only guaranteed to work at fosc/4 or
lower per the datasheet.

### Hello is anyone home?

```
MCU
0 -> ['0', _, _, _]
1 -> [_, _, _, _]
bootloader
0 <- [0, 0, 0, 0]
1 <- [0x14, '0', 0x10, 0]
```

### Leave programming mode

```
MCU
0 -> ['Q', _, _, _]
bootloader
0 <- [0, 0, 0, 0]
```

### Set address, little endian, EEPROM in bytes, FLASH in words

```
MCU
0 -> ['U', address_low, address_high, _]
bootloader
0 <- [0, 0, 0, 0]
```

### Write memory, length is big endian and in bytes

'E' in 4th byte of initial txn will write to eeprom, instead of flash

```
MCU
0 -> ['d', length_high, length_low, ('E' or !'E')] 
1 -> [b0, b1, b2, b3]  repeat until all bytes sent
n -> [bn, 0, 0, 0]  last transaction has zero's after actual data,
bootloader
0 <- [0, 0, 0, 0]
1 <- [0, 0, 0, 0]
n <- [0, 0, 0, 0]
```

### Read memory, length is big endian and in bytes

'E' in 4th byte of initial txn will read from eeprom, instead of flash

```
MCU
0 -> ['t', length_high, length_low, ('E' or !'E')]
1 -> [_, _, _, _]  repeat until all bytes sent
n -> [bn, 0, 0, 0]
bootloader
0 <- [0, 0, 0, 0]
1 <- [b0, b1, b2, b3]
n <- [bn, 0, 0, 0]  last transaction has zero's after actual data,
```
  
### Get device signature bytes

```
MCU
0 -> ['u', _, _, _]
1 -> [_, _, _, _]
bootloader
0 <- [0, 0, 0, 0]
1 <- ['u', SIG1, SIG2, SIG3]
```

## Power Monitor Bootloader

This bootloader is for the Power Monitor Hat. It is based on an
atmega328p tied via spi peripheral to a Raspberry Pi. The spi
transactions are signalled via the BUTTON pin when the power monitor
is ready to receive more data. The trigger to enter the bootloader is
via the MCU_RUNNING pin. If that pin is high, the bootloader will enter
spi peripheral mode, and new firmware can be loaded. If the pin is
low when the bootloader is entered (the default as there is a
pull-down), then the application code is started.

After the flash has been loaded, send the 'Leave Programming Code' spi
transaction. Then set the MCU_RUNNING pin low to reboot into the
application code. There is a delay of 100ms between capturing the
code and rebooting the mcu to allow the pin state change.
