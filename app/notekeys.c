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

#include <stdio.h>
#include <gdk/gdkprivate.h>
#include "X11/Xlib.h"

#include "notekeys.h"

struct notekey notekeys[50];

enum { BLACK, WHITE };

static int min, max;
static KeySym *servsyms;
static int symspercode;
static struct notekey *notekeypt;

static int find_keysym(KeySym k)
{
    int i;

    for(i = 0; i <= max-min; i++) {
	if(servsyms[i * symspercode] == k)
	    return i;
    }

    return -1;
}

static void init_keys(KeySym first, int key_offset, int count, int note_offset, int color)
{
    int key;

    key = find_keysym(first);
    if(key == -1) {
	fprintf(stderr, "Key %c not found.\n", (char)first);
	return;
    }

    key += key_offset;

    while(count) {
	if(key < 0 || key > max-min) {
	    fprintf(stderr, "key out of range.\n");
	    return;
	}
	
	notekeypt->gdk_keysym = servsyms[key*symspercode];
	notekeypt->note_offset = note_offset;
	notekeypt++;

	if(color == WHITE) {
	    switch(note_offset % 12) {
	    case 4: case 11:
		note_offset += 1;
		break;
	    default:
		note_offset += 2;
		break;
	    }
	    key++;
	} else {
	    switch(note_offset % 12) {
	    case 3: case 10:
		note_offset += 3;
		key += 2;
		break;
	    default:
		note_offset += 2;
		key += 1;
		break;
	    }
	}

	count--;
    }
}

int init_notekeys(void)
{
    int result = 0;

    if(!gdk_display) {
	fprintf(stderr, "gdk_display is NULL.\n");
	return 0;
    }

    XDisplayKeycodes(gdk_display, &min, &max);
    if(min < 8 || max > 255) {
	fprintf(stderr, "Sorry, insane X keycode numbers (min/max out of range).\n");
	return 0;
    }

    servsyms = XGetKeyboardMapping(gdk_display, min, max-min+1, &symspercode);
    if(!servsyms) {
	fprintf(stderr, "Can't retrieve X keyboard mapping.\n");
	return 0;
    }
	
    if(symspercode > 4 || symspercode < 1) {
	fprintf(stderr, "Sorry, can't handle your X keyboard (symspercode out of range).\n");
    } else {
	notekeypt = notekeys;
	init_keys('e', -2, 12, 12, WHITE);
        init_keys('x', -1, 10, 0, WHITE);
	init_keys('2', 0, 8, 13, BLACK);
	init_keys('s', 0, 7, 1, BLACK);
	notekeypt->gdk_keysym = 0;
	result = 1;
    }
    
    XFree(servsyms);

    return result;
}

int key2note(guint32 keysym)
{
    struct notekey *nk;

    for(nk = notekeys; nk->gdk_keysym; nk++) {
	if(nk->gdk_keysym == keysym)
	    return nk->note_offset;
    }

    return -1;
}




