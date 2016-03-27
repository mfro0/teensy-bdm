/*
 * bdm.c
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

#include "bdm.h"
#include "wait.h"
#include "xprintf.h"

#define DBG_BDM
#ifdef DBG_BDM
#define dbg(format, arg...) do { xprintf("DEBUG (%s()): " format, __FUNCTION__, ##arg);} while(0)
#else
#define dbg(format, arg...) do {;} while (0)
#endif /* DBG_BDM */
#define err(format, arg...) do { xprintf("ERROR (%s()): " format, __FUNCTION__, ##arg); } while(0)

/*
 * HALT target
 */
void bdmcf_halt(void)
{
    BKPT_LO();

    wait_us(10000);          /* assert /BKPT long enough for the target to halt */

    BKPT_HI();
}


/*
 * send and receive a BDM 17 bit message
 */
uint32_t bdmcf_txrx(uint32_t mess)
{
    uint32_t result = 0;
    int i;

    dbg("\r\n");


    for (i = 16; i >= 0; i--)
    {
        result <<= 1;

        DSCLK_LO();

        wait_us(1);

        if ((mess >> i) & 1)
        {
            DSO_HI();
        }
        else
        {
            DSO_LO();
        }

        /*
         * target should sample the bit here, while we sample theirs
         */
        result |= DSI();

        wait_us(1);

        DSCLK_HI();

        wait_us(1);
    }

    DSCLK_LO();
    DSO_LO();
    return result;
}

void bdmcf_ta(uint32_t time_ms)
{
    ;
}

void bdmcf_reset(void)
{
    ;
}
