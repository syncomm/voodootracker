/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	      - Main User Interface Handling -			 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/
/*
 * The Real SoundTracker - main user interface handling
 *
 * Copyright (C) 1998-1999 Michael Krause
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include <unistd.h>

#include "poll.h"

#include <gdk/gdkkeysyms.h>
#include "gtkspinbutton.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <gnome.h>

#include "gui.h"
#include "gui-subs.h"
#include "xm.h"
#include "st-subs.h"
#include "audio.h"
#include "xm-player.h"
#include "tracker.h"
#include "main.h"
#include "notekeys.h"
#include "instrument-editor.h"
#include "sample-editor.h"
#include "track-editor.h"
#include "scope-group.h"
#include "module-info.h"
#include "preferences.h"

#define SCOPES_ON_BY_DEFAULT 1

int gui_playing_mode = 0;
GtkWidget *editing_toggle, *curins_spin, *spin_octave;
GtkWidget *gui_curins_name, *gui_cursmpl_name, *songname;

/* Track Operations */
static XMPattern *pattern_buffer = NULL;
static XMNote *track_buffer = NULL;
static int track_buffer_length;

static GSList *text_entries;

static gint pipetag = -1;
static GtkWidget *window, *mainwindow_upper_hbox, *scopegroup, *samplerbox;
static GtkWidget *notebook;
static GtkWidget *spin_songlength, *spin_songpos, *spin_songpat, *spin_restartpos;
static GtkWidget *spin_editpat, *spin_patlen, *spin_numchans;
static GtkWidget *programlist_ibutton, *programlist_dbutton;
static GtkWidget *cursmpl_spin;
static GtkAdjustment *adj_amplification, *adj_pitchbend;

static void tempo_slider_changed (int value);
static void bpm_slider_changed (int value);

static gui_subs_slider tempo_slider = {
    "Tempo", 1, 31, tempo_slider_changed
};
static gui_subs_slider bpm_slider = {
    " BPM ", 32, 255, bpm_slider_changed
};

//static GdkColor clipping_led_on, clipping_led_off;
//static GtkWidget *clipping_led;
//static GdkGC *clipping_led_gc = NULL;

static int set_songpos_count = 0, startstop_count = 0;
static double set_songpos_wait_time = -1.0;

static int editing_pat = 0;
static int songpos = 0;
static int capture_keys = 1;
static int notebook_current_page = 0;
static int gui_sampler_mode = 0;

/* gui event handlers */
static void quit(void);
static void quit_requested(void);
static void quit_requested_clicked(GnomeDialog * dialog, gint button_number, gpointer data);
static void load_clicked(GtkWidget *widget);
static void save_clicked(GtkWidget *widget);
static void backing_store_toggled(GtkWidget *widget);
static void scopes_toggled(GtkWidget *widget);
static void file_save_selected(GtkWidget *w, GtkFileSelection *fs);
static void file_load_selected(GtkWidget *w, GtkFileSelection *fs);
static void current_instrument_changed(GtkSpinButton *spin);
static void current_instrument_name_changed(void);
static void current_sample_changed(GtkSpinButton *spin);
static void current_sample_name_changed(void);
static int keyevent(GtkWidget *widget, GdkEventKey *event);
static void handle_standard_keys(int shift, int ctrl, int alt, guint32 keyval);
static void programlist_songpos_changed(GtkSpinButton *spin);
static void programlist_songlength_changed(GtkSpinButton *spin);
static void programlist_songpat_changed(GtkSpinButton *spin);
static void programlist_restartpos_changed(GtkSpinButton *spin);
static void spin_editpat_changed(GtkSpinButton *spin);
static void spin_patlen_changed(GtkSpinButton *spin);
static void spin_numchans_changed(GtkSpinButton *spin);
static void programlist_insert_clicked(void);
static void programlist_delete_clicked(void);
static void notebook_page_switched(GtkNotebook *notebook, GtkNotebookPage *page, int page_num);
static void clear_song_clicked(void);
static void clear_all_clicked(void);
static void gui_adj_amplification_changed(GtkAdjustment *adj);
static void gui_adj_pitchbend_changed(GtkAdjustment *adj);
static void gui_copy_pattern(void);
static void gui_cut_pattern(void);
static void gui_paste_pattern(void);
static void gui_copy_track(void);
static void gui_cut_track(void);
static void gui_paste_track(void);
static void about_dialog(void);

/* mixer / player communication */
static void read_mixer_pipe(gpointer data, gint source, GdkInputCondition condition);
static void wait_for_player(void);
static void play_song(void);
static void play_pattern(void);

static void init_xm(int new_xm);
static void free_xm(void);
static void new_xm(void);
static void load_xm(const char *filename);

/* gui initialization / helpers */
static void text_entry_selected(void);
static void text_entry_unselected(void);

static void gui_enable(int enable);
static void change_current_pattern(int p);
static void offset_current_pattern(int offset);
static void offset_current_instrument(int offset);
static void offset_current_sample(int offset);
static void programlist_initialize(void);
static void programlist_create(GtkWidget *destbox);

static void
gui_mixer_play_song (int sp,
		     int pp)
{
    audio_ctlpipe_id i = AUDIO_CTLPIPE_PLAY_SONG;
    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &sp, sizeof(sp));
    write(audio_ctlpipe, &pp, sizeof(pp));
}

static void
gui_mixer_play_pattern (int p,
			int pp)
{
    audio_ctlpipe_id i = AUDIO_CTLPIPE_PLAY_PATTERN;
    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &p, sizeof(p));
    write(audio_ctlpipe, &pp, sizeof(pp));
}

static void
gui_mixer_stop_playing (void)
{
    audio_ctlpipe_id i = AUDIO_CTLPIPE_STOP_PLAYING;
    write(audio_ctlpipe, &i, sizeof(i));
}

static void
gui_mixer_set_songpos (int songpos)
{
    audio_ctlpipe_id i = AUDIO_CTLPIPE_SET_SONGPOS;
    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &songpos, sizeof(songpos));
}

