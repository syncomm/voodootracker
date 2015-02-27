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

#include <string.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include "gtkspinbutton.h"

#include "track-editor.h"
#include "gui.h"
#include "st-subs.h"
#include "notekeys.h"
#include "audio.h"

Tracker *tracker;
GtkWidget *vscrollbar;

static GtkWidget *hscrollbar;

static XMPattern *pattern_buffer = NULL;
static XMNote *track_buffer = NULL;
static int track_buffer_length;
static XMNote block_buffer[256];
static int block_start = -1, block_length;

static int update_freq = 30;
static int gtktimer = -1;

static void vscrollbar_changed(GtkAdjustment *adj);
static void hscrollbar_changed(GtkAdjustment *adj);
static void update_range_adjustment(GtkRange *range, int pos, int upper, int window, void(*func)());
static void update_vscrollbar(Tracker *t, int patpos, int patlen, int disprows);
static void update_hscrollbar(Tracker *t, int leftchan, int numchans, int dispchans);
static void insert_note(Tracker *t, int gdkkey);

void tracker_page_create(GtkNotebook *nb)
{
    GtkWidget *table, *thing;

    table = gtk_table_new(2, 2, FALSE);
    gtk_notebook_append_page(nb, table, gtk_label_new("Tracker"));
    gtk_container_border_width(GTK_CONTAINER(table), 10);
    gtk_widget_show(table);

    thing = tracker_new();
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 0, 1);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "patpos", GTK_SIGNAL_FUNC(update_vscrollbar), NULL);
    gtk_signal_connect(GTK_OBJECT(thing), "xpanning", GTK_SIGNAL_FUNC(update_hscrollbar), NULL);
    tracker = TRACKER(thing);

    hscrollbar = gtk_hscrollbar_new(NULL);
    gtk_table_attach(GTK_TABLE(table), hscrollbar, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
    gtk_widget_show(hscrollbar);

    vscrollbar = gtk_vscrollbar_new(NULL);
    gtk_table_attach(GTK_TABLE(table), vscrollbar, 1, 2, 0, 1, 0, GTK_FILL, 0, 0);
    gtk_widget_show(vscrollbar);
}

static void update_range_adjustment(GtkRange *range, int pos, int upper, int window, void(*func)())
{
    GtkAdjustment* adj;

    adj = gtk_range_get_adjustment(range);
    if(adj->upper != upper || adj->page_size != window) {
//#if GTK_MAJOR_VERSION < 1 || GTK_MINOR_VERSION < 1 || GTK_MICRO_VERSION < 3
	/* pos+1 and the extra set_value is required due to a bug in gtk 1.0.4 */
	adj = GTK_ADJUSTMENT(gtk_adjustment_new(pos+1, 0, upper, 1, window-2, window));
	gtk_range_set_adjustment(range, adj);
	gtk_adjustment_set_value(adj, pos);
//#else
//	adj = GTK_ADJUSTMENT(gtk_adjustment_new(pos, 0, upper, 1, window-2, window));
//	gtk_range_set_adjustment(range, adj);
//#endif
	gtk_signal_connect(GTK_OBJECT(adj), "value_changed", GTK_SIGNAL_FUNC(func), NULL);
    } else {
	if((int)(adj->value) != pos)
	    gtk_adjustment_set_value(adj, pos);
    }
}

static void update_vscrollbar(Tracker *t, int patpos, int patlen, int disprows)
{
    update_range_adjustment(GTK_RANGE(vscrollbar), patpos, patlen + disprows - 1, disprows, vscrollbar_changed);
}

static void update_hscrollbar(Tracker *t, int leftchan, int numchans, int dispchans)
{
    update_range_adjustment(GTK_RANGE(hscrollbar), leftchan, numchans, dispchans, hscrollbar_changed);
}

static void vscrollbar_changed(GtkAdjustment *adj)
{
    if(gui_playing_mode != PLAYING_SONG && gui_playing_mode != PLAYING_PATTERN) {
	gtk_signal_handler_block_by_func(GTK_OBJECT(tracker), GTK_SIGNAL_FUNC(update_vscrollbar), NULL);
	tracker_set_patpos(TRACKER(tracker), adj->value);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(tracker), GTK_SIGNAL_FUNC(update_vscrollbar), NULL);
    }
}

static void hscrollbar_changed(GtkAdjustment *adj)
{
    tracker_set_xpanning(TRACKER(tracker), adj->value);
}

