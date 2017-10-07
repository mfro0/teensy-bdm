/*
    Turbo BDM Light ColdFire - bdm routines
    Copyright (C) 2005  Daniel Malik

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "bdmcf.h"
#include "commands.h"
#include "cmd_processing.h"

#ifdef MULTIPLE_SPEEDS
/* until more than one set of rx/tx functions is needed the speed of operation can be improved by not using the pointers */

/* pointers to Rx & Tx routines */
uint8_t near (*bdmcf_rx8_ptr)(void) = bdmcf_rx8_1;
void near (*bdmcf_tx8_ptr)(uint8_t) = bdmcf_tx8_1;
uint8_t near (*bdmcf_txrx8_ptr)(uint8_t) = bdmcf_txrx8_1;
uint8_t near (*bdmcf_txrx_start_ptr)(void) = bdmcf_txrx_start_1;

/* tables with pointers to Tx & Rx functions */
uint8_t (* const bdmcf_rx8_ptrs[])(void) = { bdmcf_rx8_1 };
void (* const bdmcf_tx8_ptrs[])(uint8_t) = { bdmcf_tx8_1 };
uint8_t (* const bdmcf_txrx8_ptrs[])(uint8_t) = { bdmcf_txrx8_1 };
uint8_t (* const bdmcf_txrx_start_ptrs[])(void) = { bdmcf_txrx_start_1 };
#endif

/* halts the target CPU (stops execution of the code and brings the part into BDM mode) */
void bdmcf_halt(void)
{
#ifdef INVERT
    BKPT_OUT = 1;       /* assert BKPT */
#else
    BKPT_OUT = 0;
#endif
    wait_us(10 * 70);      /* wait 700us, this should be enough even for a target running at 2kHz clock and yet it is short enough to fit into a single USB slot */

#ifdef INVERT
    BKPT_OUT=0;         /* deassert BKPT */
#else
    BKPT_OUT=1;
#endif

    bdmcf_complete_chk_rx();    /* added in revision 0.3 */
                                /* it is a workaround for a strange problem: CF CPU V2 seems to ignore the first transfer after a halt */
                                /* I do not admit I know why it happens, but the extra NOP command fixes the problem... */
                                /* the problem has nothing to do with the delay: adding up to 400ms of delay between the halt and the read did not fix it */
}

/* resets the target CPU either into BDM mode (parameter bkpt=0) or into notmal mode (parameter bkpt!=0) */
/* length of the reset pulse is 50ms, if BKPT is to be asserted it is held active for 50ms after reset is released */
void bdmcf_reset(uint8_t bkpt)
{
    RSTI_OUT = 0;               /* reset is active low */
    RSTI_DIRECTION = 1;         /* assert it */

    if (bkpt == 0)
    {
#ifdef INVERT
        BKPT_OUT = 1;           /* assert BKPT */
#else
        BKPT_OUT = 0;
#endif
    }
    wait_ms(50);               /* 50ms */
    RSTI_DIRECTION = 0;         /* deassert reset */
    wait_ms(50);               /* give the reset pin enough time to rise even with slow RC */

#ifdef INVERT
    BKPT_OUT = 0;               /* deassert BKPT */
#else
    BKPT_OUT = 1;
#endif

    cable_status.reset = NO_RESET_ACTIVITY;     /* clear the reset flag */
    bdmcf_complete_chk_rx();                    /* added in revision 0.3 */
}

/* asserts the TA signal for the specified duration */
/* the time is in 10us ticks */
void bdmcf_ta(uint8_t time_10us)
{
    TA_OUT = 0;                 /* TA is active low */
    TA_DIRECTION = 1;           /* assert it */
    wait_us(10 * time_10us);       /* wait the specified time */
    TA_DIRECTION = 0;           /* de-assert it */
    TA_OUT = 0;
}

