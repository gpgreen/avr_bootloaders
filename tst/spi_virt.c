#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "sim_avr.h"
#include "avr_spi.h"
#include "avr_ioport.h"
#include "spi_virt.h"

/*-----------------------------------------------------------------------*/

static const char * _spi_virt_irq_names[SPI_VIRT_COUNT] = {
	[SPI_VIRT_CS] = "<spivirt.cs",
	[SPI_VIRT_SDI] = "<spivirt.sdi",
	[SPI_VIRT_SDO] = ">spivirt.sdo",
    [SPI_VIRT_BYTE_TXN_START] = "=spivirt.u8txnstart",
    [SPI_VIRT_BYTE_TXN_END] = "=spivirt.u8txnend",
    [SPI_VIRT_TXN_END] = "=spivirt.txnend"
};

/*-----------------------------------------------------------------------*/

// hook when SDO value received
static void
spi_virt_sdo_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    part->sdo_val = value & 0xFF;
    printf("SPIVIRT: SDO=0x%02x\n", part->sdo_val);
}

/*-----------------------------------------------------------------------*/

// hook when SDI value is set
static void
spi_virt_sdi_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    printf("SPIVIRT: SDI=0x%02x\n", (uint8_t)(value & 0xFF));
}

/*-----------------------------------------------------------------------*/

// timer callback at end of SPI byte transmission
static avr_cycle_count_t
spi_virt_cycle_proc(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    part->state = Idle;
    avr_raise_irq(part->irq + SPI_VIRT_SDI, part->sdi_val);
    avr_raise_irq(part->irq + SPI_VIRT_BYTE_TXN_END, (uint32_t)part);
}

/*-----------------------------------------------------------------------*/

// hook that starts an 8-bit SPI transaction
static void
spi_virt_byte_txn_start_hook(struct avr_irq_t * irq,
                             uint32_t value,
                             void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    printf("SPIVIRT: TXN START CS DN\nSPIVIRT: BYTE [%d] START\n", part->txn_idx);
    part->state = ByteTxn;
    part->sdi_val = part->cur_txn->buf[part->txn_idx];
    part->sdo_val = 0;
    avr_raise_irq(part->irq + SPI_VIRT_CS, 0);
    avr_cycle_timer_register(part->avr,
                             CS_DELAY_CYCLES * 2 + SCK_DELAY_CYCLES*16,
                             spi_virt_cycle_proc, part);
}

/*-----------------------------------------------------------------------*/

// hook called at end of 8-bit SPI transaction
static void
spi_virt_byte_txn_end_hook(struct avr_irq_t * irq,
                           uint32_t value,
                           void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    printf("SPIVIRT: BYTE [%d] END\n", part->txn_idx);
    part->cur_txn->buf[part->txn_idx++] = part->sdo_val;
    if (part->txn_idx == part->cur_txn->length) {
        avr_raise_irq(part->irq + SPI_VIRT_TXN_END, (uint32_t)part);
    } else {
        avr_raise_irq(part->irq + SPI_VIRT_BYTE_TXN_START, (uint32_t)part);
    }
    printf("AVR CYCLE: %lu\n", part->avr->cycle);
}

/*-----------------------------------------------------------------------*/

// hook called at end of a SPI transaction
static void
spi_virt_txn_end_hook(struct avr_irq_t * irq,
                      uint32_t value,
                      void * param)
{
    printf("SPIVIRT: --> TXN END <--\n");
    spi_virt_t * part = (spi_virt_t*)param;
    if (part->cur_txn->raise_cs) {
        printf("SPIVIRT: CS UP\n");
        avr_raise_irq(part->irq + SPI_VIRT_CS, 1);
    }
    part->cur_txn = NULL;
}

/*-----------------------------------------------------------------------*/

// initialize the part
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
    avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 2), 0);
}

/*-----------------------------------------------------------------------*/

// method to call when a SPI transaction is to start
void spi_virt_start_txn(spi_virt_t * part, spi_txn_t * txn)
{
    if (txn == NULL)
        return;
    part->cur_txn = txn;
    part->txn_idx = 0;
    avr_raise_irq(part->irq + SPI_VIRT_BYTE_TXN_START, (uint32_t)part);
}

/*-----------------------------------------------------------------------*/
/* Following is to handle a text file of transactions                    */
/*-----------------------------------------------------------------------*/

// global variable to hold spi transactions
spi_txn_input_t test_input;

// pointer to the current spi transaction
spi_test_txn_t * current_spi_txn;

