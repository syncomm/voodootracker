/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *		- GTK+ Envelope Editor -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#include <string.h>

#include "gtkspinbutton.h"
#include <gtk/gtk.h>

//#include "gui.h"
#include "gui-subs.h"
#include "envelope-box.h"

void envelope_box_set_envelope(EnvelopeBox *e, STEnvelope *env)
{
    g_return_if_fail(e != NULL);
    g_return_if_fail(env != NULL);

    e->current = env;

    gtk_spin_button_set_value(e->spin_length, env->num_points);
    gtk_spin_button_set_value(e->spin_pos, 0);
    gtk_spin_button_set_value(e->spin_offset, env->points[0].pos);
    gtk_spin_button_set_value(e->spin_value, env->points[0].val);
    gtk_spin_button_set_value(e->spin_sustain, env->sustain_point);
    gtk_spin_button_set_value(e->spin_loop_start, env->loop_start);
    gtk_spin_button_set_value(e->spin_loop_end, env->loop_end);

    gtk_toggle_button_set_state(e->enable, env->flags & EF_ON);
    gtk_toggle_button_set_state(e->sustain, env->flags & EF_SUSTAIN);
    gtk_toggle_button_set_state(e->loop, env->flags & EF_LOOP);
}

static void handle_toggle_button(GtkToggleButton *t, EnvelopeBox *e)
{
    int flag = 0;

    if(t == e->enable)
	flag = EF_ON;
    else if(t == e->sustain)
	flag = EF_SUSTAIN;
    else if(t == e->loop)
	flag = EF_LOOP;

    g_return_if_fail(flag != 0);

    if(t->active)
	e->current->flags |= flag;
    else
	e->current->flags &= ~flag;
}

static void handle_spin_button(GtkSpinButton *s, EnvelopeBox *e)
{
    unsigned char *p = NULL;

    if(s == e->spin_sustain)
	p = &e->current->sustain_point;
    else if(s == e->spin_loop_start)
	p = &e->current->loop_start;
    else if(s == e->spin_loop_end)
	p = &e->current->loop_end;

    g_return_if_fail(p != NULL);

    *p = gtk_spin_button_get_value_as_int(s);
}

static void spin_length_changed(GtkSpinButton *spin, EnvelopeBox *e)
{
    e->current->num_points = gtk_spin_button_get_value_as_int(spin);
}

static void spin_pos_changed(GtkSpinButton *spin, EnvelopeBox *e)
{
    int p = gtk_spin_button_get_value_as_int(e->spin_pos);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[p].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[p].val);
}

static void spin_offset_changed(GtkSpinButton *spin, EnvelopeBox *e)
{
    int p = gtk_spin_button_get_value_as_int(e->spin_pos);
    e->current->points[p].pos = gtk_spin_button_get_value_as_int(spin);
}

static void spin_value_changed(GtkSpinButton *spin, EnvelopeBox *e)
{
    int p = gtk_spin_button_get_value_as_int(e->spin_pos);
    e->current->points[p].val = gtk_spin_button_get_value_as_int(spin);
}

static void insert_clicked(GtkWidget *w, EnvelopeBox *e)
{
    int pos;

    if(e->current->num_points == ST_MAX_ENVELOPE_POINTS)
	return;

    pos = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(e->spin_pos));
    memmove(&e->current->points[pos+1], &e->current->points[pos],
	    (ST_MAX_ENVELOPE_POINTS - 1 - pos) * sizeof(e->current->points[0]));

    gtk_spin_button_set_value(e->spin_offset, e->current->points[pos].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[pos].val);
    gtk_spin_button_set_value(e->spin_length, ++e->current->num_points);
}

static void delete_clicked(GtkWidget *w, EnvelopeBox *e)
{
    int pos;

    if(e->current->num_points == 1)
	return;

    pos = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(e->spin_pos));
    memmove(&e->current->points[pos], &e->current->points[pos+1],
	    (ST_MAX_ENVELOPE_POINTS - 1 - pos) * sizeof(e->current->points[0]));

    gtk_spin_button_set_value(e->spin_offset, e->current->points[pos].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[pos].val);
    gtk_spin_button_set_value(e->spin_length, --e->current->num_points);
}

