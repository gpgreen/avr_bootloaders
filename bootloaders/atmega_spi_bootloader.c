/**********************************************************/
/* SPI Bootloader for Atmel megaAVR Controllers           */
/* Copyright (c) 2021, Greg Green                         */
/*                                                        */
/* atmega_spi_bootloader.c                                */
/*                                                        */
/* ------------------------------------------------------ */
/*                                                        */
/* based on ATmegaBOOT_168.c                              */
/* Serial Bootloader for Atmel megaAVR Controllers        */
/*                                                        */
/* tested with ATmega8, ATmega128 and ATmega168           */
/* should work with other mega's, see code for details    */
/*                                                        */
/* ATmegaBOOT.c                                           */
/*                                                        */
/*                                                        */
/* 20090308: integrated Mega changes into main bootloader */
/*           source by D. Mellis                          */
/* 20080930: hacked for Arduino Mega (with the 1280       */
/*           processor, backwards compatible)             */
/*           by D. Cuartielles                            */
/* 20070626: hacked for Arduino Diecimila (which auto-    */
/*           resets when a USB connection is made to it)  */
/*           by D. Mellis                                 */
/* 20060802: hacked for Arduino by D. Cuartielles         */
/*           based on a previous hack by D. Mellis        */
/*           and D. Cuartielles                           */
/*                                                        */
/* Monitor and debug functions were added to the original */
/* code by Dr. Erik Lins, chip45.com. (See below)         */
/*                                                        */
/* Thanks to Karl Pitrich for fixing a bootloader pin     */
/* problem and more informative LED blinking!             */
/*                                                        */
/* For the latest version see:                            */
/* http://www.chip45.com/                                 */
/*                                                        */
/* ------------------------------------------------------ */
/*                                                        */
/* based on stk500boot.c                                  */
/* Copyright (c) 2003, Jason P. Kyle                      */
/* All rights reserved.                                   */
/* see avr1.org for original file and information         */
/*                                                        */
/* This program is free software; you can redistribute it */
/* and/or modify it under the terms of the GNU General    */
/* Public License as published by the Free Software       */
/* Foundation; either version 2 of the License, or        */
/* (at your option) any later version.                    */
/*                                                        */
/* This program is distributed in the hope that it will   */
/* be useful, but WITHOUT ANY WARRANTY; without even the  */
/* implied warranty of MERCHANTABILITY or FITNESS FOR A   */
/* PARTICULAR PURPOSE.  See the GNU General Public        */
/* License for more details.                              */
/*                                                        */
/* You should have received a copy of the GNU General     */
/* Public License along with this program; if not, write  */
/* to the Free Software Foundation, Inc.,                 */
/* 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */
/*                                                        */
/* Licence can be viewed at                               */
/* http://www.fsf.org/licenses/gpl.txt                    */
/*                                                        */
/* Target = Atmel AVR m128,m64,m32,m16,m8,m162,m163,m169, */
/* m8515,m8535. ATmega161 has a very small boot block so  */
/* isn't supported.                                       */
/*                                                        */
/* Tested with m168                                       */
/**********************************************************/


/* some includes */
#include <stdint.h>
#include <avr/io.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>

/* for use with simavr */
#include <avr/avr_mcu_section.h>
AVR_MCU(F_CPU, "atmega328p");
AVR_MCU_LONG(AVR_MMCU_TAG_LFUSE, (0xE2));
AVR_MCU_LONG(AVR_MMCU_TAG_HFUSE, (0xD8));
AVR_MCU_LONG(AVR_MMCU_TAG_EFUSE, (0xFD));

/* Use the F_CPU defined in Makefile */

/* 20060803: hacked by DojoCorp */
/* 20070626: hacked by David A. Mellis to decrease waiting time for auto-reset */
/* set the waiting time for the bootloader */
/* get this from the Makefile instead */
/* #define MAX_TIME_COUNT (F_CPU>>4) */

/* 20070707: hacked by David A. Mellis - after this many errors give up and launch application */
#define MAX_ERROR_COUNT 5

/* set the UART baud rate */
/* 20060803: hacked by DojoCorp */
//#define BAUD_RATE   115200
#ifndef BAUD_RATE
#define BAUD_RATE   19200
#endif


/* SW_MAJOR and MINOR needs to be updated from time to time to avoid warning message from AVR Studio */
/* never allow AVR Studio to do an update !!!! */
#define HW_VER	 0x02
#define SW_MAJOR 0x00
#define SW_MINOR 0x01


