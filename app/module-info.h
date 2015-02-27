/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	      	   - Module Info Page -					 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _MODULE_INFO_H
#define _MODULE_INFO_H

#include <gtk/gtk.h>

void            modinfo_page_create                (GtkNotebook *nb);

void            modinfo_page_handle_keys           (int shift, int ctrl, int alt, guint32 keyval);

void            modinfo_update_instrument          (int instr);
void            modinfo_update_sample              (int sample);
void            modinfo_update_all                 (void);

void            modinfo_set_current_instrument     (int);
void            modinfo_set_current_sample         (int);

void 			songname_changed				   (GtkEntry *entry);

#endif /* _MODULE_INFO_H */
