/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2010 Bruno Randolf (br1@einfach.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>
#include <endian.h>
#include <byteswap.h>

#if DO_DEBUG
#define DEBUG(...) printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
	#define le64toh(x) (x)
	#define le32toh(x) (x)
	#define le16toh(x) (x)
	#define htole64(x) (x)
	#define htole32(x) (x)
	#define htole16(x) (x)
#else
	#define le64toh(x) bswap_64(x)
	#define le32toh(x) bswap_32(x)
	#define le16toh(x) bswap_16(x)
	#define htole64(x) bswap_64(x)
	#define htole32(x) bswap_32(x)
	#define htole16(x) bswap_16(x)
#endif

void
dump_packet(const unsigned char* buf, int len);

const char*
ether_sprintf(const unsigned char *mac);

const char*
ether_sprintf_short(const unsigned char *mac);

const char*
ip_sprintf(const unsigned int ip);

const char*
ip_sprintf_short(const unsigned int ip);

void
convert_string_to_mac(const char* string, unsigned char* mac);

int
normalize(float val, int max_val, int max);

#define normalize_db(_val, _max) \
	normalize((_val) - 30, 70, (_max))

char
get_packet_type_char(int type);

const char*
get_packet_type_name(int type);

const char*
kilo_mega_ize(unsigned int val);

#define MAC_NOT_EMPTY(_mac) (_mac[0] || _mac[1] || _mac[2] || _mac[3] || _mac[4] || _mac[5])
#define MAC_EMPTY(_mac) (!_mac[0] && !_mac[1] && !_mac[2] && !_mac[3] && !_mac[4] && !_mac[5])

#define TOGGLE_BIT(_x, _m) (_x) ^= (_m)

#define max(x,y) ((x) > (y) ? (x) : (y))

/* Fixed Point IIR Averaging Filter:
 *
 * y(n) = 0.875*y(n-1) + 0.125*x(n)
 * y(n) = y(n-1) - 0.125*y(n-1) + 0.125*x(n)
 * 8*y(n) = 8*y(n-1) - 0.125*8*y(n-1) + x(n)
 * 8*y(n) = 8*y(n-1) - 8*y(n-1) >> 3 + x(n)
 *
 * We store y(n) in Q13.3 format to get higher precision, less round off error.
*/
#define iir_average(_avg, _val) \
	(_avg = _avg - (_avg >> 3) + _val)

#define iir_average_get(_avg) \
	(_avg >> 3)

#endif
