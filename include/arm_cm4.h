/*
 *
 * arm_cm4.h
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
 * Purpose:		Definitions common to all ARM Cortex M4 processors
 *
 * Notes:
 * Edited definition of main(), now returns int. 7 Apr 14 KEL
 */

#ifndef _CPU_ARM_CM4_H
#define _CPU_ARM_CM4_H

#include "common.h"

#define LED_ON()  BITBAND_REG(GPIOC_PDOR, 5) = 1
#define LED_OFF() BITBAND_REG(GPIOC_PDOR, 5) = 0

/*ARM Cortex M4 implementation for interrupt priority shift*/
#define ARM_INTERRUPT_LEVEL_BITS          4

/*Determines the correct IRQ number from a INT value */
#define IRQ(N) (N - (1 << ARM_INTERRUPT_LEVEL_BITS))

/*Sets the priority of an interrupt*/
#define NVIC_SET_PRIORITY(irqnum, priority)  (*((volatile uint8_t *)0xE000E400 + (irqnum)) = (uint8_t)(priority))

/***********************************************************************/
// function prototypes for arm_cm4.c
void stop (void);
void wait (void);
void write_vtor (int);
void enable_irq (int);
void disable_irq (int);
void set_irq_priority (int, int);

/***********************************************************************/
  /*!< Macro to enable all interrupts. */

/*
static inline void EnableInterrupts(void)
{
    __asm__ __volatile__( "CPSIE i");
}
*/
#define EnableInterrupts(x) asm(" CPSIE i")

  /*!< Macro to disable all interrupts. */

/*
static inline void DisableInterrupts(void)
{
    __asm__ __volatile__("CPSID i");
}
*/

#define DisableInterrupts(x) asm(" CPSID i");
/***********************************************************************/

/*
 * The basic data types
 */

#include <stdint.h>

/***********************************************************************/

/*
 *  Prototype for main()
 */
extern int main(void);


#endif	/* _CPU_ARM_CM4_H */
