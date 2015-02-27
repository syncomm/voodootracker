/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       	 - GTK+ Tracker Widget  -			 	 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _TRACKER_H
#define _TRACKER_H

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#include "xm.h"

#define TRACKER(obj)          GTK_CHECK_CAST (obj, tracker_get_type (), Tracker)
#define TRACKER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, tracker_get_type (), TrackerClass)
#define IS_TRACKER(obj)       GTK_CHECK_TYPE (obj, tracker_get_type ())

typedef struct _Tracker       Tracker;
typedef struct _TrackerClass  TrackerClass;

enum {
    TRACKERCOL_BG,
    TRACKERCOL_BGHI,
    TRACKERCOL_NOTES,
    TRACKERCOL_BARS,
    TRACKERCOL_CHANNUMS,
    TRACKERCOL_CURSOR,
    TRACKERCOL_LAST
};

struct _Tracker
{
    GtkWidget widget;

    int disp_rows;
    int disp_starty;
    int disp_numchans;
    int disp_startx;
    int disp_chanwidth;
    int disp_cursor;

    GdkFont *font;
    int fonth, fontw;
    GdkGC *bg_gc, *bghi_gc, *notes_gc, *misc_gc;
    GdkColor colors[TRACKERCOL_LAST];
    int enable_backing_store;
    GdkPixmap *pixmap;

    XMPattern *curpattern;
    int patpos, oldpos;
    int num_channels;

    int cursor_ch, cursor_item;
    int leftchan;
};

struct _TrackerClass
{
    GtkWidgetClass parent_class;

    void (*patpos)(Tracker *t, int patpos, int patlen, int disprows);
    void (*xpanning)(Tracker *t, int leftchan, int numchans, int dispchans);
};

guint          tracker_get_type            (void);
GtkWidget*     tracker_new                 (void);

void           tracker_set_num_channels    (Tracker *t, int);

void           tracker_set_backing_store   (Tracker *t, int on);
void           tracker_reset               (Tracker *t);
void           tracker_redraw              (Tracker *t);

void           tracker_set_patpos          (Tracker *t, int row);
void           tracker_note_typed_advance  (Tracker *t);

void           tracker_step_cursor_item    (Tracker *t, int direction);
void           tracker_step_cursor_channel (Tracker *t, int direction);

void           tracker_set_pattern         (Tracker *t, XMPattern *pattern);
void           tracker_set_xpanning        (Tracker *t, int left_channel);

#endif /* _TRACKER_H */