static void
gui_mixer_set_pattern (int pattern)
{
    audio_ctlpipe_id i = AUDIO_CTLPIPE_SET_PATTERN;
    write(audio_ctlpipe, &i, sizeof(i));
    write(audio_ctlpipe, &pattern, sizeof(pattern));
}

static void
gui_mixer_play_note (int channel,
		     int note,
		     int instrument)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_PLAY_NOTE;
    write(audio_ctlpipe, &a, sizeof(a));
    write(audio_ctlpipe, &channel, sizeof(channel));
    write(audio_ctlpipe, &note, sizeof(note));
    write(audio_ctlpipe, &instrument, sizeof(instrument));
}

static void
gui_enable_sampler_mode (int enable)
{
    void (*func)(GtkWidget*) = enable ? gtk_widget_hide : gtk_widget_show;
    
    func(notebook);
    func(mainwindow_upper_hbox);
    func(scopegroup);
    
    (enable ? gtk_widget_show : gtk_widget_hide)(samplerbox);
    
    gui_sampler_mode = enable;
}

static void
quit (void)
{
	gtk_main_quit();
}

static void
quit_requested (void)
{
    GtkWidget *popup, *label;
    popup = gnome_dialog_new(_("Quit?"), GNOME_STOCK_BUTTON_OK,
    			GNOME_STOCK_BUTTON_CANCEL,
    			NULL);
    label = gtk_label_new ("All Data Will Be Lost!");
	gtk_signal_connect(GTK_OBJECT(popup), "clicked",
                     GTK_SIGNAL_FUNC(quit_requested_clicked),
                     NULL);
   	gtk_box_pack_start(GNOME_DIALOG(popup)->vbox, label, TRUE, TRUE, GNOME_PAD);
	gtk_window_set_position (popup, GTK_WIN_POS_CENTER);
	gtk_widget_show (label);
	gtk_widget_show (popup);
}


void quit_requested_clicked (GnomeDialog * dialog, gint button_number,
                                     gpointer data)
{
    switch (button_number) {
	    case 0: /* OK button */
    		quit();
    		break;
    	case 1: /* Cancel Button */
			break;
		default:
			g_assert_not_reached();
	};
	gnome_dialog_close(dialog);
}

static void
load_clicked (GtkWidget *widget)
{
    gtk_widget_show(file_selection_create("Load Module...", file_load_selected));
}

static void
save_clicked (GtkWidget *widget)
{
    gtk_widget_show(file_selection_create("Save Module...", file_save_selected));
}

static void
backing_store_toggled (GtkWidget *widget)
{
    tracker_set_backing_store(tracker, GTK_TOGGLE_BUTTON(widget)->active);
}

static void
scopes_toggled (GtkWidget *widget)
{
    scope_group_enable_scopes(GTK_TOGGLE_BUTTON(widget)->active);
}

static void
file_load_selected (GtkWidget *w,
	       GtkFileSelection *fs)
{
    const gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

    gtk_widget_hide(GTK_WIDGET(fs));

    load_xm(fn);
}

static void
file_save_selected (GtkWidget *w,
	       GtkFileSelection *fs)
{
    const gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

    gtk_widget_hide(GTK_WIDGET(fs));

	XM_Save(xm, fn);
}

static void
current_instrument_changed (GtkSpinButton *spin)
{
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];
    STSample *s = &i->samples[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    gtk_entry_set_text(GTK_ENTRY(gui_curins_name), i->name);
    gtk_entry_set_text(GTK_ENTRY(gui_cursmpl_name), s->name);

    instrument_editor_set_instrument(i);
    sample_editor_set_sample(s);
    modinfo_set_current_instrument(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin)) - 1);
}

static void
current_instrument_name_changed (void)
{
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];

    strncpy(i->name, gtk_entry_get_text(GTK_ENTRY(gui_curins_name)), 22);
    i->name[22] = 0;
    modinfo_update_instrument(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1);
}

static void
current_sample_changed (GtkSpinButton *spin)
{
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];
    STSample *s = &i->samples[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    gtk_entry_set_text(GTK_ENTRY(gui_cursmpl_name), s->name);
    sample_editor_set_sample(s);
    modinfo_set_current_sample(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin)));
}

static void
current_sample_name_changed (void)
{
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];
    STSample *s = &i->samples[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    strncpy(s->name, gtk_entry_get_text(GTK_ENTRY(gui_cursmpl_name)), 22);
    s->name[22] = 0;
    modinfo_update_sample(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin)));
}

static int
keyevent (GtkWidget *widget,
	  GdkEventKey *event)
{
    static void(*handle_page_keys[])(int,int,int,guint32) = {
	tracker_page_handle_keys,
	instrument_page_handle_keys,
	sampler_page_handle_keys,
	modinfo_page_handle_keys,
	prefs_page_handle_keys
    };

    if(capture_keys && GTK_WIDGET_VISIBLE(notebook)) {
	int shift = event->state & GDK_SHIFT_MASK;
	int ctrl = event->state & GDK_CONTROL_MASK;
	int alt = event->state & GDK_MOD1_MASK;

	handle_standard_keys(shift, ctrl, alt, event->keyval);
	handle_page_keys[notebook_current_page](shift, ctrl, alt, event->keyval);
	
	gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
    } else {
	switch(event->keyval) {
	case GDK_Tab:
	case GDK_Return:
	    gtk_window_set_focus(GTK_WINDOW(window), NULL);
	    capture_keys = 1;
	    break;
	}
    }

    return 1;
}

