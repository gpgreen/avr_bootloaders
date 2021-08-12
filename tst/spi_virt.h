/*
	spi_virt.h
    Copyright 2021 Greg Green <ggreen@bit-builder.com>
    
 	This file is part of avr_bootloaders.
    Emulates an MCU acting as SPI controller and the avr in simavr the peripheral
 */

#ifndef SPI_VIRT_H_
#define SPI_VIRT_H_

#include <stdint.h>
#include "sim_avr.h"

/*-----------------------------------------------------------------------*/

#define CS_DELAY_CYCLES 4
#define SCK_DELAY_CYCLES 6

/*-----------------------------------------------------------------------*/

enum {
    SPI_VIRT_CS,
    SPI_VIRT_SDI,
	SPI_VIRT_SDO,
    SPI_VIRT_BYTE_TXN_START,
    SPI_VIRT_BYTE_TXN_END,
    SPI_VIRT_TXN_END,
	SPI_VIRT_COUNT
};

/*-----------------------------------------------------------------------*/

typedef enum {
    Idle,
    ByteTxn,
} spi_virt_state_t;

/*-----------------------------------------------------------------------*/

typedef struct spi_txn
{
    int length;
    uint8_t* buf;
    int raise_cs;
} spi_txn_t;
    
/*-----------------------------------------------------------------------*/

typedef struct spi_virt 
{
    struct avr_t * avr;
    avr_irq_t * irq;
    spi_virt_state_t state;
    uint8_t sdo_val;
    uint8_t sdi_val;
    spi_txn_t* cur_txn;
    int txn_idx;
} spi_virt_t;

/*-----------------------------------------------------------------------*/

typedef struct spi_test_txn 
{
    avr_cycle_count_t start_cycle;
    spi_txn_t transaction;
    struct spi_test_txn *next;
} spi_test_txn_t;

/*-----------------------------------------------------------------------*/

typedef struct spi_txn_input 
{
    char input_path[2048];
    struct spi_test_txn * first;
} spi_txn_input_t ;

/*-----------------------------------------------------------------------*/

extern spi_txn_input_t test_input;

/*-----------------------------------------------------------------------*/

extern void spi_virt_init(struct avr_t * avr, spi_virt_t * part);

extern void spi_virt_start_txn(spi_virt_t * part, spi_txn_t* txn);

extern void spi_txn_input_init(char* path, spi_virt_t * part);

extern void spi_txn_input_cleanup(void);

/*-----------------------------------------------------------------------*/

#endif // SPI_VIRT_H_