/* transmits series of 17 bit messages, contents of messages is pointed to by *data */
/* first byte in the buffer is the MSB of the first message */
void bdmcf_tx(uint8_t count, uint8_t *data)
{
    while(count--)
    {
        bdmcf_txrx_start_ptr();
        bdmcf_tx8_ptr(*(data++));
        bdmcf_tx8_ptr(*(data++));
    }
}

/* waits for command complete indication, to be used with commands which only write data and do not read any result */
/* returns 0 on success and non-zero on error */
/* only checks the status, not the data bits */
uint8_t bdmcf_complete_chk(unsigned int next_cmd)
{
    uint8_t i = BDMCF_RETRY;
    uint8_t status;

    do
    {
        status = bdmcf_txrx_start_ptr();
        bdmcf_tx8_ptr(next_cmd >> 8);     /* send in the next command */
        bdmcf_tx8_ptr(next_cmd & 0xff);
        if (status == 0)
        {
            return 0;
        }
    } while ((i--) > 0);

    return 1;
}

/* waits for command complete indication, to be used with commands which only write data and do not read any result */
/* returns 0 on success and non-zero on error */
/* checks buss error as well as not-ready, but does not send the next command */
uint8_t bdmcf_complete_chk_rx(void)
{
    uint8_t i = BDMCF_RETRY;
    uint8_t status;
    uint8_t data;

    do
    {
        status = bdmcf_txrx_start_ptr();
        bdmcf_tx8_ptr(0);
        data = bdmcf_rx8_ptr();

        if (status == 0)
        {
            return 0;
        }
    } while ((data == 0x00) && ((i--) > 0));

    return 1;
}

/* receives series of 17 bit messages and stores them into the supplied buffer (MSB of the first message first) */
/* returns zero on success and non-zero on retry error */
uint8_t bdmcf_rx(uint8_t count, uint8_t *data)
{
    uint8_t i, status;

    while (count)
    {
        i = BDMCF_RETRY;
        do
        {
            status = bdmcf_txrx_start_ptr();
            *(data + 0) = bdmcf_rx8_ptr();
            *(data + 1) = bdmcf_rx8_ptr();
        } while ((status != 0) && ((*(data + 1)) == 0x00) && ((i--) > 0));  /* repeat while status==1 & data+1 == 00 (not ready, come again) */

        if (status != 0)
        {
            return 1;
        }
        count--;
        data += 2;
    }
    return 0;
}

/* receives series of 17 bit messages and stores them into the supplied buffer (MSB of the first message first) */
/* returns zero on success and non-zero on retry error */
/* transmits the next command while receiving the last message */
uint8_t bdmcf_rxtx(uint8_t count, uint8_t *data, unsigned int next_cmd)
{
    uint8_t i, status;

    while (count)
    {
        i = BDMCF_RETRY;
        count--;

        if (count)
        {
            do
            {
                status = bdmcf_txrx_start_ptr();
                *(data + 0) = bdmcf_rx8_ptr();
                *(data + 1) = bdmcf_rx8_ptr();
            } while ((status != 0) && ((*(data + 1)) == 0x00) && ((i--) > 0));
        }
        else
        {
            do
            {                                        /* last message - send the next command */
                status = bdmcf_txrx_start_ptr();
                *(data + 0) = bdmcf_txrx8_ptr(next_cmd >> 8);
                *(data + 1) = bdmcf_txrx8_ptr(next_cmd & 0xff);
            } while ((status != 0) && ((*(data + 1)) == 0x00) && ((i--) > 0));
        }

        if (status != 0)
        {
            return 1;
        }
        data += 2;
    }
    return 0;
}

/* transmits a 17 bit message, returns the status bit */
uint8_t bdmcf_tx_msg(unsigned int data)
{
    uint8_t status;

    status = bdmcf_txrx_start_ptr();
    bdmcf_tx8_ptr(data >> 8);
    bdmcf_tx8_ptr(data & 0xff);

    return status;
}