static void
handle_standard_keys (int shift,
		      int ctrl,
		      int alt,
		      guint32 keyval)
{
    switch (keyval) {
    case GDK_F1 ... GDK_F7:
	if(!shift && !ctrl && !alt)
	    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_octave), keyval - GDK_F1);
	break;
    case GDK_Left:
	if(ctrl) {
	    /* previous instrument */
	    offset_current_instrument(shift ? -5 : -1);
	} else if(alt) {
	    /* previous pattern */
	    offset_current_pattern(shift ? -10 : -1);
	}
	break;
    case GDK_Right:
	if(ctrl) {
	    /* next instrument */
	    offset_current_instrument(shift ? 5 : 1);
	} else if(alt) {
	    /* next pattern */
	    offset_current_pattern(shift ? 10 : 1);
	}
	break;
    case GDK_Up:
	if(ctrl) {
	    /* next sample */
	    offset_current_sample(shift ? 4 : 1);
	}
	break;
    case GDK_Down:
	if(ctrl) {
	    /* previous sample */
	    offset_current_sample(shift ? -4 : -1);
	}
	break;
    case GDK_Alt_R:
    case GDK_Meta_R:
    case GDK_Super_R:
    case GDK_Hyper_R:
    case GDK_Mode_switch: /* well... this is X :D */
	play_pattern();
	break;
    case GDK_Control_R:
    case GDK_Multi_key:
	play_song();
	break;
    case ' ':
	if(!shift && !ctrl && !alt) {
	    if(!GUI_ENABLED) {
		gui_play_stop();
	    } else {
		gui_play_stop();
		/* toggle editing mode */
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(editing_toggle), !GUI_EDITING);
	    }
	}
	break;
    }
}

static void
programlist_songpos_changed (GtkSpinButton *spin)
{
    int newpos, pat;

    newpos = gtk_spin_button_get_value_as_int(spin);
    pat = xm->pattern_order_table[newpos];

    if(newpos == songpos)
	return;

    songpos = newpos;

    if(gui_playing_mode) {
	gui_mixer_set_songpos(newpos);
	set_songpos_count++;
	change_current_pattern(pat);
    }

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songpat), pat);
}

static void
programlist_songlength_changed (GtkSpinButton *spin)
{
    int newlen;

    newlen = gtk_spin_button_get_value_as_int(spin);

    gui_update_spin_adjustment(GTK_SPIN_BUTTON(spin_songpos), 0, newlen - 1);

    xm->song_length = newlen;
}

static void
programlist_songpat_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    if(n != xm->pattern_order_table[songpos]) {
	xm->pattern_order_table[songpos] = n;
    }
}

static void
programlist_restartpos_changed (GtkSpinButton *spin)
{
    xm->restart_position = gtk_spin_button_get_value_as_int(spin);
}

static void
spin_editpat_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    if(n != editing_pat)
	change_current_pattern(n);
}

static void
spin_patlen_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);
    XMPattern *pat = &xm->patterns[editing_pat];

    if(n != pat->length) {
	st_set_pattern_length(pat, n);
	tracker_set_pattern(tracker, NULL);
	tracker_set_pattern(tracker, pat);
    }
}

static void
spin_numchans_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    g_assert((n & 1) == 0);

    if(xm->num_channels != n) {
	gui_play_stop();
	tracker_set_pattern(tracker, NULL);
	st_set_num_channels(xm, n);
	init_xm(0);
    }
}

static void
tempo_slider_changed (int value)
{
    audio_ctlpipe_id i;
    xm->tempo = value;
    i = AUDIO_CTLPIPE_INIT_PLAYER;
    write(audio_ctlpipe, &i, sizeof(i));
}

static void
bpm_slider_changed (int value)
{
    audio_ctlpipe_id i;
    xm->bpm = value;
    i = AUDIO_CTLPIPE_INIT_PLAYER;
    write(audio_ctlpipe, &i, sizeof(i));
}

static void
programlist_insert_clicked (void)
{
    int pos;

    if(xm->song_length == 255)
	return;

    pos = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_songpos));
    memmove(&xm->pattern_order_table[pos+1], &xm->pattern_order_table[pos], 255 - pos);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songpat), xm->pattern_order_table[pos]);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songlength), ++xm->song_length);
}

static void
programlist_delete_clicked (void)
{
    int pos;

    if(xm->song_length == 1)
	return;

    pos = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_songpos));
    memmove(&xm->pattern_order_table[pos], &xm->pattern_order_table[pos+1], 255 - pos);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songpat), xm->pattern_order_table[pos]);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songlength), --xm->song_length);
}

static void
gui_adj_amplification_changed (GtkAdjustment *adj)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_SET_AMPLIFICATION;
    float b = 4.0 - adj->value;

    write(audio_ctlpipe, &a, sizeof(a));
    write(audio_ctlpipe, &b, sizeof(b));
}

static void
gui_adj_pitchbend_changed (GtkAdjustment *adj)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_SET_PITCHBEND;
    float b = adj->value;

    write(audio_ctlpipe, &a, sizeof(a));
    write(audio_ctlpipe, &b, sizeof(b));
}

static void
notebook_page_switched (GtkNotebook *notebook,
			GtkNotebookPage *page,
			int page_num)
{
    notebook_current_page = page_num;
}

static void
clear_song_clicked (void)
{
    gui_play_stop();
    st_clean_song(xm);
    init_xm(1);
}

static void
clear_all_clicked (void)
{
    free_xm();
    new_xm();
}

void
gui_update_spin_adjustment (GtkSpinButton *spin,
			    int min,
			    int max)
{
    GtkAdjustment* adj;
    int p;

    adj = gtk_spin_button_get_adjustment(spin);
    p = adj->value;
    if(p < min)
	p = min;
    else if(p > max)
	p = max;

    /* p+1 and the extra set_value is required due to a bug in gtk 1.0.4 */
    adj = GTK_ADJUSTMENT(gtk_adjustment_new(p+1, min, max, 1, 1, 0));
    gtk_spin_button_set_adjustment(spin, adj);
    gtk_adjustment_set_value(adj, p);
}

