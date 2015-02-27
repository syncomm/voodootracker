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

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gtk/gtksignal.h>

#include "tracker.h"
#include "main.h"

//#define XFONTNAME "-*-courier-medium-r-*-*-18-*-*-*-*-*-*-*"
//#define XFONTNAME "5x8"
#define XFONTNAME "7x13"

//#define xm ASDFASDF

static const char * const notenames[96] = {
    "C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "H-0",
    "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "H-1",
    "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "H-2",
    "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "H-3",
    "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "H-4",
    "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "H-5",
    "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6", "G#6", "A-6", "A#6", "H-6",
    "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "A-7", "A#7", "H-7",
};

static void init_display(Tracker *t, int width, int height);

static const int default_colors[] = {
    10, 20, 30,
    100, 100, 100,
    230, 230, 230,
    170, 170, 200,
    230, 200, 0,
    250, 100, 50
};

enum {
    SIG_PATPOS,
    SIG_XPANNING,
    LAST_SIGNAL
};

#define OLDPOS_MAGIC_REDRAW -6666

#define CLEAR(win, x, y, w, h) \
do { gdk_draw_rectangle(win, t->bg_gc, TRUE, x, y, w, h); } while(0)

static guint tracker_signals[LAST_SIGNAL] = { 0 };

void
tracker_set_num_channels (Tracker *t,
			  int n)
{
    GtkWidget *widget = GTK_WIDGET(t);

    t->num_channels = n;
    if(GTK_WIDGET_REALIZED(widget)) {
	init_display(t, widget->allocation.width, widget->allocation.height);
	gtk_widget_queue_draw(widget);
    }
    gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_XPANNING], t->leftchan, t->num_channels, t->disp_numchans);
}

void tracker_set_patpos(Tracker *t, int row)
{
    g_return_if_fail(t != NULL);
    g_return_if_fail((t->curpattern == NULL && row == 0) || (row < t->curpattern->length));

    if(t->patpos != row) {
	t->patpos = row;
	if(t->curpattern != NULL) {
	    gtk_widget_queue_draw(GTK_WIDGET(t));
	    gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_PATPOS], row, t->curpattern->length, t->disp_rows);
	}
    }
}

void tracker_redraw(Tracker *t)
{
    t->oldpos = OLDPOS_MAGIC_REDRAW;
    gtk_widget_queue_draw(GTK_WIDGET(t));
}

void tracker_note_typed_advance(Tracker *t)
{
    t->patpos++;
    if(t->patpos == t->curpattern->length)
	t->patpos = 0;
    gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_PATPOS], t->patpos, t->curpattern->length, t->disp_rows);
    tracker_redraw(t);
}

void tracker_set_pattern(Tracker *t, XMPattern *pattern)
{
    g_return_if_fail(t != NULL);

    if(t->curpattern != pattern) {
	t->curpattern = pattern;
	t->oldpos = OLDPOS_MAGIC_REDRAW;
	if(pattern != NULL) {
	    if(t->patpos >= pattern->length)
		t->patpos = pattern->length - 1;
	    gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_PATPOS], t->patpos, pattern->length, t->disp_rows);
	}
	gtk_widget_queue_draw(GTK_WIDGET(t));
    }
}

void tracker_set_backing_store(Tracker *t, int on)
{
    GtkWidget *widget;

    g_return_if_fail(t != NULL);

    if(on == t->enable_backing_store)
	return;

    t->enable_backing_store = on;

    widget = GTK_WIDGET(t);
    if(GTK_WIDGET_REALIZED(widget)) {
	t->oldpos = OLDPOS_MAGIC_REDRAW;
	gtk_widget_queue_draw(GTK_WIDGET(t));

	if(on) {
	    t->pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
	    CLEAR(t->pixmap, 0, 0, widget->allocation.width, widget->allocation.height);
	} else {
	    gdk_pixmap_unref(t->pixmap);
	    t->pixmap = NULL;
	}

	gdk_gc_set_exposures(t->bg_gc, !on);
    }
}

void tracker_set_xpanning(Tracker *t, int left_channel)
{
    GtkWidget *widget;

    g_return_if_fail(t != NULL);
	
    if(t->leftchan != left_channel) {
	widget = GTK_WIDGET(t);
	if(GTK_WIDGET_REALIZED(widget)) {
	    g_return_if_fail(left_channel + t->disp_numchans <= t->num_channels);

	    t->leftchan = left_channel;
	    t->oldpos = OLDPOS_MAGIC_REDRAW;
	    gtk_widget_queue_draw(GTK_WIDGET(t));
	    
	    if(t->cursor_ch < t->leftchan)
		t->cursor_ch = t->leftchan;
	    else if(t->cursor_ch >= t->leftchan + t->disp_numchans)
		t->cursor_ch = t->leftchan + t->disp_numchans - 1;
	}
	gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_XPANNING], t->leftchan, t->num_channels, t->disp_numchans);
    }
}