/* transmits a 17 bit message, returns the least significant byte of the response */
/* to be used for transmitting second message in a multi-message command which can fail (e.g. because target is not halted) */
/* the returned byte identifies what is happening: 00 = Not ready, 01 = Bus error, FF = Illegal command */
/* the correct response in these cases is Not Ready */
uint8_t bdmcf_tx_msg_half_rx(unsigned int data)
{
    uint8_t ret_val;

    bdmcf_txrx_start_ptr();
    bdmcf_tx8_ptr(data >> 8);
    ret_val = bdmcf_txrx8_ptr(data & 0xff);

    return ret_val;
}

/* receives a 17 bit message, returns the status bit, data is stored into the supplied data buffer MSB first */
uint8_t bdmcf_rx_msg(uint8_t *data)
{
    uint8_t status;

    status = bdmcf_txrx_start_ptr();
    *data = bdmcf_rx8_ptr();
    *(data + 1) = bdmcf_rx8_ptr();

    return status;
}

/* transmits & receives a 17 bit message, data in the buffer is transmited and then replaced with received data, returns the status bit */
uint8_t bdmcf_txrx_msg(uint8_t *data)
{
    uint8_t status;

    status = bdmcf_txrx_start_ptr();
    *data = bdmcf_txrx8_ptr(*data);
    *(data + 1) = bdmcf_txrx8_ptr(*(data + 1));
  return(status);
}

/* resynchronizes communication with the target in case of noise of the CLK line, etc. */
/* returns 0 in case of sucess, non-zero in case of error */
uint8_t bdmcf_resync(void)
{
    uint8_t i;
    unsigned int data = BDMCF_CMD_NOP;

    bdmcf_tx_msg(BDMCF_CMD_NOP);    /* send in 3 NOPs to clear any error */
    bdmcf_tx_msg(BDMCF_CMD_NOP);
    bdmcf_txrx_msg(&data);

    if ((data & 3) == 0)
    {
        return 1;     /* the last NOP did not return the expected value (at least one of the two bits should be 1) */
    }

    for (i = 18; i > 0; i++)
    {
        /* now start sending in another nop and watch the result */
        if (bdmcf_txrx_start_ptr() == 0)
        {
            break;   /* the first 0 is the status */
        }
    }
    if (i == 0)
    {
        return 1;
    }
    /* transmitted & received the status, finish the nop */
    bdmcf_tx8_ptr(0x00);
    bdmcf_tx8_ptr(0x00);

    return 0;
}

/* initialises the BDM interface */
void bdmcf_init(void)
{
#ifdef NOT_USED
    PTA  = BDMCF_IDLE;    /* preload idle state into port A data register */
#ifdef DEBUG
    DDRA = DSI_OUT_MASK | TCLK_OUT_MASK | DSCLK_OUT_MASK | BKPT_OUT_MASK; /* RSTI_OUT and TA_OUT are inactive, the remaining OUT signals are outputs */
#else
    DDRA = DSI_OUT_MASK | TCLK_OUT_MASK | DSCLK_OUT_MASK | BKPT_OUT_MASK | DDRA_DDRA7; /* PTA7 is unused when not debugging, make sure it is output in such case */
#endif
    PTC  = 0;
    DDRC = DDRC_DDRC1;    /* make pin PTC1 output (it is not bonded out on the 20 pin package anyway) */
    POCR = POCR_PTE20P;   /* enable pull-ups on PTE0-2 (unused pins) */

    /* RSTO edge capture */
    T1SC = 0;             /* enable timer 1 */
    T1SC0;                /* read the status and control register */

#ifdef INVERT
    T1SC0 = T1SC0_ELS0A_MASK;   /* capture rising edge (invert), this write will also clear the interrupt flag if set */
#else
    T1SC0 = T1SC0_ELS0B_MASK;   /* capture falling edge (non-invert), this write will also clear the interrupt flag if set */
#endif
    T1SC0 |= T1SC0_CH0IE_MASK;    /* enable input capture interrupt */
#endif
    cable_status.reset = NO_RESET_ACTIVITY;  /* clear the reset flag */
}

