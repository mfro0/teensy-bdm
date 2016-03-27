/*
 * start.h
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
 * Purpose:	Kinetis start up routines.
 *
 * Notes:
 * Modified from original Freescale distribution; removed CPU identify and
 * flash identify prototypes.  4 Apr 14 KEL
 */

#ifndef __START_H__
#define __START_H__

#include "common.h"

// Function prototypes
void start(void);


#endif /*__START_H__*/