static void adjust_xpanning(Tracker *t)
{
    if(t->cursor_ch < t->leftchan)
	tracker_set_xpanning(t, t->cursor_ch);
    else if(t->cursor_ch >= t->leftchan + t->disp_numchans)
	tracker_set_xpanning(t, t->cursor_ch - t->disp_numchans + 1);
    else if(t->leftchan + t->disp_numchans > t->num_channels)
	tracker_set_xpanning(t, t->num_channels - t->disp_numchans);
}

void tracker_step_cursor_item(Tracker *t, int direction)
{
    g_return_if_fail(direction == -1 || direction == 1);
    
    t->cursor_item += direction;
    if(t->cursor_item & (~7)) {
	t->cursor_item &= 7;
	tracker_step_cursor_channel(t, direction);
    } else {
	adjust_xpanning(t);
	gtk_widget_queue_draw(GTK_WIDGET(t));
    }
}

void tracker_step_cursor_channel(Tracker *t, int direction)
{
    t->cursor_ch += direction;
    
    if(t->cursor_ch < 0)
	t->cursor_ch = t->num_channels - 1;
    else if(t->cursor_ch >= t->num_channels)
	t->cursor_ch = 0;

    adjust_xpanning(t);
    gtk_widget_queue_draw(GTK_WIDGET(t));
}

static void note2string(XMNote *note, char *buf)
{
    const char *b;

    if(note->note > 0 && note->note < 97) {
	b = notenames[note->note-1];
    } else if(note->note == 97) {
	b = "[-]";
    } else {
	b = "---";
    }

    sprintf(buf, "%s %02x %02x %c%02x  ",
	    b, note->instrument, note->volume,
	    note->fxtype >= 10 ? note->fxtype + 'a' - 10 : note->fxtype + '0',
	    note->fxparam);
}

static void print_notes_line(GtkWidget *widget, GdkDrawable *win, int y, int ch, int numch, int row)
{
    Tracker *t = TRACKER(widget);
    char buf[32*15];
    char *bufpt;

    g_return_if_fail(ch + numch <= t->num_channels);

    /* The row number */
    sprintf(buf, "%02x", row);

    if ( (row%4) != 0) {
     	gdk_gc_set_foreground(t->notes_gc, &t->colors[TRACKERCOL_NOTES]);
     	gdk_draw_string(win, t->font, t->notes_gc, 5, y, buf);
    } else {
    	gdk_gc_set_foreground(t->notes_gc, &t->colors[TRACKERCOL_CHANNUMS]);
    	gdk_draw_string(win, t->font, t->notes_gc, 5, y, buf);
    }

    /* The notes */
    for(numch += ch, bufpt = buf; ch < numch; ch++, bufpt += 15) {
	note2string(&t->curpattern->channels[ch][row], bufpt);
    }

    gdk_draw_string(win, t->font, t->notes_gc, t->disp_startx, y, buf);
}

static void print_notes_and_bars(GtkWidget *widget, GdkDrawable *win, int x, int y, int w, int h)
{
    int scry;
    int n, n1, n2;
    int my;
    Tracker *t = TRACKER(widget);
    int i, x1, y1;

    /* Limit y and h to the actually used window portion */
    my = y - t->disp_starty;
    if(my < 0) {
	my = 0;
	h += my;
    }
    if(my + h > t->fonth * t->disp_rows) {
	h = t->fonth * t->disp_rows - my;
    }

    /* Calculate first and last line to be redrawn */
    n1 = my / t->fonth;
    n2 = (my + h - 1) / t->fonth;

    /* Print the notes */
    scry = t->disp_starty + n1 * t->fonth + t->font->ascent;
    for(i = n1; i <= n2; i++, scry += t->fonth) {
	n = i + t->oldpos - t->disp_cursor;
	if(n >= 0 && n < t->curpattern->length) {
	    print_notes_line(widget, win, scry, t->leftchan, t->disp_numchans, n);
	}
    }

    /* Draw the separation bars */
    gdk_gc_set_foreground(t->misc_gc, &t->colors[TRACKERCOL_BARS]);
    x1 = t->disp_startx + t->disp_chanwidth - 2;
    y1 = t->disp_starty + n1 * t->fonth;
    h = (n2 - n1 + 1) * t->fonth;
    for(i = 1; i <= t->disp_numchans; i++, x1 += t->disp_chanwidth) {
	gdk_draw_line(win, t->misc_gc, x1, y1, x1, y1+h);
    }
}