void tracker_page_handle_keys(int shift, int ctrl, int alt, guint32 keyval)
{
    int i;
    Tracker *t = tracker;

    switch (keyval) {
    case GDK_Up:
	if(GUI_ENABLED && !ctrl && !shift && !alt)
	    tracker_set_patpos(t, t->patpos > 0 ? t->patpos - 1 : t->curpattern->length - 1);
	break;
    case GDK_Down:
	if(GUI_ENABLED && !ctrl && !shift && !alt)
	    tracker_set_patpos(t, t->patpos < t->curpattern->length - 1 ? t->patpos + 1 : 0);
	break;
    case GDK_Page_Up:
	if(!GUI_ENABLED)
	    break;
	if(t->patpos >= 16)
	    tracker_set_patpos(t, t->patpos - 16);
	else
	    tracker_set_patpos(t, 0);
	break;
    case GDK_Page_Down:
	if(!GUI_ENABLED)
	    break;
	if(t->patpos < t->curpattern->length - 16)
	    tracker_set_patpos(t, t->patpos + 16);
	else
	    tracker_set_patpos(t, t->curpattern->length - 1);
	break;
    case GDK_F3 ... GDK_F5:
	if(shift) {
	    /* track operations */
	    int l = t->curpattern->length;
	    XMNote *n = t->curpattern->channels[t->cursor_ch];

	    switch(keyval) {
	    case GDK_F3:
		/* cut track */
		if(track_buffer) {
		    free(track_buffer);
		}
		track_buffer_length = l;
		track_buffer = st_dup_track(n, l);
		st_clear_track(n, l);
		tracker_redraw(t);
		break;
	    case GDK_F4:
		/* copy track */
		if(track_buffer) {
		    free(track_buffer);
		}
		track_buffer_length = l;
		track_buffer = st_dup_track(n, l);
		tracker_redraw(t);
		break;
	    case GDK_F5:
		/* paste track */
		if(!track_buffer)
		    break;
		i = track_buffer_length;
		if(l < i)
		    i = l;
		while(i--)
		    n[i] = track_buffer[i];
		tracker_redraw(t);
		break;
	    }
	    break;
	} else if(alt) {
	    /* pattern operations */
	    XMPattern *p = t->curpattern;

	    switch(keyval) {
	    case GDK_F3:
		/* cut pattern */
		if(pattern_buffer) {
		    st_free_pattern_channels(pattern_buffer);
		    free(pattern_buffer);
		}
		pattern_buffer = st_dup_pattern(p);
		st_clear_pattern(p);
		tracker_redraw(t);
		break;
	    case GDK_F4:
		/* copy pattern */
		if(pattern_buffer) {
		    st_free_pattern_channels(pattern_buffer);
		    free(pattern_buffer);
		}
		pattern_buffer = st_dup_pattern(p);
		tracker_redraw(t);
		break;
	    case GDK_F5:
		/* paste pattern */
		if(!pattern_buffer)
		    break;
		for(i = 0; i < 32; i++) {
		    free(p->channels[i]);
		    p->channels[i] = st_dup_track(pattern_buffer->channels[i], pattern_buffer->length);
		}
		if(p->length != pattern_buffer->length) {
		    p->length = pattern_buffer->length;
		    tracker_reset(t);
		} else {
		    tracker_redraw(t);
		}
		break;
	    }
	    break;
	}
	break;
    case GDK_F9:
	if(GUI_ENABLED)
	    tracker_set_patpos(t, 0);
	break;
    case GDK_F10:
	if(GUI_ENABLED)
	    tracker_set_patpos(t, t->curpattern->length/4);
	break;
    case GDK_F11:
	if(GUI_ENABLED)
	    tracker_set_patpos(t, t->curpattern->length/2);
	break;
    case GDK_F12:
	if(GUI_ENABLED)
	    tracker_set_patpos(t, 3*t->curpattern->length/4);
	break;
    case GDK_Left:
	if(!shift && !ctrl && !alt) {
	    /* cursor left */
	    tracker_step_cursor_item(t, -1);
	}
	break;
    case GDK_Right:
	if(!shift && !ctrl && !alt) {
	    /* cursor right */
	    tracker_step_cursor_item(t, 1);
	}
	break;
    case GDK_Tab:
	tracker_step_cursor_channel(t, shift ? -1 : 1);
	break;
    case GDK_Shift_R:
	/* record pattern */
	break;
    case GDK_Delete:
    case GDK_BackSpace:
	if(GTK_TOGGLE_BUTTON(editing_toggle)->active) {
	    XMNote *note = &t->curpattern->channels[t->cursor_ch][t->patpos];
	    note->note = 0;
	    note->instrument = 0;
	    tracker_note_typed_advance(t);
	}
	break;
    default:
	if(!ctrl && !alt && !shift) {
	    if(t->cursor_item == 0)
		gui_play_note(t->cursor_ch, keyval);
	    if(GTK_TOGGLE_BUTTON(editing_toggle)->active)
		insert_note(t, keyval);
	} else if(ctrl && !alt && !shift) {
	    switch(keyval) {
	    case 'b':
		/* start marking a block */
		block_start = t->patpos;
		break;
	    case 'c':
		/* copy block */
		if(block_start >= t->curpattern->length)
		    break;
		block_length = t->patpos - block_start + 1;
		for(i = 0; i < block_length; i++) {
		    block_buffer[i] = t->curpattern->channels[t->cursor_ch][i + block_start];
		}
		break;
	    case 'x':
		/* cut block */
		if(block_start >= t->curpattern->length)
		    break;
		block_length = t->patpos - block_start + 1;
		for(i = 0; i < block_length; i++) {
		    block_buffer[i] = t->curpattern->channels[t->cursor_ch][i + block_start];
		    memset(&t->curpattern->channels[t->cursor_ch][i + block_start], 0, sizeof(XMNote));
		}
		tracker_redraw(t);
		break;
	    case 'v': case 'y':
		/* insert block */
		if(!GUI_ENABLED)
		    break;
		for(i = 0; i < block_length; i++) {
		    t->curpattern->channels[t->cursor_ch][(t->patpos+i)%t->curpattern->length] = block_buffer[i];
		}
		tracker_set_patpos(t, (t->patpos + block_length) % t->curpattern->length);
		tracker_redraw(t);
		break;
	    }
	    break;
	}
	break;
    }
}

