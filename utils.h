// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2009-2023 Cyril Hrubis <metan@ucw.cz>
 */

#ifndef UTILS_H__
#define UTILS_H__

#include <stdint.h>
#include <stdlib.h>

/*
 * Loads byte from buffer.
 */
static inline uint8_t load_byte(uint8_t **buf)
{
	return *(*buf)++;
}

/*
 * Loads word from big/little endian buffer.
 */
static inline uint16_t load_big_word(uint8_t **buf)
{
	*buf += 2;

	return (*buf)[-2]<<8 | (*buf)[-1];
}

uint16_t load_little_word(uint8_t **buf);

/*
 * Loads double word bit/little endian buffer.
 */
static inline uint32_t load_big_dword(uint8_t **buf)
{
	*buf += 4;

	return (*buf)[-4]<<24 | (*buf)[-3]<<16 | (*buf)[-2]<<8 | (*buf)[-1];
}

uint32_t load_little_dword(uint8_t **buf);

/*
 * Loads string of length len, return 0 on success.
 */
static inline void load_string(uint8_t **buf, char *str, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		(*buf)[i] = str[i];
	*buf[len] = 0;
	*buf += len;
}

#endif /* UTILS_H__ */