/* this interrupt is called whenever an active edge is detected on the RSTO input */
void rsto_detect(void)
{
#ifdef NOT_USED
    T1SC0 &= ~T1SC0_CH0F_MASK;          /* clear the interrupt flag */
#endif
    cable_status.reset = RESET_DETECTED;  /* reset of the target was detected, leave it for the debugger to what it believes is appropriate */
}

/* transmits 8 bits */
void bdmcf_tx8_1(uint8_t data)
{
#ifdef NOT_USED
    asm {
        tax           /* move the input data to X */
#ifdef INVERT
        comx        /* invert the data */
#endif
#ifdef DEBUG
        lda   #(BDMCF_IDLE * 2)
#else
        lda   #(BDMCF_IDLE/2)
#endif
        /* transmit bit 7 */
        lslx    		  /* shift MSB into C */
#ifdef DEBUG
        rora        /* rotate C into A */
#else
        rola
#endif
        sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
#ifdef DEBUG
        lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
#else
        lda   #(BDMCF_IDLE/2)
#endif
#ifdef INVERT
        bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
#else
        bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
#endif
        /* transmit bit 6 */
        lslx    		  /* shift MSB into C */
#ifdef DEBUG
        rora        /* rotate C into A */
#else
        rola
#endif
        sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
#ifdef DEBUG
        lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
#else
        lda   #(BDMCF_IDLE/2)
#endif
#ifdef INVERT
        bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
#else
        bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
#endif
        /* transmit bit 5 */
        lslx    		  /* shift MSB into C */
#ifdef DEBUG
        rora        /* rotate C into A */
#else
        rola
#endif
        sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
#ifdef DEBUG
        lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
#else
        lda   #(BDMCF_IDLE/2)
#endif
#ifdef INVERT
        bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
#else
        bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
#endif
        /* transmit bit 4 */
        lslx    		  /* shift MSB into C */
#ifdef DEBUG
        rora        /* rotate C into A */
#else
        rola
#endif
        sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
#ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
#else
      lda   #(BDMCF_IDLE/2)
#endif
#ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
#else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
#endif
    /* transmit bit 3 */
    lslx    		  /* shift MSB into C */
#ifdef DEBUG
      rora        /* rotate C into A */
#else
            rola
#endif
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
#ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
#else
      lda   #(BDMCF_IDLE/2)
#endif
#ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
#else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
#endif
    /* transmit bit 2 */
    lslx    		  /* shift MSB into C */
#ifdef DEBUG
      rora        /* rotate C into A */
#else
            rola
#endif
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
#ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
#else
      lda   #(BDMCF_IDLE/2)
#endif
#ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
#else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
#endif
     /* transmit bit 1 */
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    /* transmit bit 0 */
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
        /* finish the transmission */
    #ifdef INVERT
      sec           /* bring the DSI_OUT low, we need to spend 2 cycles doing something anyway to make the timing right */
    #else
      clc
    #endif
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    sta     PTA   /* create falling edge on DSCLK */
  }
#endif
}

