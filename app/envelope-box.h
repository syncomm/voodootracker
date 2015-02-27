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

#ifndef _ENVELOPE_BOX_H
#define _ENVELOPE_BOX_H

#include <gdk/gdk.h>
#include <gtk/gtkframe.h>

#include "xm.h"

#define ENVELOPE_BOX(obj)          GTK_CHECK_CAST (obj, envelope_box_get_type (), EnvelopeBox)
#define ENVELOPE_BOX_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, envelope_box_get_type (), EnvelopeBoxClass)
#define IS_ENVELOPE_BOX(obj)       GTK_CHECK_TYPE (obj, envelope_box_get_type ())

typedef struct _EnvelopeBox       EnvelopeBox;
typedef struct _EnvelopeBoxClass  EnvelopeBoxClass;

struct _EnvelopeBox
{
    GtkVBox vbox;

    GtkSpinButton *spin_length;
    GtkSpinButton *spin_pos;
    GtkSpinButton *spin_offset;
    GtkSpinButton *spin_value;
    GtkSpinButton *spin_sustain;
    GtkSpinButton *spin_loop_start;
    GtkSpinButton *spin_loop_end;

    GtkToggleButton *enable;
    GtkToggleButton *sustain;
    GtkToggleButton *loop;

    STEnvelope *current;
};

struct _EnvelopeBoxClass
{
    GtkVBoxClass parent_class;
};

guint          envelope_box_get_type           (void);
GtkWidget*     envelope_box_new                (const gchar *label);

void           envelope_box_set_envelope       (EnvelopeBox *e, STEnvelope *env);

#endif /* _ENVELOPE_BOX_H */
