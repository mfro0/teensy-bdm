/*
 * tbdm.c
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

#include "arm_cm4.h"
#include <stdint.h>
#include <stdbool.h>

void uart_init(UART_MemMapPtr uartch, int sysclk, int baud)
{
    uint16_t ubd, brfa;
    uint8_t temp;

    /*
     * assign pins for uart0
     */
    PORTB_PCR16 = PORT_PCR_PE_MASK | PORT_PCR_PS_MASK | PORT_PCR_PFE_MASK | PORT_PCR_MUX(3);
    PORTB_PCR17 = PORT_PCR_DSE_MASK | PORT_PCR_SRE_MASK | PORT_PCR_MUX(3);

    /* Enable the clock to the selected UART */
    if (uartch == UART0_BASE_PTR)
        SIM_SCGC4 |= SIM_SCGC4_UART0_MASK;
    else
        if (uartch == UART1_BASE_PTR)
            SIM_SCGC4 |= SIM_SCGC4_UART1_MASK;
        else
            if (uartch == UART2_BASE_PTR)
                SIM_SCGC4 |= SIM_SCGC4_UART2_MASK;
            else
                if(uartch == UART3_BASE_PTR)
                    SIM_SCGC4 |= SIM_SCGC4_UART3_MASK;else
                    if(uartch == UART4_BASE_PTR)
                        SIM_SCGC1 |= SIM_SCGC1_UART4_MASK;

    /*
     * Make sure that the transmitter and receiver are disabled while we
     * change settings.
     */
    UART_C2_REG(uartch) &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK );

    /*
     * Configure the UART for 8-bit mode, no parity
     */

    /*
     * We need all default settings, so entire register is cleared
     */
    UART_C1_REG(uartch) = 0;

    /* Calculate baud settings */
    ubd = (uint16_t) ((sysclk * 1000) / (baud * 16));

    /* Save off the current value of the UARTx_BDH except for the SBR */
    temp = UART_BDH_REG(uartch) & ~(UART_BDH_SBR(0x1F));
    UART_BDH_REG(uartch) = temp | UART_BDH_SBR(((ubd & 0x1F00) >> 8));
    UART_BDL_REG(uartch) = (uint8_t) (ubd & UART_BDL_SBR_MASK);

    /* Determine if a fractional divider is needed to get closer to the baud rate */
    brfa = (((sysclk * 32000) / (baud * 16)) - (ubd * 32));

    /* Save off the current value of the UARTx_C4 register except for the BRFA */
    temp = UART_C4_REG(uartch) & ~(UART_C4_BRFA(0x1F));
    UART_C4_REG(uartch) = temp | UART_C4_BRFA(brfa);
    /* Enable receiver and transmitter */
    UART_C2_REG(uartch) |= (UART_C2_TE_MASK | UART_C2_RE_MASK );
}

bool uart_char_present (UART_MemMapPtr channel)
{
    return (UART_S1_REG(channel) & UART_S1_RDRF_MASK) != 0;
}

char uart_receive(UART_MemMapPtr channel)
{
    /* Wait until character has been received */
    while (!uart_char_present(channel));

    /* Return the 8-bit data from the receiver */
    return UART_D_REG(channel);
}


void uart_send(UART_MemMapPtr uartch, uint8_t c)
{
    /*
     * wait until space is available in the FIFO
     */
    while (! (UART_S1_REG(uartch) / UART_S1_TDRE_MASK))
        ;

    /* Send the character */
    UART_D_REG(uartch) = (uint8_t) c;
}