/* receives 8 bits */
uint8_t bdmcf_rx8_1(void) {
#ifdef NOT_USED
  asm {
    lda     #BDMCF_IDLE /* preload idle state of signals into A */
    sta     PTA         /* bring DSCLK and DSI low */
    clrh                /* load address of PTC to H:X */
    ldx     @DSO_IN_PORT
    /* receive bit 7 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    rola          /* shift the sampled bit into A from the bottom */
    #ifdef INVERT
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create falling edge on DSCLK */
    #else
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    lsr     ,X    /* shift the input data bit (PTC0) into C */
    /* receive bit 6 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    rola          /* shift the sampled bit into A from the bottom */
    #ifdef INVERT
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create falling edge on DSCLK */
    #else
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    lsr     ,X    /* shift the input data bit (PTC0) into C */
    /* receive bit 5 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    rola          /* shift the sampled bit into A from the bottom */
    #ifdef INVERT
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create falling edge on DSCLK */
    #else
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    lsr     ,X    /* shift the input data bit (PTC0) into C */
    /* receive bit 4 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    rola          /* shift the sampled bit into A from the bottom */
    #ifdef INVERT
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create falling edge on DSCLK */
    #else
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    lsr     ,X    /* shift the input data bit (PTC0) into C */
    /* receive bit 3 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    rola          /* shift the sampled bit into A from the bottom */
    #ifdef INVERT
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create falling edge on DSCLK */
    #else
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    lsr     ,X    /* shift the input data bit (PTC0) into C */
    /* receive bit 2 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    rola          /* shift the sampled bit into A from the bottom */
    #ifdef INVERT
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create falling edge on DSCLK */
    #else
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    lsr     ,X    /* shift the input data bit (PTC0) into C */
    /* receive bit 1 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    rola          /* shift the sampled bit into A from the bottom */
    #ifdef INVERT
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create falling edge on DSCLK */
    #else
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    lsr     ,X    /* shift the input data bit (PTC0) into C */
    /* receive bit 0 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    rola          /* shift the sampled bit into A from the bottom */
    #ifdef INVERT
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create falling edge on DSCLK */
    #else
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    lsr     ,X    /* shift the input data bit (PTC0) into C */
    rola          /* shift the sampled bit into A from the bottom */
    #ifdef INVERT
      coma        /* invert the data */
    #endif
  }
#endif
}

/* transmits and receives 8 bits */
uint8_t bdmcf_txrx8_1(uint8_t data)
{
#ifdef NOT_USED
  asm {
    tax           /* move the input data to X */
    #ifdef INVERT
      comx        /* invert the data */
    #endif
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    sta     PTA   /* bring DSCLK low and write the first (MSB) bit value to the port */
     /* transmit and receive bit 7 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    lsrx          /* shift X back to what it was */
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
    lsr     PTC   /* shift the input data bit (PTC0) into C */
        rolx          /* shift the sample into X from the bottom */
     /* transmit and receive bit 6 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    lsrx          /* shift X back to what it was */
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
    lsr     PTC   /* shift the input data bit (PTC0) into C */
        rolx          /* shift the sample into X from the bottom */
     /* transmit and receive bit 5 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    lsrx          /* shift X back to what it was */
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
    lsr     PTC   /* shift the input data bit (PTC0) into C */
        rolx          /* shift the sample into X from the bottom */
     /* transmit and receive bit 4 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    lsrx          /* shift X back to what it was */
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
    lsr     PTC   /* shift the input data bit (PTC0) into C */
        rolx          /* shift the sample into X from the bottom */
     /* transmit and receive bit 3 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    lsrx          /* shift X back to what it was */
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
    lsr     PTC   /* shift the input data bit (PTC0) into C */
        rolx          /* shift the sample into X from the bottom */
     /* transmit and receive bit 2 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    lsrx          /* shift X back to what it was */
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
    lsr     PTC   /* shift the input data bit (PTC0) into C */
        rolx          /* shift the sample into X from the bottom */
     /* transmit and receive bit 1 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    lsrx          /* shift X back to what it was */
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
    lsr     PTC   /* shift the input data bit (PTC0) into C */
        rolx          /* shift the sample into X from the bottom */
     /* transmit and receive bit 0 */
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    #ifdef DEBUG
      lda   #(BDMCF_IDLE*2)  /* reload idle value into A */
    #else
      lda   #(BDMCF_IDLE/2)
    #endif
    lslx    		  /* shift MSB into C */
    #ifdef DEBUG
      rora        /* rotate C into A */
    #else
            rola
    #endif
    lsrx          /* shift X back to what it was */
    sta     PTA   /* create falling edge on DSCLK and write the next bit value to the port */
    lsr     PTC   /* shift the input data bit (PTC0) into C */
        rolx          /* shift the sample into X from the bottom */
     /* transmission and reception is now complete, the result is in X */
    txa           /* move the result data to A */
    #ifdef INVERT
      coma        /* invert the data */
    #endif
  }
#endif
}

