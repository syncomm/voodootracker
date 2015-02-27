/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       	  - Oscilloscope Group -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _SCOPE_GROUP_H
#define _SCOPE_GROUP_H

#include <sys/time.h>
#include <unistd.h>

#include <gtk/gtk.h>

GtkWidget*       scope_group_create            (int num_channels);

void             scope_group_set_num_channels  (int);

void             scope_group_enable_scopes     (int enable);

void             scope_group_start_updating    (void);
void             scope_group_stop_updating     (void);
void             scope_group_set_update_freq   (int);

#endif /* _SCOPE_GROUP_H */