void
gui_update_player_pos (double songtime,
		       int nsongpos,
		       int npatpos)
{
    /* This test prevents feedback which occurs when manually changing the song position;
       if the audio thread doesn't follow immediately, the song position would be set back
       to its old position some lines below... */
    if(set_songpos_count != 0 || (set_songpos_wait_time != -1.0 && songtime < set_songpos_wait_time))
	return;

    set_songpos_wait_time = -1.0;

    if(gui_playing_mode == PLAYING_SONG && nsongpos != songpos) {
	songpos = nsongpos;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songpos), songpos);
	change_current_pattern(xm->pattern_order_table[songpos]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songpat), xm->pattern_order_table[songpos]);
    }
    if(gui_playing_mode != PLAYING_NOTE) {
	tracker_set_patpos(tracker, npatpos);
    }

    if(notebook_current_page == 0) {
	gdk_flush(); /* X drawing accumulates otherwise and makes pattern scrolling rather non-realtime :) */
    }
}

static void
read_mixer_pipe (gpointer data,
		 gint source,
		 GdkInputCondition condition)
{
    audio_backpipe_id a;
    struct pollfd pfd = { source, POLLIN, 0 };
    int x;

  loop:
    if(poll(&pfd, 1, 0) <= 0)
	return;

    read(source, &a, sizeof(a));

    switch(a) {
    case AUDIO_BACKPIPE_PLAYING_STOPPED:
	if(startstop_count > 0) {
	    /* can be equal to zero when the audio subsystem decides to stop playing on its own. */
	    startstop_count--;
	}
	gui_playing_mode = 0;
	scope_group_stop_updating();
	tracker_stop_updating();
	gui_enable(1);
	break;

    case AUDIO_BACKPIPE_PLAYING_STARTED:
    case AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED:
	startstop_count--;
	gui_playing_mode = (a == AUDIO_BACKPIPE_PLAYING_STARTED) ? PLAYING_SONG : PLAYING_PATTERN;
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(editing_toggle), 0);
	gui_enable(0);
	scope_group_start_updating();
	tracker_start_updating();
	break;

    case AUDIO_BACKPIPE_PLAYING_NOTE_STARTED:
	startstop_count--;
	if(!gui_playing_mode) {
	    gui_playing_mode = PLAYING_NOTE;
	    scope_group_start_updating();
	}
	break;

    case AUDIO_BACKPIPE_DRIVER_OPEN_FAILED:
	startstop_count--;
	fprintf(stderr, "Can't start playing / sampling. Driver open failed.\n");
	break;

    case AUDIO_BACKPIPE_SONGPOS_CONFIRM:
	set_songpos_count--;
	readpipe(source, &set_songpos_wait_time, sizeof(set_songpos_wait_time));
	g_assert(set_songpos_count >= 0);
	break;

    case AUDIO_BACKPIPE_SAMPLING_STARTED:
	startstop_count--;
	gui_enable_sampler_mode(1);
	sample_editor_sampling_mode_activated();
	break;

    case AUDIO_BACKPIPE_SAMPLING_STOPPED:
	startstop_count--;
	gui_enable_sampler_mode(0);
	sample_editor_sampling_mode_deactivated();
	break;

    case AUDIO_BACKPIPE_SAMPLING_TICK:
	readpipe(source, &x, sizeof(x));
	sample_editor_sampling_tick(x);
	break;

    default:
	fprintf(stderr, "\n\n*** read_mixer_pipe: unexpected backpipe id %d\n\n\n", a);
	break;
    }

    goto loop;
}

static void
wait_for_player (void)
{
    struct pollfd pfd = { audio_backpipe, POLLIN, 0 };

    startstop_count++;
    while(startstop_count != 0) {
	g_return_if_fail(poll(&pfd, 1, -1) > 0);
	read_mixer_pipe(NULL, audio_backpipe, 0);
    }
}

static void
play_song (void)
{
    gui_play_stop();
    gui_mixer_play_song(songpos, 0);
    change_current_pattern(xm->pattern_order_table[songpos]);
    wait_for_player();
}

static void
play_pattern (void)
{
    gui_play_stop();
    gui_mixer_play_pattern(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)), 0);
    wait_for_player();
}

void
gui_play_stop (void)
{
    gui_mixer_stop_playing();
    wait_for_player();
}

void
gui_start_sampling (void)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_SAMPLE;
    gui_play_stop();
    write(audio_ctlpipe, &a, sizeof(a));
    wait_for_player();
}

void
gui_stop_sampling (void)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_STOP_SAMPLING;
    write(audio_ctlpipe, &a, sizeof(a));
    wait_for_player();
}

static void
init_xm (int new_xm)
{
    audio_ctlpipe_id i;

    i = AUDIO_CTLPIPE_INIT_PLAYER;
    write(audio_ctlpipe, &i, sizeof(i));
    tracker_reset(tracker);
    if(new_xm) {
	programlist_initialize();
	editing_pat = -1;
	change_current_pattern(xm->pattern_order_table[0]);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(curins_spin), 1);
	current_instrument_changed(GTK_SPIN_BUTTON(curins_spin));
	modinfo_set_current_instrument(0);
	modinfo_set_current_sample(0);
	modinfo_update_all();
    } else {
	i = editing_pat;
	editing_pat = -1;
	change_current_pattern(i);
    }
    gtk_adjustment_set_value(tempo_slider.adjustment1, xm->tempo);
    gtk_adjustment_set_value(bpm_slider.adjustment1, xm->bpm);
    tracker_set_num_channels(tracker, xm->num_channels);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_numchans), xm->num_channels);
    scope_group_set_num_channels(xm->num_channels);
}

static void
free_xm (void)
{
    gui_play_stop();
    instrument_editor_set_instrument(NULL);
    sample_editor_set_sample(NULL);
    tracker_set_pattern(tracker, NULL);
    XM_Free(xm);
}

static void
new_xm (void)
{
    xm = XM_New();

    if(!xm) {
	fprintf(stderr, "Whooops, having memory problems?\n");
	exit(1);
    }
    init_xm(1);
}

