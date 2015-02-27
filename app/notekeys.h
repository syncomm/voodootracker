/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	      	   - Note Key Mapping -					 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _NOTEKEYS_H
#define _NOTEKEYS_H

#include <gdk/gdktypes.h>

struct notekey {
    guint gdk_keysym;
    int note_offset;
};

extern struct notekey notekeys[];

extern int init_notekeys(void);
extern int key2note(guint32 keysym);

#endif /* _NOTEKEYS_H */