static void insert_note(Tracker *t, int gdkkey)
{
    int n, i, s;
    XMNote *note = &t->curpattern->channels[t->cursor_ch][t->patpos];
    unsigned char *modpt;

    if(t->cursor_item == 0) {
	/* Note column */
	n = key2note(gdkkey);

	if(n == -1)
	    return;

	n += 12 * gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_octave)) + 1;
	    
	if(n > 96)
	    return;

	note->note = n;
	note->instrument = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
	tracker_note_typed_advance(t);
	return;
    }

    if(t->cursor_item == 5) {
	/* Effect column (not the parameter) */
	switch(gdkkey) {
	case '0' ... '9':
	    n = gdkkey - '0';
	    break;
	case 'a' ... 'f':
	case 'g': case 'h': case 'k': case 'l': case 'p': case 'r': case 't': case 'x':
	    n = gdkkey - 'a' + 10;
	    break;
	default:
	    return;
	}
	note->fxtype = n;
	tracker_note_typed_advance(t);	
	return;
    }

    if(!((gdkkey >= '0' && gdkkey <= '9') || (gdkkey >= 'a' && gdkkey <= 'f')))
	return;

    n = gdkkey - '0' - (gdkkey >= 'a') * ('a' - '9' - 1);

    switch(t->cursor_item) {
    case 1: case 2: /* instrument column */
	i = 2;
	modpt = &note->instrument;
	break;
    case 3: case 4: /* volume column */
	i = 4;
	modpt = &note->volume;
	break;
    case 6: case 7: /* effect parameter */
	i = 7;
	modpt = &note->fxparam;
	break;
    default:
	g_assert_not_reached();
	return;
    }

    i = (i - t->cursor_item) * 4;
    s = *modpt & (0xf0 >> i);
    s |= n << i;
    *modpt = s;
    tracker_note_typed_advance(t);	
}

gint
tracker_timeout (gpointer data)
{
    struct timeval tv;
    double machtime;
    double display_songtime;
    int i;

    gettimeofday(&tv, NULL);
    machtime = tv.tv_sec + (double)tv.tv_usec / 1e6;

    audio_lock(AUDIO_LOCK_PLAYER_TIME);
    display_songtime = audio_playerpos_songtime + machtime - audio_playerpos_realtime;
    audio_unlock(AUDIO_LOCK_PLAYER_TIME);

    for(i = audio_playerpos_start; i != audio_playerpos_end; i = (i + 1) % audio_playerpos_len) {
	if(audio_playerpos[i].time > display_songtime)
	    break;
    }

    audio_playerpos_start = i;

    if(i == audio_playerpos_end)
	return TRUE;

    gui_update_player_pos(audio_playerpos[i].time, audio_playerpos[i].songpos, audio_playerpos[i].patpos);

    return TRUE;
}

void
tracker_start_updating (void)
{
    if(gtktimer != -1)
	return;

    gtktimer = gtk_timeout_add(1000/update_freq, tracker_timeout, NULL);
}

void
tracker_stop_updating (void)
{
    if(gtktimer == -1)
	return;

    gtk_timeout_remove(gtktimer);
    gtktimer = -1;
}

void
tracker_set_update_freq (int freq)
{
    update_freq = freq;
    if(gtktimer != -1) {
	tracker_stop_updating();
	tracker_start_updating();
    }
}