static void print_channel_numbers(GtkWidget *widget, GdkDrawable *win)
{
    int x, y, i;
    char buf[4];
    Tracker *t = TRACKER(widget);

    gdk_gc_set_foreground(t->misc_gc, &t->colors[TRACKERCOL_CHANNUMS]);
    x = t->disp_startx + t->disp_chanwidth - (2 * t->fontw);
    y = t->disp_starty + t->font->ascent;
    for(i = 1; i <= t->disp_numchans; i++, x += t->disp_chanwidth) {
	sprintf(buf, "%2d", i + t->leftchan);
	gdk_draw_rectangle(win, t->bg_gc, TRUE, x, t->disp_starty, 2*t->fontw, t->fonth);
	gdk_draw_string(win, t->font, t->misc_gc, x, y, buf);
    }
}

static void print_cursor(GtkWidget *widget, GdkDrawable *win)
{
    int x, y, width;
    Tracker *t = TRACKER(widget);

    g_return_if_fail(t->cursor_ch >= t->leftchan && t->cursor_ch < t->leftchan + t->disp_numchans);
    g_return_if_fail((unsigned)t->cursor_item <= 7);

    width = 1;
    x = 0;
    y = t->disp_starty + t->disp_cursor * t->fonth;

    switch(t->cursor_item) {
    case 0: /* note */
	width = 3;
	break;
    case 1: /* instrument 0 */
	x = 4;
	break;
    case 2: /* instrument 1 */
	x = 5;
	break;
    case 3: /* volume 0 */
	x = 7;
	break;
    case 4: /* volume 1 */
	x = 8;
	break;
    case 5: /* effect 0 */
	x = 10;
	break;
    case 6: /* effect 0 */
	x = 11;
	break;
    case 7: /* effect 0 */
	x = 12;
	break;
    default:
	g_assert_not_reached();
	break;
    }

    x = x * t->fontw + t->disp_startx + (t->cursor_ch - t->leftchan) * t->disp_chanwidth - 1; 

    gdk_gc_set_foreground(t->misc_gc, &t->colors[TRACKERCOL_CURSOR]);
    gdk_draw_rectangle(win, t->misc_gc, FALSE, x, y, width * t->fontw, t->fonth - 1);
}

static void tracker_draw(GtkWidget *widget, GdkRectangle *area)
{
    Tracker *t;
    int dist, absdist;
    int y, ytemp, redrawcnt;
    GdkWindow *win;
    int fonth;

    g_return_if_fail(widget != NULL);

    if(!GTK_WIDGET_VISIBLE(widget))
	return;

    t = TRACKER(widget);
    g_return_if_fail(t->curpattern != NULL);

    win = t->enable_backing_store ? (GdkWindow*)t->pixmap : widget->window;
    fonth = t->fonth;

    dist = t->patpos - t->oldpos;
    absdist = ABS(dist);
    t->oldpos = t->patpos;

    redrawcnt = t->disp_rows;
    y = t->disp_starty;

    if(absdist < t->disp_rows) {
	/* this is interesting. we don't have to redraw the whole area, we can optimize
	   by scrolling around instead. */
	if(dist > 0) {
	    /* go down in pattern -- scroll up */
	    redrawcnt = absdist;
	    gdk_window_copy_area(win, t->bg_gc,
				 0, y, win, 0, y + (absdist * fonth),
				 widget->allocation.width, (t->disp_rows - absdist) * fonth);
	    y += (t->disp_rows - absdist) * fonth;
	} else if(dist < 0) {
	    /* go up in pattern -- scroll down */
	    redrawcnt = absdist + 1;
	    gdk_window_copy_area(win, t->bg_gc,
				 0, y + (absdist * fonth), win, 0, y,
				 widget->allocation.width, (t->disp_rows - absdist) * fonth);
	}
    }

    if(dist != 0) {
	if(absdist <= t->disp_cursor) {
	    ytemp = (t->disp_cursor - dist) * fonth + t->disp_starty;
	    CLEAR(win, 0, ytemp, widget->allocation.width, fonth);
	    print_notes_and_bars(widget, win, 0, ytemp, widget->allocation.width, fonth);
	}
	CLEAR(win, 0, y, widget->allocation.width, redrawcnt * fonth);
	print_notes_and_bars(widget, win, 0, y, widget->allocation.width, redrawcnt * fonth);
	print_channel_numbers(widget, win);
    }

    /* update the cursor */
    ytemp = t->disp_cursor * fonth + t->disp_starty;
    gdk_draw_rectangle(win, t->bghi_gc, TRUE, 0, ytemp, widget->allocation.width, fonth);
    print_notes_and_bars(widget, win, 0, ytemp, widget->allocation.width, fonth);
    print_cursor(widget, win);
    
    if(t->enable_backing_store) {
	gdk_draw_pixmap(widget->window, t->bg_gc, t->pixmap,
			area->x, area->y,
			area->x, area->y,
			area->width, area->height);
    }
}

