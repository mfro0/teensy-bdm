/*
 *
 * xstring.c
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
 * Copyright 2016        M. Fröschle
 *
 * Parts of this code are based on Kevin Cuzner's blog post "Teensy 3.1 bare metal: Writing a USB driver"
 * from http://kevincuzner.com. Many thanks for the groundwork!
 *
 */

#include <stdint.h>
#include "xstring.h"

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *to = dst;

    while (to < (uint8_t *) dst + n)
        *to++ = * (uint8_t *) src++;

    return dst;
}

void bzero(void *s, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
        ((unsigned char *) s)[i] = '\0';
}

void *memset(void *s, int c, size_t n)
{
    uint8_t *dst = s;

    do
    {
        *dst++ = c;
    } while ((dst - (uint8_t *) s) < n);

    return s;
}


int memcmp(const void *s1, const void *s2, size_t max)
{
    int i;
    int cmp = 0;

    for (i = 0; i < max; i++)
    {
        cmp = (* (const char *) s1 - * (const char *) s2);
        if (cmp != 0) return cmp;
    }
    return cmp;
}

int strcmp(const char *s1, const char *s2)
{
    int i;
    int cmp;

    for (i = 0; *s1++ && *s2++; i++)
    {
        cmp = (*s1 - *s2);
        if (cmp != 0) return cmp;
    }
    return cmp;
}

int strncmp(const char *s1, const char *s2, size_t max)
{
    int i;
    int cmp;

    for (i = 0; i < max && *s1++ && *s2++; i++);
    {
        cmp = (*s1 - *s2);
        if (cmp != 0) return cmp;
    }
    return cmp;
}

char *strcpy(char *dst, const char *src)
{
    char *ptr = dst;

    while ((*dst++ = *src++) != '\0');
    return ptr;
}

char *strncpy(char *dst, const char *src, size_t max)
{
    char *ptr = dst;

    while ((*dst++ = *src++) != '\0' && max-- >= 0);
    return ptr;
}

int atoi(const char *c)
{
    int value = 0;
    while (isdigit(*c))
    {
        value *= 10;
        value += (int) (*c - '0');
        c++;
    }
    return value;
}

size_t strlen(const char *s)
{
    const char *start = s;

    while (*s++);

    return s - start - 1;
}


char *strcat(char *dst, const char *src)
{
    char *ret = dst;
    dst = &dst[strlen(dst)];
    while ((*dst++ = *src++) != '\0');
    return ret;
}

char *strncat(char *dst, const char *src, size_t max)
{
    size_t i;
    char *ret = dst;

    dst = &dst[strlen(dst)];
    for (i = 0; i < max && *src; i++)
    {
        *dst++ = *src++;
    }
    *dst++ = '\0';

    return ret;
}
