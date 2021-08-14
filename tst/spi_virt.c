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
    [SPI_VIRT_TXN_END] = "=spivirt.txnend",
    [SPI_VIRT_BUTTON] = ">spivirt.button",
    [SPI_VIRT_NEW_TXN_SIGNAL] = ">spivirt.newtxn",
};

/*-----------------------------------------------------------------------*/

// hook when button value received
static void
spi_virt_button_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    part->button = value & 0xFF;
    printf("SPIVIRT: BUTTON=0x%02x\n", part->button);
    // make sure we are well into startup phase before we trigger
    // a new spi transaction, as we get a low signal on button
    // right after bootup
    if (part->button == 0 && part->avr->cycle > 2000) {
        printf("SPIVIRT: BUTTON DOWN, new spi txn can start\n");
        avr_raise_irq(part->irq + SPI_VIRT_NEW_TXN_SIGNAL, (uint32_t)part);
    }
}

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

static void
spi_virt_txn_advance_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    part->current_txn->cycle = part->avr->cycle;
    if (part->output_file != NULL) {
        fprintf(part->output_file, "%lu ", part->avr->cycle);
        for (int i=0; i<part->current_txn->transaction.length; i++)
            fprintf(part->output_file, "%02x ", part->current_txn->transaction.buf[i]);
        fprintf(part->output_file, "\n");
        fflush(part->output_file);
    }
    part->current_txn = part->current_txn->next;
    if (part->current_txn != NULL) {
        spi_virt_start_txn(part, &part->current_txn->transaction);
    }
}

/*-----------------------------------------------------------------------*/

// initialize the part
void spi_virt_init(struct avr_t * avr, spi_virt_t * part,
    spi_virt_wiring_t * wiring)
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
    avr_irq_register_notify(part->irq + SPI_VIRT_BUTTON, spi_virt_button_hook, part);
    avr_irq_register_notify(part->irq + SPI_VIRT_NEW_TXN_SIGNAL, spi_virt_txn_advance_hook, part);
    
    avr_connect_irq(
        part->irq + SPI_VIRT_CS,
        avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(wiring->chip_select.port),
                      wiring->chip_select.pin));

    avr_connect_irq(
        avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT),
        part->irq + SPI_VIRT_SDO);

    avr_connect_irq(
        part->irq + SPI_VIRT_SDI,
        avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT));
    
    avr_connect_irq(
        avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(wiring->button.port),
                      wiring->button.pin),
        part->irq + SPI_VIRT_BUTTON);

    // make sure the CS pin is high
    avr_raise_irq(part->irq + SPI_VIRT_CS, 1);
    // button should be high
    avr_raise_irq(
        avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(wiring->button.port),
                      wiring->button.pin), 1);
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

/*-----------------------------------------------------------------------*/

static avr_cycle_count_t
spi_txn_start(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
    spi_virt_t * part = (spi_virt_t*)param;
    if (part->current_txn != NULL) {
        spi_virt_start_txn(part, &part->current_txn->transaction);
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

static void
initiate_spi_txn(spi_virt_t* mcu)
{
    mcu->current_txn = test_input.first;
    if (mcu->current_txn == NULL)
        return;
    printf("SPIVIRT: first spi transaction scheduled at [%lu]\n",
           test_input.start_cycle);
    avr_cycle_timer_register(mcu->avr, test_input.start_cycle, spi_txn_start, mcu);
}

/*-----------------------------------------------------------------------*/

/* example of input file
 *
 * # SPI Transaction input file
 * # first row is cycle to start this file of transactions
 * # each row is one transaction of 4 bytes
 * # next four columns are spi byte in hex
 * # final column is 0 if CS not raised, 1 if CS raised after transaction complete
 * 30 00 00 00 0    # hello, anyone there?
 * 00 00 00 00 1
 * 75 00 00 00 0    # device signature bytes
 * 00 00 00 00 1
 * 55 00 00 00 0    # set address 0x0000
 * 00 00 00 00 1
 */

// read an input file and get all the spi transactions
void spi_txn_input_init(char* path, spi_virt_t* mcu)
{
    int cnt;
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
        char* start = input_line;
        if (test_input.start_cycle == 0) {
            // get the cycle
            cnt = sscanf(start, "%lu", &test_input.start_cycle);
            if (cnt != 1) {
                goto error_exit;
            }
            continue;
        }
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
        start += 2;
        // make the transaction
        spi_test_txn_t * txn = malloc(sizeof(struct spi_test_txn));
        txn->cycle = 0;
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

/*-----------------------------------------------------------------------*/

void spi_virt_save_to_file(spi_virt_t * part, char * path)
{
    if (strlen(path) == 0)
        return;
    part->output_file = fopen(path, "w");
    if (part->output_file == NULL) {
        printf("SPIVIRT: unable to open file '%s' for writing\n", path);
        return;
    }
    
}