static gint tracker_expose(GtkWidget *widget, GdkEventExpose *event)
{
    Tracker *t = TRACKER(widget);

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_TRACKER(widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    if(t->enable_backing_store) {
	tracker_draw(widget, &event->area);
    } else {
	if(!t->curpattern)
	    return FALSE;
	if(t->oldpos == OLDPOS_MAGIC_REDRAW) {
	    tracker_draw(widget, &event->area);
	} else {
	    CLEAR(widget->window, event->area.x, event->area.y, event->area.width, event->area.height);
	    print_notes_and_bars(widget, widget->window, event->area.x, event->area.y, event->area.width, event->area.height);
	    print_channel_numbers(widget, widget->window);
	    print_cursor(widget, widget->window);
	}
    }

    return FALSE;
}

static void tracker_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
    Tracker *t = TRACKER(widget);
    requisition->width = 15 * t->fontw + 3*t->fontw + 10;
    requisition->height = 11*t->fonth;
}

static void init_display(Tracker *t, int width, int height)
{
    GtkWidget *widget = GTK_WIDGET(t);
    int u;

    t->disp_rows = height / t->fonth;
    if(!(t->disp_rows % 2))
	t->disp_rows--;
    t->disp_cursor = t->disp_rows / 2;
    t->disp_starty = (height - t->fonth * t->disp_rows) / 2;

    t->disp_chanwidth = 15 * t->fontw;
    u = width - 3*t->fontw - 10; // + 2*t->fontw;
    t->disp_numchans = u / t->disp_chanwidth;
    
    g_return_if_fail(xm != NULL);
    if(t->disp_numchans > t->num_channels)
	t->disp_numchans = t->num_channels;

    t->disp_startx = (u - t->disp_numchans * t->disp_chanwidth) / 2 + 3*t->fontw + 5;
    t->oldpos = OLDPOS_MAGIC_REDRAW;
    adjust_xpanning(t);

    if(t->curpattern) {
	gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_PATPOS], t->patpos, t->curpattern->length, t->disp_rows);
	gtk_signal_emit(GTK_OBJECT(t), tracker_signals[SIG_XPANNING], t->leftchan, t->num_channels, t->disp_numchans);
    }
    
    if(t->enable_backing_store) {
	if(t->pixmap)
	    gdk_pixmap_unref(t->pixmap);
	t->pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
	CLEAR(t->pixmap, 0, 0, widget->allocation.width, widget->allocation.height);
    }
}

static void tracker_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    Tracker *t;
    
    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_TRACKER (widget));
    g_return_if_fail (allocation != NULL);

    widget->allocation = *allocation;
    if (GTK_WIDGET_REALIZED (widget)) {
	t = TRACKER (widget);
	
	gdk_window_move_resize (widget->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);

	init_display(t, allocation->width, allocation->height);
    }
}

void tracker_reset(Tracker *t)
{
    GtkWidget *widget;

    g_return_if_fail(t != NULL);

    widget = GTK_WIDGET(t);
    if(GTK_WIDGET_REALIZED(widget)) {
	init_display(t, widget->allocation.width, widget->allocation.height);
	t->oldpos = OLDPOS_MAGIC_REDRAW;
	t->patpos = 0;
	t->cursor_ch = 0;
	t->cursor_item = 0;
	t->leftchan = 0;
	adjust_xpanning(t);
	gtk_widget_queue_draw(GTK_WIDGET(t));
    }
}

static void init_colors(GtkWidget *widget)
{
    int n;
    const int *p;
    GdkColor *c;

    for (n = 0, p = default_colors, c = TRACKER(widget)->colors; n < TRACKERCOL_LAST; n++, c++) {
	c->red = *p++ * 65535 / 255;
	c->green = *p++ * 65535 / 255;
	c->blue = *p++ * 65535 / 255;
        c->pixel = (gulong)((c->red & 0xff00)*256 + (c->green & 0xff00) + (c->blue & 0xff00)/256);
        gdk_color_alloc(gtk_widget_get_colormap(widget), c);
    }
}