static void
load_xm (const char *filename)
{
    const char *err;

    free_xm();
    xm = XM_Load(filename, &err);

    if(!xm) {
	fprintf(stderr, "Error while loading: %s\n", err);
	new_xm();
    } else
	init_xm(1);
}

void
gui_play_note (int channel, guint gdkkey)
{
    int n;

    if(-1 == (n = key2note(gdkkey)))
	return;

    gui_mixer_play_note(channel,
			n + 12 * gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_octave)) + 1,
			gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin)));
    startstop_count++;
}

static void
gui_enable (int enable)
{
    gtk_widget_set_sensitive(vscrollbar, enable);
    gtk_widget_set_sensitive(editing_toggle, enable);
    gtk_widget_set_sensitive(spin_patlen, enable);
    gtk_widget_set_sensitive(spin_songlength, enable);
    gtk_widget_set_sensitive(spin_songpat, enable);
    gtk_widget_set_sensitive(spin_restartpos, enable);
    gtk_widget_set_sensitive(programlist_ibutton, enable);
    gtk_widget_set_sensitive(programlist_dbutton, enable);
}

static void
change_current_pattern (int p)
{
    if(editing_pat == p)
	return;

    editing_pat = p;
    tracker_set_pattern(tracker, &xm->patterns[p]);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_patlen), xm->patterns[p].length);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_editpat), p);
    if(!GUI_ENABLED)
	gui_mixer_set_pattern(p);
}

static void
offset_current_pattern (int offset)
{
    int nv;

    nv = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)) + offset;

    if(nv < 0)
	nv = 0;
    else if(nv > 255)
	nv = 255;

    change_current_pattern(nv);
}

void
gui_set_current_instrument (int n)
{
    g_return_if_fail(n >= 1 && n <= 128);
    if(n != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin)));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(curins_spin), n);
}

void
gui_set_current_sample (int n)
{
    g_return_if_fail(n >= 0 && n <= 15);
    if(n != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin)));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cursmpl_spin), n);
}

static void
offset_current_instrument (int offset)
{
    int nv, v;

    v = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
    nv = v + offset;

    if(nv < 1)
	nv = 1;
    else if(nv > 128)
	nv = 128;

    if(v != nv)
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(curins_spin), nv);
}

static void
offset_current_sample (int offset)
{
    int nv, v;

    v = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin));
    nv = v + offset;

    if(nv < 0)
	nv = 0;
    else if(nv > 15)
	nv = 15;

    if(v != nv)
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(cursmpl_spin), nv);
}

static void
text_entry_selected (void)
{
    GSList *i;
    GtkWidget *f;

    if(capture_keys == 1) {
	for(i = text_entries, f = GTK_WINDOW(window)->focus_widget; i; i = g_slist_next(i)) {
	    if(f == i->data) {
		capture_keys = 0;
		break;
	    }
	}
    }
}

static void
text_entry_unselected (void)
{
    if(capture_keys == 0) {
	capture_keys = 1;
    }
}

void
gui_get_text_entry (int length,
		    void(*changedfunc)(),
		    GtkWidget **widget)
{
    GtkWidget *thing;

    thing = gtk_entry_new_with_max_length(length);
    gtk_signal_connect_after(GTK_OBJECT(thing), "focus_in_event",
			     GTK_SIGNAL_FUNC(text_entry_selected), NULL);
    gtk_signal_connect_after(GTK_OBJECT(thing), "focus_out_event",
			     GTK_SIGNAL_FUNC(text_entry_unselected), NULL);
    gtk_signal_connect_after(GTK_OBJECT(thing), "changed",
			     GTK_SIGNAL_FUNC(changedfunc), NULL);

    text_entries = g_slist_prepend(text_entries, thing);
    *widget = thing;
}

static void
programlist_initialize (void)
{
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songlength), xm->song_length);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songpos), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_restartpos), xm->restart_position);
    gui_update_spin_adjustment(GTK_SPIN_BUTTON(spin_songpos), 0, xm->song_length - 1);
    gui_update_spin_adjustment(GTK_SPIN_BUTTON(spin_restartpos), 0, xm->song_length - 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songpat), xm->pattern_order_table[0]);
}

static void
programlist_create (GtkWidget *destbox)
{
    GtkWidget *hbox, *vbox, *thing;

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(destbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    gui_put_labelled_spin_button(vbox, "Song length", 1, 256, &spin_songlength, programlist_songlength_changed);
    gui_put_labelled_spin_button(vbox, "Current pos", 0, 255, &spin_songpos, programlist_songpos_changed);
    gui_put_labelled_spin_button(vbox, "Pattern", 0, 255, &spin_songpat, programlist_songpat_changed);
    gui_put_labelled_spin_button(vbox, "Restart pos", 0, 255, &spin_restartpos, programlist_restartpos_changed);

    hbox = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_button_new_with_label("Insert");
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(programlist_insert_clicked), NULL);
    programlist_ibutton = thing;
    
    thing = gtk_button_new_with_label("Delete");
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(programlist_delete_clicked), NULL);
    programlist_dbutton = thing;
}

#if 0
static gint
gui_clipping_led_event (GtkWidget *thing, GdkEvent *event)
{
    if(!clipping_led_gc)
	clipping_led_gc = gdk_gc_new(thing->window);

    switch (event->type) {
    case GDK_MAP:
    case GDK_EXPOSE:
	gdk_gc_set_foreground(clipping_led_gc, mixer_clipping ? &clipping_led_on : &clipping_led_off);
	gdk_draw_rectangle(thing->window, clipping_led_gc, 1, 0, 0, -1, -1);
    default:
	break;
    }
    return 0;
}
#endif

static void
gui_copy_pattern (void)
{
	Tracker *t = tracker;
	XMPattern *p = t->curpattern;
	if(pattern_buffer) {
		st_free_pattern_channels(pattern_buffer);
		free(pattern_buffer);
	}
	pattern_buffer = st_dup_pattern(p);
	tracker_redraw(t);
};

