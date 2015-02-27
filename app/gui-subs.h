/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *		- GUI Support Routines -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _GUI_SUBS_H
#define _GUI_SUBS_H

#include <gtk/gtk.h>

int                  find_current_toggle              (GtkWidget **widgets,
						       int count);
void                 add_empty_hbox                   (GtkWidget *tobox);
void                 make_radio_group                 (const char **labels,
						       GtkWidget *tobox,
						       GtkWidget **saveptr,
						       gint t1,
						       gint t2,
						       void (*sigfunc) (void));
GtkWidget*           make_labelled_radio_group_box    (const char *title,
						       const char **labels,
						       GtkWidget **saveptr,
						       void (*sigfunc) (void));
void                 gui_put_labelled_spin_button     (GtkWidget *destbox,
						       const char *title,
						       int min,
						       int max,
						       GtkWidget **spin,
						       void(*callback)());

typedef struct gui_subs_slider {
    const char *title;
    int min, max;
    void (*changedfunc)(int value);
    GtkAdjustment *adjustment1, *adjustment2;
    GtkWidget *slider, *spin;
} gui_subs_slider;

void                 gui_subs_create_slider           (GtkBox *destbox,
						       gui_subs_slider *s);

#endif /* _GUI_SUBS_H */