static void tracker_realize(GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;
    Tracker *t;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_TRACKER (widget));
    
    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    t = TRACKER(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
    attributes.visual = gtk_widget_get_visual (widget);
    attributes.colormap = gtk_widget_get_colormap (widget);
    
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);

    init_colors(widget);

    t->bg_gc = gdk_gc_new(widget->window);
    t->bghi_gc = gdk_gc_new(widget->window);
    t->notes_gc = gdk_gc_new(widget->window);
    t->misc_gc = gdk_gc_new(widget->window);
    gdk_gc_set_foreground(t->bg_gc, &t->colors[TRACKERCOL_BG]);
    gdk_gc_set_foreground(t->bghi_gc, &t->colors[TRACKERCOL_BGHI]);
    gdk_gc_set_foreground(t->notes_gc, &t->colors[TRACKERCOL_NOTES]);

    if(!t->enable_backing_store)
	gdk_gc_set_exposures (t->bg_gc, 1); /* man XCopyArea, grep exposures */

    init_display(t, attributes.width, attributes.height);

    gdk_window_set_user_data (widget->window, widget);
    gdk_window_set_background(widget->window, &t->colors[TRACKERCOL_BG]);
}

static void my_3ints_marshal(GtkObject *object, GtkSignalFunc func, gpointer func_data, GtkArg *args) {
    typedef void (*my_3ints_marshal_func)(Tracker *, int, int, int);
    my_3ints_marshal_func rfunc = (my_3ints_marshal_func) func;
    (*rfunc) (TRACKER(object),
	      GTK_VALUE_INT(args[0]),
	      GTK_VALUE_INT(args[1]),
	      GTK_VALUE_INT(args[2]));
}
  
static void tracker_class_init(TrackerClass *class)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass*) class;
    widget_class = (GtkWidgetClass*) class;
    
    widget_class->realize = tracker_realize;
    widget_class->expose_event = tracker_expose;
    widget_class->draw = tracker_draw;
    widget_class->size_request = tracker_size_request;
    widget_class->size_allocate = tracker_size_allocate;

    tracker_signals[SIG_PATPOS] = gtk_signal_new ("patpos",
						  GTK_RUN_FIRST,
						  object_class->type,
						  GTK_SIGNAL_OFFSET(TrackerClass, patpos),
						  my_3ints_marshal,
						  GTK_TYPE_NONE, 3,
						  GTK_TYPE_INT,
						  GTK_TYPE_INT,
						  GTK_TYPE_INT);
    tracker_signals[SIG_XPANNING] = gtk_signal_new ("xpanning",
						    GTK_RUN_FIRST,
						    object_class->type,
						    GTK_SIGNAL_OFFSET(TrackerClass, xpanning),
						    my_3ints_marshal,
						    GTK_TYPE_NONE, 3,
						    GTK_TYPE_INT,
						    GTK_TYPE_INT,
						    GTK_TYPE_INT);

    gtk_object_class_add_signals(object_class, tracker_signals, LAST_SIGNAL);
    
    class->patpos = NULL;
    class->xpanning = NULL;
}

static void tracker_init(Tracker *t)
{
    t->font = gdk_font_load(XFONTNAME);
    t->fonth = t->font->ascent + t->font->descent;
    t->fontw = gdk_string_width(t->font, "X"); /* let's just hope this is a non-proportional font */
    t->oldpos = -1;
    t->curpattern = NULL;
    t->enable_backing_store = 0;
    t->pixmap = NULL;
    t->patpos = 0;
    t->cursor_ch = 0;
    t->cursor_item = 0;
}

guint tracker_get_type()
{
    static guint tracker_type = 0;
    
    if (!tracker_type) {
	GtkTypeInfo tracker_info =
	{
	    "Tracker",
	    sizeof(Tracker),
	    sizeof(TrackerClass),
	    (GtkClassInitFunc) tracker_class_init,
	    (GtkObjectInitFunc) tracker_init,
	    (GtkArgSetFunc) NULL,
	    (GtkArgGetFunc) NULL,
	};
	
	tracker_type = gtk_type_unique (gtk_widget_get_type (), &tracker_info);
    }
    
    return tracker_type;
}

GtkWidget* tracker_new()
{
    return GTK_WIDGET(gtk_type_new(tracker_get_type()));
}