static void
gui_cut_pattern (void)
{
	Tracker *t = tracker;
	XMPattern *p = t->curpattern;
	
	if(pattern_buffer) {
		    st_free_pattern_channels(pattern_buffer);
		    free(pattern_buffer);
	}
	pattern_buffer = st_dup_pattern(p);
	st_clear_pattern(p);
	tracker_redraw(t);
};

static void
gui_paste_pattern (void)
{
	int i;
	Tracker *t = tracker;
	XMPattern *p = t->curpattern;
	
	if(!pattern_buffer)
		    return;
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
};

static void
gui_copy_track (void)
{
	Tracker *t = tracker;
	int l = t->curpattern->length;
    XMNote *n = t->curpattern->channels[t->cursor_ch];

	if(track_buffer) {
	    free(track_buffer);
	}
	track_buffer_length = l;
	track_buffer = st_dup_track(n, l);
	tracker_redraw(t);
};

static void
gui_cut_track (void)
{
	Tracker *t = tracker;
	int l = t->curpattern->length;
    XMNote *n = t->curpattern->channels[t->cursor_ch];

    if(track_buffer) {
	    free(track_buffer);
	}
	track_buffer_length = l;
	track_buffer = st_dup_track(n, l);
	st_clear_track(n, l);
	tracker_redraw(t);	
};

static void
gui_paste_track (void)
{
	int i;
	Tracker *t = tracker;
	int l = t->curpattern->length;
    XMNote *n = t->curpattern->channels[t->cursor_ch];

    if(!track_buffer)
	    return;
	i = track_buffer_length;
	if(l < i)
	    i = l;
	while(i--)
	    n[i] = track_buffer[i];
	tracker_redraw(t);
};

static void
about_dialog (void)
{
	const gchar *authors[] = {"Greg S. Hayes", NULL};
	GtkWidget *about = gnome_about_new( _("Voodoo Tracker"), VERSION,
						_("Copyright FSF (c) 1999"),
						authors,
						"The Future of Digital Music",
						NULL);
	gtk_widget_show (about);
}
	
static GnomeUIInfo file_menu[] = {
    { GNOME_APP_UI_ITEM, N_("_New Module"), N_("Create a new blank module"), clear_all_clicked, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 'n', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Open Module..."), N_("Open an existing module"), load_clicked, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN, 'o', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Save Module"), N_("Save the current module"), NULL, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE, 's', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, N_("Save Module _as..."), N_("Save the current module with a new name"), save_clicked, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE_AS, 0, 0, NULL },

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, N_("_Print module..."), N_("Print the current module"), NULL, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 'p', GDK_CONTROL_MASK, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("Q_uit"), N_("Exit the program"), quit_requested, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 'q', GDK_CONTROL_MASK, NULL },

	GNOMEUIINFO_END
};

static GnomeUIInfo cut_menu[] = {
    { GNOME_APP_UI_ITEM, N_("Se_lection"), N_("Cut the selection to the clipboard"), NULL, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CUT, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("T_rack"), N_("Cut the track to the clipboard"), gui_cut_track, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CUT, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("P_attern"), N_("Cut the pattern to the clipboard"), gui_cut_pattern, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CUT, 0, 0, NULL },

	GNOMEUIINFO_END
};

static GnomeUIInfo copy_menu[] = {
    { GNOME_APP_UI_ITEM, N_("Se_lection"), N_("Copy the selection to the clipboard"), NULL, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_COPY, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("T_rack"), N_("Copy the track to the clipboard"), gui_copy_track, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_COPY, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("P_attern"), N_("Copy the pattern to the clipboard"), gui_copy_pattern, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_COPY, 0, 0, NULL },

	GNOMEUIINFO_END
};

static GnomeUIInfo paste_menu[] = {
    { GNOME_APP_UI_ITEM, N_("Se_lection"), N_("Paste the selection from the clipboard"), NULL, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("T_rack"), N_("Paste the track from the clipboard"), gui_paste_track, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("P_attern"), N_("Paste the pattern from the clipboard"), gui_paste_pattern, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE, 0, 0, NULL },

    GNOMEUIINFO_END
};

static GnomeUIInfo edit_menu[] = {
    GNOMEUIINFO_SUBTREE (N_("Cu_t"), cut_menu),
    GNOMEUIINFO_SUBTREE (N_("_Copy"), copy_menu),
    GNOMEUIINFO_SUBTREE (N_("_Paste"), paste_menu),

	GNOMEUIINFO_SEPARATOR,
	
	{ GNOME_APP_UI_ITEM, N_("Clear Song"), N_("Clear all patterns"), clear_song_clicked, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_UNDO, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("Clear All"), N_("Clear all patterns and instraments"), clear_all_clicked, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_UNDO, 0, 0, NULL },
	
	GNOMEUIINFO_END
};

static GnomeUIInfo help_menu[] = {
	{ GNOME_APP_UI_ITEM, N_("_About..."), N_("Information about Voodoo Tracker"), about_dialog, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT, 0, 0, NULL },

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_HELP ("Voodoo-Tracker"),

	GNOMEUIINFO_END
};

static GnomeUIInfo main_menu[] = {
	GNOMEUIINFO_SUBTREE (N_("_File"), file_menu),
    GNOMEUIINFO_SUBTREE (N_("_Edit"), edit_menu),
    GNOMEUIINFO_SUBTREE (N_("_Help"), help_menu),

    GNOMEUIINFO_END
};


