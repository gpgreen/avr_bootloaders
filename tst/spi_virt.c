#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "sim_avr.h"
#include "avr_spi.h"
#include "avr_ioport.h"
#include "spi_virt.h"

static const char * _spi_virt_irq_names[SPI_VIRT_COUNT] = {
	[SPI_VIRT_CS] = "<spivirt.cs",
	[SPI_VIRT_SDI] = "<spivirt.sdi",
	[SPI_VIRT_SDO] = ">spivirt.sdo",
    [SPI_VIRT_BYTE_TXN_START] = "=spivirt.u8txnstart",
    [SPI_VIRT_BYTE_TXN_END] = "=spivirt.u8txnend",
    [SPI_VIRT_TXN_END] = "=spivirt.txnend"
};

static void
spi_virt_sdo_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    part->sdo_val = value & 0xFF;
    printf("SPIVIRT: SDO=0x%02x\n", part->sdo_val);
}

static void
spi_virt_sdi_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    printf("SPIVIRT: SDI=0x%02x\n", (uint8_t)(value & 0xFF));
}

// timer callback at end of SPI byte transmission
static avr_cycle_count_t
spi_virt_cycle_proc(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    part->state = Idle;
    avr_raise_irq(part->irq + SPI_VIRT_SDI, part->sdi_val);
    avr_raise_irq(part->irq + SPI_VIRT_BYTE_TXN_END, (uint32_t)part);
}

// hook that starts an 8-bit SPI transaction
static void
spi_virt_byte_txn_start_hook(struct avr_irq_t * irq,
                             uint32_t value,
                             void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    printf("SPIVIRT: BYTE %d TXN START CS DN\n", part->txn_idx);
    part->state = ByteTxn;
    part->sdi_val = part->cur_txn->buf[part->txn_idx];
    part->sdo_val = 0;
    avr_raise_irq(part->irq + SPI_VIRT_CS, 0);
    avr_cycle_timer_register(part->avr,
                             CS_DELAY_CYCLES * 2 + SCK_DELAY_CYCLES*16,
                             spi_virt_cycle_proc, part);
}

// hook called at end of 8-bit SPI transaction
static void
spi_virt_byte_txn_end_hook(struct avr_irq_t * irq,
                           uint32_t value,
                           void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    printf("SPIVIRT: BYTE %d TXN END\n", part->txn_idx);
    part->cur_txn->buf[part->txn_idx++] = part->sdo_val;
    if (part->txn_idx == part->cur_txn->length) {
        avr_raise_irq(part->irq + SPI_VIRT_TXN_END, (uint32_t)part);
    } else {
        avr_raise_irq(part->irq + SPI_VIRT_BYTE_TXN_START, (uint32_t)part);
    }
    printf("AVR CYCLE: %lu\n", part->avr->cycle);
}

// hook called at end of a SPI transaction
static void
spi_virt_txn_end_hook(struct avr_irq_t * irq,
                      uint32_t value,
                      void * param)
{
    printf("SPIVIRT: TXN END\n");
    spi_virt_t * part = (spi_virt_t*)param;
    if (part->cur_txn->raise_cs) {
        printf("SPIVIRT: CS UP\n");
        avr_raise_irq(part->irq + SPI_VIRT_CS, 1);
    }
    part->cur_txn = NULL;
}

void spi_virt_init(struct avr_t * avr, spi_virt_t * part)
{
    memset(part, 0, sizeof(spi_virt_t));
    part->avr = avr;
    part->state = Idle;

    
    part->irq = avr_alloc_irq(&avr->irq_pool, 0, SPI_VIRT_COUNT, _spi_virt_irq_names);
    avr_irq_register_notify(part->irq + SPI_VIRT_SDO, spi_virt_sdo_hook, part);
    avr_irq_register_notify(part->irq + SPI_VIRT_SDI, spi_virt_sdi_hook, part);
    avr_irq_register_notify(part->irq + SPI_VIRT_BYTE_TXN_START, spi_virt_byte_txn_start_hook, part);
    avr_irq_register_notify(part->irq + SPI_VIRT_BYTE_TXN_END, spi_virt_byte_txn_end_hook, part);
    avr_irq_register_notify(part->irq + SPI_VIRT_TXN_END, spi_virt_txn_end_hook, part);
    
    avr_connect_irq(
        part->irq + SPI_VIRT_CS,
        avr_io_getirq(part->avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 2));

    avr_connect_irq(
        avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT),
        part->irq + SPI_VIRT_SDO);

    avr_connect_irq(
        part->irq + SPI_VIRT_SDI,
        avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT));
    
    // make sure the CS, SCK pins are correct
    avr_raise_irq(part->irq + SPI_VIRT_CS, 1);

    struct avr_io_t *pio = avr->io_port;
    for (;pio != NULL; pio = pio->next) {
        printf("io module: %s\n", pio->kind);
    }
}

// method to call when a SPI transaction is to start
void spi_virt_start_txn(spi_virt_t * part, spi_txn_t * txn)
{
    if (txn == NULL)
        return;
    part->cur_txn = txn;
    part->txn_idx = 0;
    avr_raise_irq(part->irq + SPI_VIRT_BYTE_TXN_START, (uint32_t)part);
}
