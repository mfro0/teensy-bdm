#ifndef USB_H
#define USB_H

/*
 * usb.h
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
/**
 * Header file for my implementation of using the USB peripheral
 * Kevin Cuzner
 */

#include "arm_cm4.h"

/**
 * Initializes the USB module
 */
void usb_init(void);

void usb_endp0_handler(uint8_t);
void usb_endp1_handler(uint8_t);
void usb_endp2_handler(uint8_t);
void usb_endp3_handler(uint8_t);
void usb_endp4_handler(uint8_t);
void usb_endp5_handler(uint8_t);
void usb_endp6_handler(uint8_t);
void usb_endp7_handler(uint8_t);
void usb_endp8_handler(uint8_t);
void usb_endp9_handler(uint8_t);
void usb_endp10_handler(uint8_t);
void usb_endp11_handler(uint8_t);
void usb_endp12_handler(uint8_t);
void usb_endp13_handler(uint8_t);
void usb_endp14_handler(uint8_t);
void usb_endp15_handler(uint8_t);

#endif // USB_H

