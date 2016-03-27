#ifndef BDM_H
#define BDM_H

/*
 * bdm.h
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

/*
 * Teensy PIN assignment for BDM
 *
 * LED:         PTC5 (PIN 13)
 * UART RX:     PTB16 (PIN 0)
 * UART TX:     PTB17 (PIN 1)
 * /BKPT:       PTD0 (PIN 2)        --> output
 * DSCLK:       PTA12 (PIN3)        --> output
 * DSI:         PTA13 (PIN4)        --> input
 * DSO:         PTD7 (PIN5)         --> output
 */

#define BKPT_LO(s)      GPIOD_PCOR = (1 << 0)
#define BKPT_HI(nu)     GPIOD_PSOR = (1 << 0)

#define DSCLK_LO()      GPIOA_PCOR = (1 << 12)
#define DSCLK_HI()      GPIOA_PSOR = (1 << 12)

#define DSO_LO()        GPIOD_PCOR = (1 << 7)
#define DSO_HI()        GPIOD_PSOR = (1 << 7)
#define DSO_TOGGLE()    GPIOD_PTOR = (1 << 7)

#define DSI()           (GPIOA_PDIR & (1 << 13))

extern uint32_t bdmcf_txrx(uint32_t mess);
extern void bdmcf_ta(uint32_t time_ms);
extern void bdmcf_reset(void);
#endif // BDM_H