/*
 * transmits 1 bit of logic low value and receives 1 bit
 */
uint8_t bdmcf_txrx_start_1(void)
{
    uint8_t res = 0;

    DSO_IN = 0;     /* bring DSI low */
    DSCLK_OUT = 0;  /* bring DSCLK low */
    DSCLK_OUT = 1;  /* create rising edge on DSCLK */
    DSCLK_OUT = 0;  /* create falling edge on DSCLK */
    res = DSI_OUT;

    return res;

#ifdef NOT_USED
  asm {
    lda     #BDMCF_IDLE   /* preload idle state of signals into A */
     /* transmit zero and receive 1 bit */
    sta     PTA   /* bring DSCLK and DSI low */
        nop           /* make the time between the assignement and the rising edge the same as during normal transmission */
    nop
    #ifdef INVERT
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create rising edge on DSCLK */
    #else
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
        nop           /* make the time between the edges the same as during normal reception */
    #ifdef INVERT
      bset  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT  /* create falling edge on DSCLK */
    #else
      bclr  DSCLK_OUT_BITNUM,DSCLK_OUT_PORT
    #endif
    lsr     PTC   /* shift the input data bit (PTC0) into C */
    clra					/* clear the accumulator */
        rola          /* shift the sample into A from the bottom */
    #ifdef INVERT
      eor   #0x01 /* negate the received bit */
    #endif
  }
#endif
}

/* JTAG support */

/* initialises the JTAG TAP and brings the TAP into RUN-TEST/IDLE state */
void jtag_init(void)
{
#ifdef NOT_USED
  uint8_t i;
  PTA  = JTAG_IDLE;    /* preload idle state into port A data register */
  #ifdef DEBUG
    DDRA = TDI_OUT_MASK | TCLK_OUT_MASK | TRST_OUT_MASK | TMS_OUT_MASK; /* RSTI_OUT and TA_OUT are inactive, the remaining OUT signals are outputs */
  #else
    DDRA = TDI_OUT_MASK | TCLK_OUT_MASK | TRST_OUT_MASK | TMS_OUT_MASK | DDRA_DDRA7; /* PTA7 is unused when not debugging, make sure it is output in such case */
  #endif
  PTC  = 0;
  DDRC = DDRC_DDRC1;    /* make pin PTC1 output (it is not bonded out on the 20 pin package anyway) */
  POCR = POCR_PTE20P;   /* enable pull-ups on PTE0-2 (unused pins) */
  /* RSTO edge capture */
  T1SC = 0;             /* enable timer 1 */
  T1SC0;                /* read the status and control register */
  #ifdef INVERT
    T1SC0 = T1SC0_ELS0A_MASK;   /* capture rising edge (invert), this write will also clear the interrupt flag if set */
  #else
    T1SC0 = T1SC0_ELS0B_MASK;   /* capture falling edge (non-invert), this write will also clear the interrupt flag if set */
  #endif
  T1SC0 |= T1SC0_CH0IE_MASK;    /* enable input capture interrupt */
  cable_status.reset=NO_RESET_ACTIVITY;  /* clear the reset flag */
  /* now the JTAG pins are in their default states and the RSTO sensing is set-up */
  TRST_RESET();                 /* assert TRST */
  wait_ms(50);                 /* 50ms */
  TRST_SET();                   /* de-assert TRST */
  wait_ms(10);                 /* 10ms */
    TMS_SET();                    /* bring TMS high */
  for (i=10;i>0;i--) {	  		  /* create 10 pulses on the TCLK line in case TRST is not connected to bring TAP to TEST-LOGIC-RESET (but it better should be as it is BKPT...) */
    TCLK_SET();
    asm (NOP); asm (NOP); asm (NOP);  /* nop padding to ensure that TCLK frequency is <0.5MHz, TCLK freq must be <= system clock / 4 and min crystal freq for 52235 is 2MHz... */
    TCLK_RESET();
    asm (NOP); asm (NOP); asm (NOP);
  }
    TMS_RESET();                  /* take TMS low */
  TCLK_SET();										/* transition TAP to RUN-TEST/IDLE */
  asm (NOP); asm (NOP); asm (NOP);
  TCLK_RESET();
  asm (NOP); asm (NOP); asm (NOP);
#endif
}

