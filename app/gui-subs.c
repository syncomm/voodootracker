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

#include "gui-subs.h"

int
find_current_toggle (GtkWidget **widgets, int count)
{
    int i;
    for (i = 0; i < count; i++) {
	if(GTK_TOGGLE_BUTTON(*widgets++)->active) {
	    return i;
	}
    }
    return -1;
}

void
add_empty_hbox (GtkWidget *tobox)
{
    GtkWidget *thing = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (thing);
    gtk_box_pack_start (GTK_BOX (tobox), thing, TRUE, TRUE, 0);
}

void
make_radio_group (const char **labels, GtkWidget *tobox,
		  GtkWidget **saveptr, gint t1, gint t2,
		  void (*sigfunc) (void))
{
    GtkWidget *thing = NULL;

    while (*labels) {
	thing = gtk_radio_button_new_with_label ((thing
						  ? gtk_radio_button_group (GTK_RADIO_BUTTON (thing))
						  : 0),
						 *labels++);
	*saveptr++ = thing;
	gtk_widget_show (thing);
	gtk_box_pack_start (GTK_BOX (tobox), thing, t1, t2, 0);
	gtk_signal_connect (GTK_OBJECT (thing), "clicked", (GtkSignalFunc) sigfunc, NULL);
    }
}

GtkWidget*
make_labelled_radio_group_box (const char *title,
			       const char **labels,
			       GtkWidget **saveptr,
			       void (*sigfunc) (void))
{
    GtkWidget *box, *thing;

    box = gtk_hbox_new(FALSE, 4);
  
    thing = gtk_label_new(title);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
   
    make_radio_group(labels, box, saveptr, FALSE, TRUE, sigfunc);

    return box;
}

void
gui_put_labelled_spin_button (GtkWidget *destbox,
			      const char *title,
			      int min,
			      int max,
			      GtkWidget **spin,
			      void(*callback)())
{
    GtkWidget *hbox, *thing;

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(destbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_label_new(title);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    *spin = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(min, min, max, 1.0, 5.0, 0.0)), 0, 0);
    gtk_box_pack_start(GTK_BOX(hbox), *spin, FALSE, TRUE, 0);
    gtk_widget_show(*spin);
    gtk_signal_connect(GTK_OBJECT(*spin), "changed",
		       GTK_SIGNAL_FUNC(callback), NULL);
}

GtkWidget*
file_selection_create (const gchar *title,
		       void(*clickfunc)())
{
    GtkWidget *window;

    window = gtk_file_selection_new (title);
    gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);
	
    gtk_signal_connect(GTK_OBJECT (window), "destroy",
		       GTK_SIGNAL_FUNC(gtk_main_quit),
		       NULL);
    gtk_signal_connect(GTK_OBJECT (GTK_FILE_SELECTION (window)->ok_button),
		       "clicked", GTK_SIGNAL_FUNC(clickfunc),
		       window);
 
    gtk_signal_connect(GTK_OBJECT (window), "delete_event",
		       GTK_SIGNAL_FUNC(gtk_widget_hide),
		       window);
    gtk_signal_connect_object(GTK_OBJECT (GTK_FILE_SELECTION (window)->cancel_button),
			      "clicked", GTK_SIGNAL_FUNC(gtk_widget_hide),
			      GTK_OBJECT (window));

    return window;
}

static void
gui_subs_slider_update_1 (GtkWidget *w,
			  gui_subs_slider *s)
{
    int v = s->adjustment1->value;
    gtk_adjustment_set_value(s->adjustment2, v);
    s->changedfunc(v);
}

static void
gui_subs_slider_update_2 (GtkSpinButton *spin,
			  gui_subs_slider *s)
{
    int v = gtk_spin_button_get_value_as_int(spin);
    if(v != s->adjustment1->value) {
	/* the 'if' is only needed for gtk+-1.0 */
	gtk_adjustment_set_value(s->adjustment1, v);
    }
    s->changedfunc(v);
}

void
gui_subs_create_slider (GtkBox *destbox,
			gui_subs_slider *s)
{
    GtkWidget *thing, *box;

    box = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box);
    gtk_box_pack_start(destbox, box, FALSE, TRUE, 0);

    thing = gtk_label_new(s->title);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);

    s->adjustment1 = GTK_ADJUSTMENT(gtk_adjustment_new(s->min, s->min, s->max, 1, (s->max - s->min) / 10, 0));
    s->slider = gtk_hscale_new(s->adjustment1);
    gtk_scale_set_draw_value(GTK_SCALE(s->slider), FALSE);
    gtk_widget_show(s->slider);
    gtk_box_pack_start(GTK_BOX(box), s->slider, TRUE, TRUE, 0);
    gtk_signal_connect (GTK_OBJECT(s->adjustment1), "value_changed",
			GTK_SIGNAL_FUNC(gui_subs_slider_update_1), s);

    s->adjustment2 = GTK_ADJUSTMENT(gtk_adjustment_new(s->min, s->min, s->max, 1, (s->max - s->min) / 10, 0));
    thing = gtk_spin_button_new(s->adjustment2, 0, 0);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "changed",
		       GTK_SIGNAL_FUNC(gui_subs_slider_update_2), s);
}

