/*
	tst_atmega_spi_bootloader.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
    Copyright 2021 Greg Green <ggreen@bit-builder.com>
    
 	This file is part of avr_bootloaders.
    This file was copied from simavr/exampels/simduino.c
 */

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>

#include <pthread.h>

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_hex.h"
#include "sim_gdb.h"
#include "parts/uart_pty.h"
#include "sim_vcd_file.h"
#include "spi_virt.h"

uart_pty_t uart_pty;
avr_t * avr = NULL;
avr_vcd_t vcd_file;
spi_virt_t mcu;

struct avr_flash {
	char avr_flash_path[1024];
	int avr_flash_fd;
};

// avr special flash initalization
// here: open and map a file to enable a persistent storage for the flash memory
void avr_special_init( avr_t * avr, void * data)
{
	struct avr_flash *flash_data = (struct avr_flash *)data;

	printf("%s\n", __func__);
	// open the file
	flash_data->avr_flash_fd = open(flash_data->avr_flash_path,
									O_RDWR|O_CREAT, 0644);
	if (flash_data->avr_flash_fd < 0) {
		perror(flash_data->avr_flash_path);
		exit(1);
	}
	// resize and map the file the file
	(void)ftruncate(flash_data->avr_flash_fd, avr->flashend + 1);
	ssize_t r = read(flash_data->avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1) {
		fprintf(stderr, "unable to load flash memory\n");
		perror(flash_data->avr_flash_path);
		exit(1);
	}
}

// avr special flash deinitalization
// here: cleanup the persistent storage
void avr_special_deinit( avr_t* avr, void * data)
{
	struct avr_flash *flash_data = (struct avr_flash *)data;

	printf("%s\n", __func__);
	lseek(flash_data->avr_flash_fd, SEEK_SET, 0);
	ssize_t r = write(flash_data->avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1) {
		fprintf(stderr, "unable to load flash memory\n");
		perror(flash_data->avr_flash_path);
	}
	close(flash_data->avr_flash_fd);
	uart_pty_stop(&uart_pty);
}

uint8_t hello_buf[8] = {'0', 0, 0, 0, 0, 0, 0, 0};
spi_txn_t hello1 = {
    .length = 4,
    .buf = hello_buf,
    .raise_cs = 0,
};
spi_txn_t hello2 = {
    .length = 4,
    .buf = &hello_buf[4],
    .raise_cs = 1,
};

static avr_cycle_count_t
spi_txn_end(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    spi_virt_start_txn(part, &hello2);
}

static avr_cycle_count_t
spi_txn_start(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    spi_virt_start_txn(part, &hello1);
    avr_cycle_timer_register(part->avr, 2000, spi_txn_end, part);
}

static void
spi_txn(spi_virt_t* mcu)
{
    avr_cycle_timer_register(mcu->avr, 2000000, spi_txn_start, mcu);
}

int main(int argc, char *argv[])
{
	struct avr_flash flash_data;
	char boot_path[1024] = "../build-power-monitor-bootloader-avr/power-monitor-bootloader-atmega328p.hex";
	uint32_t boot_base, boot_size;
	char * mmcu = "atmega328p";
	uint32_t freq = 8000000;
	int debug = 0;
	int verbose = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i] + strlen(argv[i]) - 4, ".hex"))
			strncpy(boot_path, argv[i], sizeof(boot_path));
		else if (!strcmp(argv[i], "-d"))
			debug++;
		else if (!strcmp(argv[i], "-v"))
			verbose++;
		else {
			fprintf(stderr, "%s: invalid argument %s\n", argv[0], argv[i]);
			exit(1);
		}
	}

	avr = avr_make_mcu_by_name(mmcu);
	if (!avr) {
		fprintf(stderr, "%s: Error creating the AVR core\n", argv[0]);
		exit(1);
	}

	uint8_t * boot = read_ihex_file(boot_path, &boot_size, &boot_base);
	if (!boot) {
		fprintf(stderr, "%s: Unable to load %s\n", argv[0], boot_path);
		exit(1);
	}
	if (boot_base > 32*1024*1024) {
		mmcu = "atmega2560";
		freq = 20000000;
	}
	printf("%s bootloader 0x%05x: %d bytes\n", mmcu, boot_base, boot_size);

	snprintf(flash_data.avr_flash_path, sizeof(flash_data.avr_flash_path),
			"tst_atmega_spi_bootloader_%s_flash.bin", mmcu);
	flash_data.avr_flash_fd = 0;
	// register our own functions
	avr->custom.init = avr_special_init;
	avr->custom.deinit = avr_special_deinit;
	avr->custom.data = &flash_data;
	avr_init(avr);
	avr->frequency = freq;

	memcpy(avr->flash + boot_base, boot, boot_size);
	free(boot);
	avr->pc = boot_base;
	/* end of flash, remember we are writing /code/ */
	avr->codeend = avr->flashend;
	avr->log = 1 + verbose;

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (debug) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	uart_pty_init(avr, &uart_pty);
	uart_pty_connect(&uart_pty, '0');

    spi_virt_init(avr, &mcu);
    spi_txn(&mcu);
    
	while (1) {
		int state = avr_run(avr);
		if ( state == cpu_Done || state == cpu_Crashed)
			break;
	}

}