/* trasitions the state machine from RUN-TEST/IDLE to SHIFT-DR or SHIFT-IR state */
/* if mode == 0 DR path is used, if mode != 0 IR path is used */
/* leaves TCLK high and TMS low on exit */
void jtag_transition_shift(uint8_t mode)
{
#ifdef NOT_USED
    TMS_SET();                    /* bring TMS high */
  TCLK_RESET();                 /* just in case TCLK was left high */
  asm (NOP); asm (NOP); asm (NOP);
  TCLK_SET();										/* transition TAP to SELECT DR-SCAN */
  asm (NOP); asm (NOP); asm (NOP);
  TCLK_RESET();
  asm (NOP); asm (NOP); asm (NOP);
  if (mode!=0) {                /* select the IR path */
    TCLK_SET(); 								/* transition TAP to SELECT IR-SCAN */
    asm (NOP); asm (NOP); asm (NOP);
    TCLK_RESET();
    asm (NOP); asm (NOP); asm (NOP);
  }
    TMS_RESET();                  /* take TMS low */
  TCLK_SET(); 								  /* transition TAP to CAPTURE-DR/IR */
  asm (NOP); asm (NOP); asm (NOP);
  TCLK_RESET();
  asm (NOP); asm (NOP); asm (NOP);
  TCLK_SET(); 								  /* transition TAP to SHIFT-DR/IR */
  asm (NOP); asm (NOP); asm (NOP);
  TCLK_RESET();
#endif
}

/* transitions the JTAG state machine to the TEST-LOGIC-REST state */
/* the jtag must be re-initialised after this has happened since no other routine knows how to get out of this state */
void jtag_transition_reset(void)
{
#ifdef NOT_USED
  uint8_t i;
    TMS_SET();                          /* bring TMS high */
  for (i=10;i>0;i--) {	  		        /* create 10 pulses on the TCLK line */
    TCLK_SET();
    asm (NOP); asm (NOP); asm (NOP);
    TCLK_RESET();
    asm (NOP); asm (NOP); asm (NOP);
  }
#endif
}

