/*
 * wait.c
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

#include "wait.h"
#include "arm_cm4.h"
/*
 * wait on timer 1.
 */
void wait_us(uint32_t us)
{
    volatile uint32_t ticks = PIT_CVAL0;
    us /= 26;      /* a single tick is (roughly) 104 µs */

    do
    {
    } while (PIT_CVAL0 > ticks - us);
}

void wait_ns(uint32_t ns)
{
    volatile int i;

    for (i = 0; i < ns; i++)
    {
        wait_us(1000);
    }
}

void wait_ms(uint32_t ms)
{
    volatile int i;

    for (i = 0; i < ms; i++)
    {
        wait_ns(1000);
    }
}
