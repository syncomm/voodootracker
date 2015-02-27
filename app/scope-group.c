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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include "scope-group.h"
#include "sample-display.h"
#include "audio.h"

static GtkWidget *hbox;
static SampleDisplay *scopes[32];
static GtkWidget *scopebuttons[32];
static int numchan;

static int scopes_on = 0;
static int update_freq = 40;

static int gtktimer = -1;

static void
button_toggled (GtkWidget *widget,
		int n)
{
    int on = GTK_TOGGLE_BUTTON(widget)->active;

    player_mute_channels[n] = !on;
}

GtkWidget*
scope_group_create (int num_channels)
{
    GtkWidget *button, *box, *thing;
    int i;
    char buf[5];

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_set_spacing(GTK_BOX(hbox), 0);

    for(i = 0; i < 32; i++) {
	button = gtk_toggle_button_new();
	scopebuttons[i] = button;
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "toggled",
			   GTK_SIGNAL_FUNC(button_toggled), (void*)i);

	box = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(box);
	gtk_container_add(GTK_CONTAINER(button), box);

	thing = sample_display_new(FALSE);
	gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
	scopes[i] = SAMPLE_DISPLAY(thing);

	sprintf(buf, "%02d", i+1);
	thing = gtk_label_new(buf);
	gtk_widget_show(thing);
	gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
    }

    scope_group_set_num_channels(num_channels);

    return hbox;
}

void
scope_group_set_num_channels (int num_channels)
{
    int i;

    numchan = num_channels;
    
    for(i = 0; i < 32; i++) {
	(i < num_channels ? gtk_widget_show : gtk_widget_hide)(scopebuttons[i]);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(scopebuttons[i]), 1);
    }
}

void
scope_group_enable_scopes (int enable)
{
    int i;

    scopes_on = enable;

    for(i = 0; i < 32; i++) {
	(enable ? gtk_widget_show : gtk_widget_hide)(GTK_WIDGET(scopes[i]));
    }
}

int
scope_group_range_offset (int o)
{
    while(o < 0)
	o += scopebuf_len;
    return o % scopebuf_len;
}

gint
scope_group_timeout (gpointer data)
{
    struct timeval tv;
    double machtime;
    double display_songtime;
    const double scopebuf_time = (double)scopebuf_len / scopebuf_freq;
    int i, l;
    static gint8 *buf = NULL;
    static int bufsize = 0;
    int o1, o2;

    if(!scopes_on)
	return TRUE;

    gettimeofday(&tv, NULL);
    machtime = tv.tv_sec + (double)tv.tv_usec / 1e6;

    audio_lock(AUDIO_LOCK_PLAYER_TIME);
    display_songtime = audio_playerpos_songtime - scopebuftime + machtime - audio_playerpos_realtime - ((double)1 / update_freq);
    audio_unlock(AUDIO_LOCK_PLAYER_TIME);

    if(display_songtime < -scopebuf_time / 2 || display_songtime >= scopebuf_time) {
	goto ende;
    }

    o1 = display_songtime * scopebuf_freq;
    o2 = o1 + (scopebuf_freq / update_freq);

    if(o2 > scopebufpt)
	o2 = scopebufpt;

    if(o2 <= o1) {
	goto ende;
    }

    /* o1 is in [-scopebuf_len , scopebuf_len - 1],
       o2 is in [o1 + 1, scopebuf_len - 1] */

    l = o2 - o1;
    if(bufsize < l) {
	free(buf);
	buf = malloc(l);
	bufsize = l;
    }

    if(o1 < 0)
	o1 += scopebuf_len;
    if(o2 < 0)
	o2 += scopebuf_len;

    g_assert(o1 >= 0 && o1 <= scopebuf_len);
    g_assert(o2 >= 0 && o2 <= scopebuf_len);
    g_assert(o2 > o1 || scopebuf_len - o1 + o2 == l);

    for(i = 0; i < numchan; i++) {
	if(o2 > o1) {
	    sample_display_set_data_8(scopes[i], scopebufs[i] + o1, l, TRUE);
	} else {
	    memcpy(buf, scopebufs[i] + o1, scopebuf_len - o1);
	    memcpy(buf + scopebuf_len - o1, scopebufs[i], o2);
	    sample_display_set_data_8(scopes[i], buf, l, TRUE);
	}
    }
    return TRUE;

  ende:
    for(i = 0; i < numchan; i++) {
	sample_display_set_data_8(scopes[i], NULL, 0, FALSE);
    }
    return TRUE;
}

void
scope_group_start_updating (void)
{
    if(!scopes_on || gtktimer != -1)
	return;

    gtktimer = gtk_timeout_add(1000/update_freq, scope_group_timeout, NULL);
}

void
scope_group_stop_updating (void)
{
    int i;

    if(!scopes_on || gtktimer == -1)
	return;

    gtk_timeout_remove(gtktimer);
    gtktimer = -1;

    for(i = 0; i < numchan; i++) {
	sample_display_set_data_8(scopes[i], NULL, 0, FALSE);
    }
}

void
scope_group_set_update_freq (int freq)
{
    update_freq = freq;
    if(scopes_on && gtktimer != -1) {
	scope_group_stop_updating();
	scope_group_start_updating();
    }
}
