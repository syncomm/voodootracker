/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	     	 - Preferences Handling -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

/*
 * The Real SoundTracker - Preferences handling
 *
 * Copyright (C) 1998 Michael Krause
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

#include <stdio.h>

#include "gui-subs.h"
#include "preferences.h"
#include "scope-group.h"
#include "track-editor.h"

static const int mixfreqs[] = { 8000, 16000, 22050, 44100, -1 };

static GtkWidget *prefs_mode1_w[2];
static GtkWidget *prefs_resolution_w[2];
static GtkWidget *prefs_channels_w[2];
static GtkWidget *prefs_mixfreq_w[4];
static GtkWidget *prefs_fragsize_w[6];
static GtkWidget *prefs_audio_box;
static GtkWidget *prefs_use_from_other_w;
static int prefs_audio_current_mode;

static void           prefs_use_from_other_toggled              (void);
static void           prefs_scopesfreq_changed                  (int value);
static void           prefs_trackerfreq_changed                 (int value);

st_prefs st_preferences = {
    {
	{ 16, 1, 44100, 1024, 0 },
	{ 16, 2, 44100, 2048, 0 }
    }
};

static gui_subs_slider prefs_scopesfreq_slider = {
    "Scopes Frequency", 1, 50, prefs_scopesfreq_changed
};
static gui_subs_slider prefs_trackerfreq_slider = {
    "Tracker Frequency", 1, 50, prefs_trackerfreq_changed
};

static void
prefs_scopesfreq_changed (int value)
{
    scope_group_set_update_freq(value);
}

static void
prefs_trackerfreq_changed (int value)
{
    tracker_set_update_freq(value);
}

int
prefs_fragsize_log2 (int fragsize)
{
    int i;

    for(i = 0, fragsize >>= 10;
	fragsize != 0;
	i++) {
	fragsize >>= 1;
    }
    if(i == 6) {
	i = 2;
    }
    return i + 9;
}

static void
prefs_init_from_structure (int mode)
{
    int i, j;

    i = st_preferences.audio_playback[mode].use_from_other;
    gtk_signal_handler_block_by_func(GTK_OBJECT(prefs_use_from_other_w), GTK_SIGNAL_FUNC(prefs_use_from_other_toggled), NULL);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prefs_use_from_other_w), i);
    gtk_signal_handler_unblock_by_func(GTK_OBJECT(prefs_use_from_other_w), GTK_SIGNAL_FUNC(prefs_use_from_other_toggled), NULL);
    gtk_widget_set_sensitive(prefs_audio_box, !i);
    if(i) {
	return;
    }

    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prefs_resolution_w[st_preferences.audio_playback[mode].resolution / 8 - 1]), TRUE);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prefs_channels_w[st_preferences.audio_playback[mode].channels - 1]), TRUE);

    for(i = 0, j = st_preferences.audio_playback[mode].mixfreq;
	mixfreqs[i] != -1;
	i++) {
	if(j == mixfreqs[i])
	    break;
    }
    if(mixfreqs[i] == -1) {
	i = 3;
    }
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prefs_mixfreq_w[i]), TRUE);

    i = prefs_fragsize_log2(st_preferences.audio_playback[mode].fragsize) - 9;
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prefs_fragsize_w[i]), TRUE);
}

static void
prefs_mode1_changed (void)
{
    int n = find_current_toggle(prefs_mode1_w, 2);

    if(n != prefs_audio_current_mode) {
	prefs_audio_current_mode = n;
	prefs_init_from_structure(prefs_audio_current_mode);
    }
}

static void
prefs_copy_from_other_clicked (void)
{
    memcpy(&st_preferences.audio_playback[prefs_audio_current_mode],
	   &st_preferences.audio_playback[prefs_audio_current_mode ^ 1],
	   sizeof(st_preferences.audio_playback[0]));
    st_preferences.audio_playback[prefs_audio_current_mode].use_from_other = 0;
    prefs_init_from_structure(prefs_audio_current_mode);
}

static void
prefs_use_from_other_toggled (void)
{
    int u = GTK_TOGGLE_BUTTON(prefs_use_from_other_w)->active;
    st_preferences.audio_playback[prefs_audio_current_mode].use_from_other = u;
    if(!u) {
	prefs_init_from_structure(prefs_audio_current_mode);
    }
    if(u && st_preferences.audio_playback[prefs_audio_current_mode ^ 1].use_from_other) {
	/* not both at the same time */
	st_preferences.audio_playback[prefs_audio_current_mode ^ 1].use_from_other = 0;
    }
    gtk_widget_set_sensitive(prefs_audio_box, !u);
}