/* onboard LED is used to indicate, that the bootloader was entered (3x flashing) */
/* if monitor functions are included, LED goes on after monitor was entered */
#if defined __AVR_ATmega128__ || defined __AVR_ATmega1280__
/* Onboard LED is connected to pin PB7 (e.g. Crumb128, PROBOmega128, Savvy128, Arduino Mega) */
#define LED_DDR  DDRB
#define LED_PORT PORTB
#define LED_PIN  PINB
#define LED      PINB7
#elif defined POWER_MONITOR_FW
/* onboard LED is connection to pin PB0 on power monitor hat */
#define LED_DDR  DDRB
#define LED_PORT PORTB
#define LED_PIN  PINB
#define LED      PINB0
#else
/* Onboard LED is connected to pin PB5 in Arduino NG, Diecimila, Nano, and Duomilanuove */ 
/* other boards like e.g. Crumb8, Crumb168 are using PB2 (D13) */
#define LED_DDR  DDRB
#define LED_PORT PORTB
#define LED_PIN  PINB
#define LED      PINB5
#endif

#if defined(POWER_MONITOR_FW) || defined(NANO_PROTO_FW)

// the boot pin (D9) [MCU_RUNNING]
#define BOOT_DDR DDRB
#define BOOT_PORT PORTB
#define BOOT_PIN PINB
#define BOOT PINB1

// set MCU_RUNNING (APP) to external pull down
AVR_MCU_EXTERNAL_PORT_PULL('B', (1<<1), 0)

// the ready for new spi txn pin (D2) [BUTTON]
#define BUTTON_DDR DDRD
#define BUTTON_PORT PORTD
#define BUTTON_PIN PIND
#define BUTTON PIND2

AVR_MCU_EXTERNAL_PORT_PULL('D', (1<<2), 1)

#endif

#if defined(POWER_MONITOR_FW)
    
// set MCU_SHUTDOWN to external pullup
AVR_MCU_EXTERNAL_PORT_PULL('B', (1<<6), 1)

#elif defined(NANO_PROTO_FW)
    
// set MCU_SHUTDOWN to external pullup
AVR_MCU_EXTERNAL_PORT_PULL('D', (1<<3), 1)

#endif


/* define various device id's */
/* manufacturer byte is always the same */
#define SIG1	0x1E	// Yep, Atmel is the only manufacturer of AVR micros.  Single source :(

#if defined __AVR_ATmega1280__
#define SIG2	0x97
#define SIG3	0x03
#define PAGE_SIZE	0x80U	//128 words

#elif defined __AVR_ATmega1281__
#define SIG2	0x97
#define SIG3	0x04
#define PAGE_SIZE	0x80U	//128 words

#elif defined __AVR_ATmega128__
#define SIG2	0x97
#define SIG3	0x02
#define PAGE_SIZE	0x80U	//128 words

#elif defined __AVR_ATmega64__
#define SIG2	0x96
#define SIG3	0x02
#define PAGE_SIZE	0x80U	//128 words

#elif defined __AVR_ATmega32__
#define SIG2	0x95
#define SIG3	0x02
#define PAGE_SIZE	0x40U	//64 words

#elif defined __AVR_ATmega16__
#define SIG2	0x94
#define SIG3	0x03
#define PAGE_SIZE	0x40U	//64 words

#elif defined __AVR_ATmega8__
#define SIG2	0x93
#define SIG3	0x07
#define PAGE_SIZE	0x20U	//32 words

#elif defined __AVR_ATmega88__
#define SIG2	0x93
#define SIG3	0x0a
#define PAGE_SIZE	0x20U	//32 words

#elif defined __AVR_ATmega168__
#define SIG2	0x94
#define SIG3	0x06
#define PAGE_SIZE	0x40U	//64 words

#elif defined __AVR_ATmega328P__
#define SIG2	0x95
#define SIG3	0x0F
#define PAGE_SIZE	0x40U	//64 words

#elif defined __AVR_ATmega328__
#define SIG2	0x95
#define SIG3	0x14
#define PAGE_SIZE	0x40U	//64 words

#elif defined __AVR_ATmega162__
#define SIG2	0x94
#define SIG3	0x04
#define PAGE_SIZE	0x40U	//64 words

#elif defined __AVR_ATmega163__
#define SIG2	0x94
#define SIG3	0x02
#define PAGE_SIZE	0x40U	//64 words

