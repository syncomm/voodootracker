/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *		   - Endian Conversion -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _ENDIAN_CONV_H
#define _ENDIAN_CONV_H

#include <config.h>

static inline guint32
get_le_32 (guint8 *src)
{
#if defined(__i386__)
    return *(guint32*)src;
#else
    return (src[0] << 0) + (src[1] << 8) + (src[2] << 16) + (src[3] << 24);
#endif
}

static inline void
put_le_32 (guint8 *dest, gint32 d)
{
#if defined(__i386__)
    *(guint32*)dest = d;
#else
    dest[0] = d >> 0; dest[1] = d >> 8; dest[2] = d >> 16; dest[3] = d >> 24;
#endif
}

static inline guint16
get_le_16 (guint8 *src)
{
#if defined(__i386__)
    return *(guint16*)src;
#else
    return (src[0] << 0) + (src[1] << 8);
#endif
}

static inline void
put_le_16 (guint8 *dest, gint16 d)
{
#if defined(__i386__)
    *(guint16*)dest = d;
#else
    dest[0] = d >> 0; dest[1] = d >> 8;
#endif
}

static inline guint16
get_be_16 (guint8 *src)
{
    return (src[0] << 8) + (src[1] << 0);
}

void
byteswap_16_array (gint16 *data,
		   int count);

#endif /* _ENDIAN_CONV_H */


