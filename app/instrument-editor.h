/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	      	   - Instrument Editor -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _INSTRUMENT_EDITOR_H
#define _INSTRUMENT_EDITOR_H

#include <gtk/gtk.h>

#include "xm.h"

void            instrument_page_create              (GtkNotebook *nb);
void            instrument_page_handle_keys         (int shift, int ctrl, int alt, guint32 keyval);

void            instrument_editor_set_instrument    (STInstrument *);
STInstrument*   instrument_editor_get_instrument    (void);
void            instrument_editor_update            (void);                     /* instrument data has changed, redraw */
void            instrument_editor_enable            (void);

#endif /* _INSTRUMENT_EDITOR_H */