static void
prefs_resolution_changed (void)
{
    st_preferences.audio_playback[prefs_audio_current_mode].resolution = (find_current_toggle(prefs_resolution_w, 2) + 1) * 8;
}

static void
prefs_channels_changed (void)
{
    st_preferences.audio_playback[prefs_audio_current_mode].channels = find_current_toggle(prefs_channels_w, 2) + 1;
}

static void
prefs_mixfreq_changed (void)
{
    st_preferences.audio_playback[prefs_audio_current_mode].mixfreq = mixfreqs[find_current_toggle(prefs_mixfreq_w, 4)];
}

static void
prefs_fragsize_changed (void)
{
    st_preferences.audio_playback[prefs_audio_current_mode].fragsize = 1 << (find_current_toggle(prefs_fragsize_w, 6) + 9);
}

void
prefs_page_create (GtkNotebook *nb)
{
    GtkWidget *vbox, *frame, *box1, *box2, *thing;
    static const char *mode1labels[] = { "Editing", "Playing", NULL };
    static const char *resolutionlabels[] = { "8 bits", "16 bits", NULL };
    static const char *channelslabels[] = { "Mono", "Stereo", NULL };
    static const char *mixfreqlabels[] = { "8000", "16000", "22050", "44100", NULL };
    static const char *fragsizelabels[] = { "512", "1024", "2048", "4096", "8192", "16384", NULL };

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(vbox), 10);
    gtk_notebook_append_page(nb, vbox, gtk_label_new("Preferences"));
    gtk_widget_show(vbox);

    frame = gtk_frame_new("Audio Playback Parameters");
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, TRUE, 0);

    box1 = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(box1), 4);
    gtk_container_add(GTK_CONTAINER(frame), box1);
    gtk_widget_show(box1);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

    thing = make_labelled_radio_group_box("For mode:", mode1labels, prefs_mode1_w, prefs_mode1_changed);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prefs_mode1_w[prefs_audio_current_mode = 1]), TRUE);

    add_empty_hbox(box2);

    prefs_use_from_other_w = gtk_check_button_new_with_label("Use settings from other mode");
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prefs_use_from_other_w), 0);
    gtk_box_pack_start(GTK_BOX(box2), prefs_use_from_other_w, FALSE, TRUE, 0);
    gtk_widget_show(prefs_use_from_other_w);
    gtk_signal_connect (GTK_OBJECT(prefs_use_from_other_w), "toggled",
			GTK_SIGNAL_FUNC(prefs_use_from_other_toggled), NULL);

    thing = gtk_button_new_with_label("Copy settings from other mode");
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect (GTK_OBJECT(thing), "clicked",
			GTK_SIGNAL_FUNC(prefs_copy_from_other_clicked), NULL);

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);

    prefs_audio_box = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(prefs_audio_box);
    gtk_box_pack_start(GTK_BOX(box1), prefs_audio_box, FALSE, TRUE, 0);
   
    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(prefs_audio_box), box2, FALSE, TRUE, 0);

    add_empty_hbox(box2);
    thing = make_labelled_radio_group_box("Resolution:", resolutionlabels, prefs_resolution_w, prefs_resolution_changed);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    thing = make_labelled_radio_group_box("Channels:", channelslabels, prefs_channels_w, prefs_channels_changed);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(prefs_audio_box), box2, FALSE, TRUE, 0);

    add_empty_hbox(box2);
    thing = make_labelled_radio_group_box("Mixing Frequency [Hz]:", mixfreqlabels, prefs_mixfreq_w, prefs_mixfreq_changed);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(prefs_audio_box), box2, FALSE, TRUE, 0);

    add_empty_hbox(box2);
    thing = make_labelled_radio_group_box("Mixing Buffer Size [Samples]:", fragsizelabels, prefs_fragsize_w, prefs_fragsize_changed);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);

    prefs_init_from_structure(prefs_audio_current_mode);

    frame = gtk_frame_new("GUI Parameters");
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, TRUE, 0);

    box1 = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(box1), 4);
    gtk_container_add(GTK_CONTAINER(frame), box1);
    gtk_widget_show(box1);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

    add_empty_hbox(box2);
    gui_subs_create_slider(GTK_BOX(box2), &prefs_scopesfreq_slider);
    gui_subs_create_slider(GTK_BOX(box2), &prefs_trackerfreq_slider);
    add_empty_hbox(box2);

    gtk_adjustment_set_value(prefs_scopesfreq_slider.adjustment1, 40);
    gtk_adjustment_set_value(prefs_trackerfreq_slider.adjustment1, 30);
}

void
prefs_page_handle_keys (int shift,
			int ctrl,
			int alt,
			guint32 keyval)
{
}

