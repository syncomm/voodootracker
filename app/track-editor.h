/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       		 - Track Editor -				 	 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _TRACK_EDITOR_H
#define _TRACK_EDITOR_H

#include <sys/time.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include "xm.h"
#include "tracker.h"

extern Tracker *tracker;
extern GtkWidget *vscrollbar;

void      tracker_page_create              (GtkNotebook *nb);

void      tracker_page_handle_keys         (int shift,
					    int ctrl,
					    int alt,
					    guint32 keyval);

/* Handling of real-time scrolling */
void      tracker_start_updating      (void);
void      tracker_stop_updating       (void);
void      tracker_set_update_freq     (int);

#endif /* _TRACK_EDITOR_H */
