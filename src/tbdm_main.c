/*
 * tbdm_main.c
 *
 * This file is part of tbdm.
 *
 * tbdm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tbdm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tbdm.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2016        M. Froeschle
 *
 * Parts of this code are based on Kevin Cuzner's blog post "Teensy 3.1 bare metal: Writing a USB driver"
 * from http://kevincuzner.com. Many thanks for the groundwork!
 *
 */

#include "common.h"
#include "arm_cm4.h"
#include "uart.h"
#include "usb.h"
#include "xprintf.h"
#include "wait.h"
#include "bdm.h"


#define DBG_MAIN
#ifdef DBG_MAIN
#define dbg(format, arg...) do { xprintf("DEBUG (%s()): " format, __FUNCTION__, ##arg);} while(0)
#else
#define dbg(format, arg...) do {;} while (0)
#endif /* DBG_MAIN */
#define err(format, arg...) do { xprintf("ERROR (%s()): " format, __FUNCTION__, ##arg); } while(0)

#define MAJOR_VERSION   0
#define MINOR_VERSION   1

/*
 * Teensy PIN assignment
 *
 * LED:         PTC5 (PIN 13)
 * UART RX:     PTB16 (PIN 0)
 * UART TX:     PTB17 (PIN 1)
 * /BKPT:       PTD0 (PIN 2)
 * DSCLK:       PTA12 (PIN3)
 * DSI:         PTA13 (PIN4)
 * DSO:         PTD7 (PIN5)
 */

int main(void)
{
    uint32_t v;

    PORTC_PCR5 = PORT_PCR_MUX(0x1);     /* LED is on PTC5 (pin 13), config as GPIO (alt = 1) */
    GPIOC_PDDR = (1 << 5);              /* make this an output pin */
    LED_OFF();                          /* start with LED off */

    PORTA_PCR12 = PORT_PCR_MUX(0x1);
    PORTA_PCR13 = PORT_PCR_MUX(0x1);
    GPIOA_PDDR = (1 << 12);         /* make this outputs */
    GPIOA_PCOR = (1 << 12);

    PORTD_PCR7 = PORT_PCR_MUX(0x1);
    GPIOD_PDDR = (1 << 7);
    GPIOD_PCOR = (1 << 7);


    v = (uint32_t) mcg_clk_hz;
    v = v / 1000000;

    /*
     * enable the PIT clock
     */
    SIM_SCGC3 |= SIM_SCGC3_ADC1_MASK;
    SIM_SCGC6 |= SIM_SCGC6_PIT_MASK | SIM_SCGC6_ADC0_MASK;

    /*
     * turn on PIT
     */
    PIT_MCR = 0x00;

    /*
     * Timer 0
     */
    PIT_LDVAL0 = 0xffffffff;            /* setup timer 0 free running */
    PIT_TCTRL0 = PIT_TCTRL_TIE_MASK;    /* enable timer 0 interrupts */
    PIT_TCTRL0 |= PIT_TCTRL_TEN_MASK;   /* start timer 0 */

    /*
     * Timer 1
     */
    PIT_LDVAL1 = 0x0003E7FF;            /* setup timer 1 for 256000 cycles */
    PIT_TCTRL1 = PIT_TCTRL_TIE_MASK;    /* enable Timer 1 interrupts */
    PIT_TCTRL1 |= PIT_TCTRL_TEN_MASK;   /* start Timer 1 */


    uart_init(UART0_BASE_PTR, core_clk_khz, 230400);


    xprintf("Teensy TBLCF V%d.%02d, %s %s\r\n\r\n", MAJOR_VERSION, MINOR_VERSION, __DATE__, __TIME__);

    dbg("init USB device ...\r\n");
    usb_init();
    dbg("done.\r\n");

    enable_irq(IRQ(INT_PIT0));
    enable_irq(IRQ(INT_PIT1));

    dbg("enable interrupts ...\r\n");
    EnableInterrupts();
    dbg("done.\r\n");

    while(1)
    {

    }

    return  0;                        // should never get here!
}

void PIT0_IRQHandler()
{
    /*
     * reset the interrupt flag
     */
    PIT_TFLG0 |= PIT_TFLG_TIF_MASK;
    dbg("timer 0 interrupt!\r\n");
}

void PIT1_IRQHandler()
{
    static uint16_t count = 0;
    static uint8_t stat = 0;

    if (count == 500)
    {
        stat ^= 0x01;
        count = 0;
    }
    else
    {
        count++;
    }

    //reset the interrupt flag
    PIT_TFLG1 |= PIT_TFLG_TIF_MASK;
    // dbg("timer 1 interrupt!\r\n");
}