#elif defined __AVR_ATmega169__
#define SIG2	0x94
#define SIG3	0x05
#define PAGE_SIZE	0x40U	//64 words

#elif defined __AVR_ATmega8515__
#define SIG2	0x93
#define SIG3	0x06
#define PAGE_SIZE	0x20U	//32 words

#elif defined __AVR_ATmega8535__
#define SIG2	0x93
#define SIG3	0x08
#define PAGE_SIZE	0x20U	//32 words
#endif


/* function prototypes */
void spi_txn(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4);
void byte_response(uint8_t);
void flash_led(uint8_t);

/* some variables */
union address_union {
	uint16_t word;
	uint8_t  byte[2];
} address;

union length_union {
	uint16_t word;
	uint8_t  byte[2];
} length;

struct flags_struct {
	unsigned eeprom : 1;
	unsigned rampz  : 1;
} flags;

uint8_t buff[256];
uint8_t address_high;

uint8_t pagesz=0x80;

uint8_t bootuart = 0;

uint8_t error_count = 0;

uint8_t spi_txn_buf[4];

void app_start(void)
{
    // autoreset via watchdog (sneaky!)
    WDTCSR = _BV(WDE);
    while (1); // 16 ms
}

/* main program starts here */
int main(void)
{
	uint8_t idx;
	uint16_t w;
    
    // button pin stays low
    BUTTON_DDR |= _BV(BUTTON);
    BUTTON_PORT &= ~_BV(BUTTON);

    // if MCU_RUNNING pin is high, that means remain in bootloader mode
    // so set to input, no pullup, as it has external pull down
    BOOT_DDR &= ~_BV(BOOT);

    // if the application pin is low, jump to app
    if (!(BOOT_PIN & _BV(BOOT)))
        app_start();
    
#if defined(POWER_MONITOR_BOOTLOADER)
    // enable pin (4) is high
    DDRD |= _BV(4);
    PORTD |= _BV(4);
    
    // make sure mcu shutdown pin (6) is low
    // eeprom pin (7) is high
    DDRB |= (_BV(6)|_BV(7));
    PORTB &= ~_BV(6);
    PORTB |= _BV(7);
#endif

    // setup SPI for peripheral mode

    // CS, SCK, MOSI to input
    DDRB &= ~(_BV(2)|_BV(3)|_BV(5));
    // set pullups on input pins
    PORTB |= (_BV(2)|_BV(3)|_BV(5));
    
    // MISO to output
    DDRB |= _BV(4);

    // enable SPI
    SPCR = _BV(SPE);
    
	/* set LED pin as output */
	LED_DDR |= _BV(LED);


	/* flash onboard LED to signal entering of bootloader */
#if defined(__AVR_ATmega128__) || defined(__AVR_ATmega1280__)
	// 4x for UART0, 5x for UART1
	flash_led(NUM_LED_FLASHES + bootuart);
#else
	flash_led(NUM_LED_FLASHES);
#endif

	/* forever loop */
	for (;;) {

        // get some bytes
        spi_txn(0,0,0,0);


        /* A bunch of if...else if... gives smaller code than switch...case ! */

        /* Hello is anyone home ? */ 
        if(spi_txn_buf[0]=='0') {
            byte_response('0');
        }

        /* Leave programming mode  */
        else if(spi_txn_buf[0]=='Q') {
            app_start();
        }


        /* Set address, little endian. EEPROM in bytes, FLASH in words  */
        /* Perhaps extra address bytes may be added in future to support > 128kB FLASH.  */
        /* This might explain why little endian was used here, big endian used everywhere else.  */
        else if(spi_txn_buf[0]=='U') {
            address.byte[0] = spi_txn_buf[1];
            address.byte[1] = spi_txn_buf[2];
        }


        /* Write memory, length is big endian and is in bytes  */
        else if(spi_txn_buf[0]=='d') {
            length.byte[1] = spi_txn_buf[1];
            length.byte[0] = spi_txn_buf[2];
            flags.eeprom = 0;
            if (spi_txn_buf[3] == 'E')
                flags.eeprom = 1;
            spi_txn(0,0,0,0);
            // Store data in buffer, can't keep up with data stream whilst programming pages
            for (w=0,idx=0; w<length.word; w++,idx++) {
                if (idx == 4) {
                    idx = 0;
                    spi_txn(0,0,0,0);
                }
                buff[w] = spi_txn_buf[idx];
            }
            // verify last of bytes in transaction are 0's
            for (; idx<4; idx++)
                if (spi_txn_buf[idx] != 0)
                    app_start();
            if (flags.eeprom) {		                //Write to EEPROM one byte at a time
                address.word <<= 1;
                for(w=0; w<length.word; w++) {
                    eeprom_write_byte((void *)address.word,buff[w]);
                    address.word++;
                }			
            }
            else {					        //Write to FLASH one page at a time

                // if write length is greater than a page, bail
                if (length.word > (PAGE_SIZE<<1))
                    app_start();

                if (address.byte[1]>127)
                    address_high = 0x01;	//Only possible with m128, m256 will need 3rd address byte. FIXME
                else
                    address_high = 0x00;
#if defined(__AVR_ATmega128__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega1281__)
                RAMPZ = address_high;
#endif
                address.word = address.word << 1;	        //address * 2 -> byte location
                /* if ((length.byte[0] & 0x01) == 0x01) length.word++;	//Even up an odd number of bytes */
                if ((length.byte[0] & 0x01))
                    length.word++;	//Even up an odd number of bytes
                cli();					//Disable interrupts, just to be sure

                boot_page_erase_safe(address.word);
                uint8_t* addr = buff;
                for (uint16_t i=0; i<length.word; i+=2) {
                    uint16_t w = *addr++;
                    w += (*addr++) << 8;
                    boot_page_fill_safe(address.word + i, w);
                }
                boot_page_write_safe(address.word);
                boot_rww_enable_safe();
            }
        }

        /* Read memory block mode, length is big endian.  */
        else if(spi_txn_buf[0]=='t') {
            length.byte[1] = spi_txn_buf[1];
            length.byte[0] = spi_txn_buf[2];
#if defined(__AVR_ATmega128__) || defined(__AVR_ATmega1280__)
            if (address.word>0x7FFF)
                flags.rampz = 1;		// No go with m256, FIXME
            else
                flags.rampz = 0;
#endif
            address.word = address.word << 1;	        // address * 2 -> byte location
            if (spi_txn_buf[3] == 'E')
                flags.eeprom = 1;
            else
                flags.eeprom = 0;
            uint8_t read_buf[4];
            for (w=0,idx=0;w < length.word;w++,idx++) {		        // Can handle odd and even lengths okay
                // filled up spi buffer, send it..
                if (idx == 4) {
                    idx = 0;
                    spi_txn(read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
                }
                if (flags.eeprom) {	                        // Byte access EEPROM read
                    read_buf[idx] = eeprom_read_byte((void *)address.word);
                }
                else {
                
                    if (!flags.rampz)
                        read_buf[idx] = pgm_read_byte_near(address.word);
#if defined(__AVR_ATmega128__) || defined(__AVR_ATmega1280__)
                    else
                        read_buf[idx] = pgm_read_byte_far(address.word + 0x10000);
                    // Hmmmm, yuck  FIXME when m256 arrvies
#endif
                }
                address.word++;
            }
            // send remaining bytes via spi, if any..
            if (idx < 4) {
                for(;idx<4;idx++)
                    read_buf[idx] = 0;
                spi_txn(read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
            }
        }


        /* Get device signature bytes  */
        else if(spi_txn_buf[0]=='u') {
            spi_txn('u',SIG1,SIG2,SIG3);
        }

	} /* end of forever loop */

}

void spi_txn(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
{
    // button pin low to signal ready for more
    BUTTON_PORT &= ~_BV(BUTTON);
    uint32_t count = 0;
    for (int i=0; i<4; i++) {
        if (i == 0)
            SPDR = b1;
        else if (i == 1)
            SPDR = b2;
        else if (i == 2)
            SPDR = b3;
        else
            SPDR = b4;
        while (!(SPSR & _BV(SPIF))) {
            count++;
            if (count > MAX_TIME_COUNT)
                app_start();
            // take button pin back high
            BUTTON_PORT |= _BV(BUTTON);
        }
        if (SPSR & _BV(WCOL)) {
            if (error_count++ == MAX_ERROR_COUNT)
                app_start();
        }
        spi_txn_buf[i] = SPDR;
    }
}

void byte_response(uint8_t val)
{
    spi_txn(0x14,val,0x10,0);
}


void flash_led(uint8_t count)
{
	while (count--) {
		LED_PORT |= _BV(LED);
		_delay_ms(100);
		LED_PORT &= ~_BV(LED);
		_delay_ms(100);
	}
}

