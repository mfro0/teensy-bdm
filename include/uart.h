#ifndef UART_H
#define UART_H

/*
 * uart.h
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

extern void uart_init(UART_MemMapPtr uartch, int sysclk, int baud);
extern bool uart_char_present (UART_MemMapPtr channel);
extern uint8_t uart_receive(UART_MemMapPtr channel);
extern void uart_send(UART_MemMapPtr uartch, uint8_t c);

#endif /* UART_H */