static void envelope_box_init(EnvelopeBox *e)
{
}

static void envelope_box_class_init(EnvelopeBoxClass *class)
{
}

static void put_labelled_spin_button(GtkWidget *destbox, const char *title, int min, int max, GtkSpinButton **spin,
				     void(*callback)(), void *callbackdata)
{
    GtkWidget *hbox, *thing;

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(destbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_label_new(title);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(min, min, max, 1.0, 5.0, 0.0)), 0, 0);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "changed",
		       GTK_SIGNAL_FUNC(callback), callbackdata);

    *spin = GTK_SPIN_BUTTON(thing);
}

GtkWidget* envelope_box_new(const gchar *label)
{
    EnvelopeBox *e;
    GtkWidget *box2, *thing, *box3, *box4;

    e = gtk_type_new(envelope_box_get_type());
    GTK_BOX(e)->spacing = 2;
    GTK_BOX(e)->homogeneous = FALSE;

    box2 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(e), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    thing = gtk_check_button_new_with_label(label);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(handle_toggle_button), e);
    e->enable = GTK_TOGGLE_BUTTON(thing);

    add_empty_hbox(box2);

    box2 = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(e), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    /* Numerical list editing fields */
    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    put_labelled_spin_button(box3, "Env length", 1, 12, &e->spin_length, spin_length_changed, e);
    put_labelled_spin_button(box3, "Current pos", 0, 11, &e->spin_pos, spin_pos_changed, e);
    put_labelled_spin_button(box3, "Offset", 0, 65535, &e->spin_offset, spin_offset_changed, e);
    put_labelled_spin_button(box3, "Value", 0, 64, &e->spin_value, spin_value_changed, e);

    box4 = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(box3), box4, FALSE, TRUE, 0);
    gtk_widget_show(box4);

    thing = gtk_button_new_with_label("Insert");
    gtk_box_pack_start(GTK_BOX(box4), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(insert_clicked), e);
    
    thing = gtk_button_new_with_label("Delete");
    gtk_box_pack_start(GTK_BOX(box4), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(delete_clicked), e);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

#if 0    
    /* Graphical editing stuff to be inserted here */
    thing = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
#endif
    /* Sustain / Loop widgets */
    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    thing = gtk_check_button_new_with_label("Sustain");
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(handle_toggle_button), e);
    e->sustain = GTK_TOGGLE_BUTTON(thing);

    put_labelled_spin_button(box3, "Point", 0, 11, &e->spin_sustain, handle_spin_button, e);

    thing = gtk_check_button_new_with_label("Loop");
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "toggled",
		       GTK_SIGNAL_FUNC(handle_toggle_button), e);
    e->loop = GTK_TOGGLE_BUTTON(thing);

    put_labelled_spin_button(box3, "Start", 0, 11, &e->spin_loop_start, handle_spin_button, e);
    put_labelled_spin_button(box3, "End", 0, 11, &e->spin_loop_end, handle_spin_button, e);

    return GTK_WIDGET(e);
}

guint envelope_box_get_type()
{
    static guint envelope_box_type = 0;
    
    if (!envelope_box_type) {
	GtkTypeInfo envelope_box_info =
	{
	    "EnvelopeBox",
	    sizeof(EnvelopeBox),
	    sizeof(EnvelopeBoxClass),
	    (GtkClassInitFunc) envelope_box_class_init,
	    (GtkObjectInitFunc) envelope_box_init,
	    (GtkArgSetFunc) NULL,
	    (GtkArgGetFunc) NULL,
	};
	
	envelope_box_type = gtk_type_unique(gtk_vbox_get_type (), &envelope_box_info);
    }
    
    return envelope_box_type;
}