/*-----------------------------------------------------------------------*/

static avr_cycle_count_t
spi_txn_start(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    spi_virt_start_txn(part, &current_spi_txn->transaction);
    current_spi_txn = current_spi_txn->next;
    if (current_spi_txn != NULL) {
        if (avr->cycle > current_spi_txn->start_cycle) {
            printf("SPIVIRT: next spi txn scheduled cycle of %lu is backwards in time\n",
                   current_spi_txn->start_cycle);
            return 0;
        }
        avr_cycle_count_t t = current_spi_txn->start_cycle - avr->cycle;
        if (t == 0)
            spi_txn_start(part->avr, avr->cycle, part);
        else
            avr_cycle_timer_register(part->avr, t, spi_txn_start, part);
    }
    return 0;
}


/*-----------------------------------------------------------------------*/

static void
initiate_spi_txn(spi_virt_t* mcu)
{
    current_spi_txn = test_input.first;
    if (current_spi_txn == NULL)
        return;
    printf("SPIVIRT: first spi transactions scheduled at cycle %lu\n",
           current_spi_txn->start_cycle);
    avr_cycle_timer_register(mcu->avr, current_spi_txn->start_cycle, spi_txn_start, mcu);
}

/*-----------------------------------------------------------------------*/

/* example of input file
 *
 * # SPI Transaction input file
 * # each row is one transaction of 4 bytes
 * # first column is which avr cycle to start in
 * # next four columns are spi byte in hex
 * # final column is 0 if CS not raised, 1 if CS raised after transaction complete
 * 2000000 30 00 00 00 0    # hello, anyone there?
 * 2002000 00 00 00 00 1
 * 2005000 75 00 00 00 0    # device signature bytes
 * 2007000 00 00 00 00 1
 * 2012000 55 00 00 00 0    # set address 0x0000
 * 2017000 00 00 00 00 1
 */

// read an input file and get all the spi transactions
void spi_txn_input_init(char* path, spi_virt_t* mcu)
{
    int txn_count = 0;
    char input_line[80];
    test_input.first = NULL;
    strncpy(test_input.input_path, path, sizeof(test_input.input_path));

    if (strlen(path) == 0)
        return;
    
    FILE* f = fopen(test_input.input_path, "r");
    if (f == NULL)
        return;
    spi_test_txn_t ** last = &test_input.first;
    while (1) {
        if (fgets(input_line, 80, f) != input_line) {
            if (feof(f))
                break;
            goto error_exit;
        }
        // make sure line is null terminated
        if (input_line[79] != 0)
            input_line[79] = 0;
        // empty lines is end of file
        if (strlen(input_line) == 0)
            break;
        // skip comment lines
        if (input_line[0] == '#' || input_line[0] == '\n')
            continue;
        // get the cycle
        uint64_t cycle;
        char* start = input_line;
        int cnt = sscanf(start, "%lu", &cycle);
        if (cnt != 1) {
            goto error_exit;
        }
        start = strchr(start, ' ');
        // get the 4 hex bytes
        uint8_t hexnum[4];
        for (int i=0; i<4; i++) {
            cnt = sscanf(start, "%hhx", &hexnum[i]);
            if (cnt != 1)
                goto error_exit;
            start = strchr(start + 1, ' ');
        }
        // get the cs raise flag
        int cs;
        cnt = sscanf(start + 1, "%d", &cs);
        if (cnt != 1)
            goto error_exit;
        // make the transaction
        spi_test_txn_t * txn = malloc(sizeof(struct spi_test_txn));
        txn->start_cycle = cycle;
        txn->transaction.length = 4;
        uint8_t* buf = malloc(4);
        memcpy(buf, hexnum, 4);
        txn->transaction.buf = buf;
        txn->transaction.raise_cs = cs;
        txn->next = NULL;
        *last = txn;
        last = &txn->next;
        txn_count++;
    }
    fclose(f);
    printf("SPIVIRT: file '%s' parsed\n", test_input.input_path);
    printf("SPIVIRT: %d transactions created.\n", txn_count);
    initiate_spi_txn(mcu);
    return;
error_exit:
    perror("Error: ");
    fclose(f);
    test_input.first = NULL;
}
            
/*-----------------------------------------------------------------------*/

void spi_txn_input_cleanup(void)
{
    spi_test_txn_t *txn = test_input.first;
    while (txn != NULL) {
        free(txn->transaction.buf);
        spi_test_txn_t *delete_me = txn;
        txn = txn->next;
        free(delete_me);
    }
}