static GnomeUIInfo toolbar[] = {
    { GNOME_APP_UI_ITEM, N_("Open..."), N_("Open an existing module"), load_clicked, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_OPEN, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("Save..."), N_("Save current module"), save_clicked, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_SAVE, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("Play"), N_("Play the current module"), play_song, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_FORWARD, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("Loop"), N_("Loop the current pattern"), play_pattern, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_REFRESH, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("Stop"), N_("Stop playing/looping"), gui_play_stop, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_STOP, 0, 0, NULL },

    GNOMEUIINFO_SEPARATOR,
	
	{ GNOME_APP_UI_ITEM, N_("Clear Song"), N_("Clear all patterns"), clear_song_clicked, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_UNDO, 0, 0, NULL },
    { GNOME_APP_UI_ITEM, N_("Clear All"), N_("Clear all patterns and instraments"), clear_all_clicked, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_TRASH, 0, 0, NULL },
	
    GNOMEUIINFO_SEPARATOR,

    { GNOME_APP_UI_ITEM, N_("Quit"), N_("Exit the program"), quit_requested, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_QUIT, 0, 0, NULL },
	
    GNOMEUIINFO_END
};	
int
gui_init (int argc,
	  char *argv[])
{
    GtkWidget *thing, *box2, *mainvbox, *table, *hbox, *dockitem, *handlebox;
//    GtkWidget *frame;
//    GdkColormap *colormap;

    gnome_init ("Voodoo Tracker", VERSION, argc, argv);
    
    pipetag = gdk_input_add(audio_backpipe, GDK_INPUT_READ, read_mixer_pipe, NULL);

    text_entries = g_slist_alloc();

    window = gnome_app_new ("VoodooTracker", "Voodoo Tracker " VERSION);
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			GTK_SIGNAL_FUNC (quit_requested), NULL);

    gnome_app_create_menus (GNOME_APP(window), main_menu);
    gnome_app_create_toolbar (GNOME_APP(window), toolbar);

    if(!init_notekeys()) {
	return 0;
    }

    mainvbox = gtk_vbox_new(FALSE, 10);
	gnome_app_set_contents(GNOME_APP(window), mainvbox);
    gtk_widget_show(mainvbox);
//    gtk_container_border_width (GTK_CONTAINER (mainvbox), 10);	
	
    /* The upper part of the window */
    dockitem = gnome_dock_item_new("Control Panel",(GNOME_DOCK_ITEM_BEH_EXCLUSIVE | GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL));
    gnome_app_add_dock_item(GNOME_APP(window), dockitem, GNOME_DOCK_TOP, 3, 3, 0);
    gtk_widget_show(dockitem);
    mainwindow_upper_hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_border_width(GTK_CONTAINER (mainwindow_upper_hbox), 10);
	gtk_container_add(dockitem, mainwindow_upper_hbox);
    gtk_widget_show(mainwindow_upper_hbox);

    /* Program List */
    programlist_create(mainwindow_upper_hbox);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    
    /* Load, Play Song, Stop etc. */
    box2 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), box2, TRUE, TRUE, 0);
    gtk_widget_show(box2);

    /* Misc settings */
    table = gtk_table_new(2, 5, FALSE);
    gtk_box_pack_start(GTK_BOX(box2), table, TRUE, TRUE, 0);
    gtk_widget_show(table);

    thing = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 1, 2);
    gui_subs_create_slider(GTK_BOX(thing), &tempo_slider);
    gtk_widget_show(thing);

    thing = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 0, 1);
    gui_subs_create_slider(GTK_BOX(thing), &bpm_slider);
    gtk_widget_show(thing);

    thing = gtk_label_new("Num. Chanels");
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 1, 2, 0, 1);
    gtk_widget_show(thing);

    spin_numchans = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(8, 2, 32, 2.0, 8.0, 0.0)), 0, 0);
    gtk_table_attach_defaults(GTK_TABLE(table), spin_numchans, 2, 3, 0, 1);
    gtk_signal_connect(GTK_OBJECT(spin_numchans), "changed",
		       GTK_SIGNAL_FUNC(spin_numchans_changed), NULL);
    gtk_widget_show(spin_numchans);

    thing = gtk_label_new("PatLength");
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 3, 4, 0, 1);
    gtk_widget_show(thing);

    spin_patlen = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(64, 1, 256, 1.0, 16.0, 0.0)), 0, 0);
    gtk_table_attach_defaults(GTK_TABLE(table), spin_patlen, 4, 5, 0, 1);
    gtk_signal_connect(GTK_OBJECT(spin_patlen), "changed",
		       GTK_SIGNAL_FUNC(spin_patlen_changed), NULL);
    gtk_widget_show(spin_patlen);

	thing = gtk_label_new("Octave");
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 1, 2, 1, 2);
    gtk_widget_show(thing);

    spin_octave = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(5.0, 0.0, 6.0, 1.0, 1.0, 0.0)), 0, 0);
    gtk_table_attach_defaults(GTK_TABLE(table), spin_octave, 2, 3, 1, 2);
    gtk_widget_show(spin_octave);

    thing = gtk_label_new("Pattern");
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 3, 4, 1, 2);
    gtk_widget_show(thing);

    spin_editpat = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 255, 1.0, 10.0, 0.0)), 0, 0);
    gtk_table_attach_defaults(GTK_TABLE(table), spin_editpat, 4, 5, 1, 2);
    gtk_widget_show(spin_editpat);
    gtk_signal_connect(GTK_OBJECT(spin_editpat), "changed",
	GTK_SIGNAL_FUNC(spin_editpat_changed), NULL);

    /* Editing status */
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box2), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_label_new("Editing:");
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    editing_toggle = gtk_check_button_new_with_label("On");
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(editing_toggle), 0);
    gtk_box_pack_start(GTK_BOX(hbox), editing_toggle, FALSE, TRUE, 0);
    gtk_widget_show(editing_toggle);

    thing = gtk_check_button_new_with_label("Doublebuffer");
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 1);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect (GTK_OBJECT(thing), "toggled",
			GTK_SIGNAL_FUNC(backing_store_toggled), NULL);

    thing = gtk_check_button_new_with_label("Scopes");
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), SCOPES_ON_BY_DEFAULT);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect (GTK_OBJECT(thing), "toggled",
			GTK_SIGNAL_FUNC(scopes_toggled), NULL);

	thing = gtk_label_new("Songname:");
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    gui_get_text_entry(20, songname_changed, &songname);
    gtk_box_pack_start(GTK_BOX(hbox), songname, TRUE, TRUE, 0);
    gtk_signal_connect_after(GTK_OBJECT(songname), "changed",
			     GTK_SIGNAL_FUNC(songname_changed), NULL);
    gtk_widget_show(songname);

    /* Current instrument display */
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box2), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_label_new("Instrument");
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    curins_spin = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(1.0, 1.0, 128.0, 1.0, 16.0, 0.0)), 0, 0);
    gtk_box_pack_start(GTK_BOX(hbox), curins_spin, FALSE, TRUE, 0);
    gtk_widget_show(curins_spin);
    gtk_signal_connect (GTK_OBJECT(curins_spin), "changed",
			GTK_SIGNAL_FUNC(current_instrument_changed), NULL);

    gui_get_text_entry(22, current_instrument_name_changed, &gui_curins_name);
    gtk_box_pack_start(GTK_BOX(hbox), gui_curins_name, TRUE, TRUE, 0);
    gtk_widget_show(gui_curins_name);

    thing = gtk_label_new("Sample");
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    cursmpl_spin = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 15.0, 1.0, 4.0, 0.0)), 0, 0);
    gtk_box_pack_start(GTK_BOX(hbox), cursmpl_spin, FALSE, TRUE, 0);
    gtk_widget_show(cursmpl_spin);
    gtk_signal_connect (GTK_OBJECT(cursmpl_spin), "changed",
			GTK_SIGNAL_FUNC(current_sample_changed), NULL);

    gui_get_text_entry(22, current_sample_name_changed, &gui_cursmpl_name);
    gtk_box_pack_start(GTK_BOX(hbox), gui_cursmpl_name, TRUE, TRUE, 0);
    gtk_widget_show(gui_cursmpl_name);

    /* Amplification slider and clipping indicator */
    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    
    box2 = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), box2, FALSE, TRUE, 0);