/* writes given bit stream into data/instruction path of JTAG */
/* bit_count specifies the number of bits to write */
/* data are transmitted starting with LSB of the LAST byte in the supplied buffer (for easier readability of code which uses JTAG) */
/* expects to find the TAP in SHIFT-DR or SHIFT-IR state */
/* if tap_transition == 0 leaves the TAP state unchanged, if tap_transition != 0 leaves the tap in RUN-TEST/IDLE */
void jtag_write(uint8_t tap_transition, uint8_t bit_count, uint8_t * datap)
{
#ifdef NOT_USED
  uint8_t bit_no=0;
  uint8_t data;
  datap += (bit_count>>3);                                  /* each byte has 8 bits, point to the last byte in the buffer */
  if ((bit_count&0x07)==0) datap--;                         /* the size fits into the bytes precisely */
  while (bit_no<bit_count) {
    if ((bit_no&0x07)==0) data=*(datap--);                  /* fetch new byte from the buffer and update the pointer */
    if (data&0x01) TDI_OUT_SET(); else TDI_OUT_RESET();     /* assign new bit value to TDI */
    TCLK_RESET();                                           /* take TCLK low */
    data>>=1;                                               /* shift the data down */
    bit_no++;                                               /* update the bit number */
    if (bit_no==bit_count) if (tap_transition!=0) TMS_SET();/* bring TMS high if exit from the SHIFT state is required */
    TCLK_SET();                                             /* take TCLK high */
  }
  if (tap_transition) {                                     /* the TAP is now in EXIT1-DR/IR if exit was requested */
    TCLK_RESET();                                           /* take TCLK low */
    asm (NOP); asm (NOP); asm (NOP);
    TCLK_SET();                                             /* bring TCLK high */ /* transition TAP to UPDATE-DR/IR */
    asm (NOP); asm (NOP); asm (NOP);
    TMS_RESET();                                            /* take TMS low */
    TCLK_RESET();                                           /* take TCLK low */
    asm (NOP); asm (NOP); asm (NOP);
    TCLK_SET();                                             /* bring TCLK high */ /* transition TAP to RUN-TEST/IDLE */
    asm (NOP); asm (NOP); asm (NOP);
    TCLK_RESET();                                           /* take TCLK low */
  }                                                         /* now the tap is in RUN-TEST/IDLE if exit was requested */
#endif
}

/* reads bitstream out of JTAG */
/* bit_count specifies the number of bits to read */
/* data are stored starting with LSB of the LAST byte in the supplied buffer (for easier readability of code which uses JTAG) */
/* expects to find the TAP in SHIFT-DR or SHIFT-IR state */
/* if tap_transition == 0 leaves the TAP state unchanged, if tap_transition != 0 leaves the tap in RUN-TEST/IDLE */
void jtag_read(uint8_t tap_transition, uint8_t bit_count, uint8_t * datap)
{
#ifdef NOT_USED
  uint8_t bit_no=0;
  uint8_t data=0;
  datap += (bit_count>>3);                                  /* each byte has 8 bits, point to the last byte in the buffer */
  if ((bit_count&0x07)==0) datap--;                         /* the size fits into the bytes precisely */
  while (bit_no<bit_count) {
    TCLK_RESET();                                           /* take TCLK low */
    data>>=1;                                               /* shift the data down */
    if (TDO_IN_SET) data|=0x80;                             /* move the current bit into the data variable */
    bit_no++;                                               /* update the bit number */
    if ((bit_no&0x07)==0) {
      *(datap--)=data;                                      /* store the received byte into the buffer and update the pointer */
      data=0;                                               /* clear the receiving variable */
    }
    if (bit_no==bit_count) if (tap_transition!=0) TMS_SET();/* bring TMS high if exit from the SHIFT state is required */
    TCLK_SET();                                             /* take TCLK high */
  }
  if ((bit_no&0x07)!=0) {                                   /* there are bits in the data variable to be stored into the buffer */
    data>>=(8-(bit_no&0x07));                               /* shift the bits into the right place */
    *(datap)=data;                                          /* store the last bits into the buffer */
  }
  if (tap_transition) {                                     /* the TAP is now in EXIT1-DR/IR if exit was requested */
    TCLK_RESET();                                           /* take TCLK low */
    asm (NOP); asm (NOP); asm (NOP);
    TCLK_SET();                                             /* bring TCLK high */ /* transition TAP to UPDATE-DR/IR */
    asm (NOP); asm (NOP); asm (NOP);
    TMS_RESET();                                            /* take TMS low */
    TCLK_RESET();                                           /* take TCLK low */
    asm (NOP); asm (NOP); asm (NOP);
    TCLK_SET();                                             /* bring TCLK high */ /* transition TAP to RUN-TEST/IDLE */
    asm (NOP); asm (NOP); asm (NOP);
    TCLK_RESET();                                           /* take TCLK low */
  }                                                         /* now the tap is in RUN-TEST/IDLE if exit was requested */
#endif
}