#if 0
    frame = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(box2), frame, FALSE, TRUE, 0);
    gtk_widget_show(frame);

    clipping_led = thing = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA (thing), 20, 10);
    gtk_widget_set_events(thing, GDK_EXPOSURE_MASK);
    gtk_container_add (GTK_CONTAINER(frame), thing);
    colormap = gtk_widget_get_colormap(thing);
    clipping_led_on.red = 0xEEEE;
    clipping_led_on.green = 0;
    clipping_led_on.blue = 0;
    clipping_led_on.pixel = 0;
    clipping_led_off.red = 0;
    clipping_led_off.green = 0;
    clipping_led_off.blue = 0;
    clipping_led_off.pixel = 0;
    gdk_color_alloc(colormap, &clipping_led_on);
    gdk_color_alloc(colormap, &clipping_led_off);
    gtk_signal_connect(GTK_OBJECT(thing), "event", GTK_SIGNAL_FUNC(gui_clipping_led_event), thing);
    gtk_widget_show (thing);
#endif
    adj_amplification = GTK_ADJUSTMENT(gtk_adjustment_new(3.0, 0, 4.0, 0.1, 0.1, 0.1));
    thing = gtk_vscale_new(adj_amplification);
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_signal_connect (GTK_OBJECT(adj_amplification), "value_changed",
			GTK_SIGNAL_FUNC(gui_adj_amplification_changed), NULL);
    thing = gtk_label_new("vol");
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    adj_pitchbend = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, -20.0, +20.0, 1, 1, 1));
    thing = gtk_vscale_new(adj_pitchbend);
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_signal_connect (GTK_OBJECT(adj_pitchbend), "value_changed",
			GTK_SIGNAL_FUNC(gui_adj_pitchbend_changed), NULL);

    /* Oscilloscopes */
//    thing = gtk_hbox_new(TRUE, 0);
    dockitem = gnome_dock_item_new("Oscilloscopes",(GNOME_DOCK_ITEM_BEH_EXCLUSIVE | GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL));
    gnome_app_add_dock_item(GNOME_APP(window), dockitem, GNOME_DOCK_TOP, 4, 4, 0);
//    gtk_container_add(GTK_CONTAINER(dockitem), thing);
//    gtk_widget_show(thing);
    gtk_widget_show(dockitem);
    scopegroup = scope_group_create(0);
//    gtk_box_pack_start(GTK_BOX(thing), scopegroup, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(dockitem), scopegroup);
    gtk_container_resize_children(GTK_CONTAINER(dockitem));
    gtk_widget_show(scopegroup);
    scope_group_enable_scopes(SCOPES_ON_BY_DEFAULT);

    /* The lower part (notebook display) */
    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(mainvbox), notebook, TRUE, TRUE, 0);
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_widget_show(notebook);
    gtk_container_border_width(GTK_CONTAINER(notebook), 0);
    gtk_signal_connect(GTK_OBJECT(notebook), "switch_page",
		       GTK_SIGNAL_FUNC(notebook_page_switched), NULL);

    tracker_page_create(GTK_NOTEBOOK(notebook));
    instrument_page_create(GTK_NOTEBOOK(notebook));
    sample_editor_page_create(GTK_NOTEBOOK(notebook));
    modinfo_page_create(GTK_NOTEBOOK(notebook));
    prefs_page_create(GTK_NOTEBOOK(notebook));

    samplerbox = sampler_sampling_page_create();
    gtk_box_pack_start(GTK_BOX(mainvbox), samplerbox, TRUE, TRUE, 0);

    /* capture all key presses */
    gtk_signal_connect(GTK_OBJECT(window), "key_press_event", GTK_SIGNAL_FUNC(keyevent), NULL);

    tracker_set_backing_store(tracker, 1);

    if(argc == 2) {
	load_xm(argv[1]);
    } else
	new_xm();

    gtk_widget_show (window);

    return 1;
}
