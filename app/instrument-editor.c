/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r		 *
 *                                                       *
 *	      	   - Instrument Editor -		 *
 *							 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 Broukoid	Jul 9 2000	Added load/save FT2 instrument
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#include "instrument-editor.h"
#include "sample-editor.h"
#include "envelope-box.h"
#include "endian-conv.h"
#include "recode.h"

#include "xm.h"
#include "st-subs.h"
#include "gui.h"
#include "gui-subs.h"
#include <stdio.h>
#include <string.h>

static GtkWidget *volenv, *panenv, *mainbox, *submainbox, *saveinstbutt;
static GtkWidget *instrument_editor_vibtype_w[4];

static GtkWidget *loadwindow, *savewindow;


static STInstrument *current_instrument;


static void instrument_editor_load_save_clicked(GtkWidget *widget, GtkWidget *fs);
static void instrument_editor_load_xi(void);
static void instrument_editor_save_xi(void);

static void
instrument_editor_load_save_clicked (GtkWidget *widget, GtkWidget *fs)
{
    gtk_widget_show(fs);
}




static void
instrument_page_volfade_changed (int value)
{
    current_instrument->volfade = value;
}

static void
instrument_page_vibspeed_changed (int value)
{
    current_instrument->vibrate = value;
}

static void
instrument_page_vibdepth_changed (int value)
{
    current_instrument->vibdepth = value;
}

static void
instrument_page_vibsweep_changed (int value)
{
    current_instrument->vibsweep = value;
}

static gui_subs_slider instrument_page_sliders[] = {
    { "VolFade", 0, 0xfff, instrument_page_volfade_changed },
    { "VibSpeed", 0, 0x3f, instrument_page_vibspeed_changed },
    { "VibDepth", 0, 0xf, instrument_page_vibdepth_changed },
    { "VibSweep", 0, 0xff, instrument_page_vibsweep_changed },
};

static void
instrument_editor_vibtype_changed (void)
{
    current_instrument->vibtype = find_current_toggle(instrument_editor_vibtype_w, 4);
}

void
instrument_page_create (GtkNotebook *nb)
{
    GtkWidget *vbox, *thing, *box;
    static const char *vibtypelabels[] = { "Sine", "Square", "Saw Down", "Saw Up", NULL };

    mainbox = gtk_hbox_new(FALSE, 4);
    gtk_container_border_width(GTK_CONTAINER(mainbox), 10);
    gtk_notebook_append_page(nb, mainbox, gtk_label_new("Instrument Editor"));
    gtk_widget_show(mainbox);

    submainbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainbox), submainbox, TRUE, TRUE, 0);
    gtk_widget_show(submainbox);

    thing = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(submainbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    vbox = gtk_hbox_new(FALSE, 10);
    gtk_box_pack_start(GTK_BOX(submainbox), vbox, FALSE, TRUE, 0);
    gtk_widget_show(vbox);

    volenv = envelope_box_new("Volume envelope");
    gtk_box_pack_start(GTK_BOX(vbox), volenv, TRUE, TRUE, 0);
    gtk_widget_show(volenv);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    panenv = envelope_box_new("Panning envelope");
    gtk_box_pack_start(GTK_BOX(vbox), panenv, TRUE, TRUE, 0);
    gtk_widget_show(panenv);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(submainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    box = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(submainbox), box, FALSE, TRUE, 0);
    gtk_widget_show(box);

    add_empty_hbox(box);
    thing = make_labelled_radio_group_box("Vibrato Type:", vibtypelabels, instrument_editor_vibtype_w, instrument_editor_vibtype_changed);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
    gui_subs_create_slider(GTK_BOX(box), &instrument_page_sliders[0]);
    add_empty_hbox(box);

    box = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(submainbox), box, FALSE, TRUE, 0);
    gtk_widget_show(box);

    add_empty_hbox(box);
    gui_subs_create_slider(GTK_BOX(box), &instrument_page_sliders[1]);
    gui_subs_create_slider(GTK_BOX(box), &instrument_page_sliders[2]);
    gui_subs_create_slider(GTK_BOX(box), &instrument_page_sliders[3]);
    add_empty_hbox(box);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(submainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(submainbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(mainbox), vbox, TRUE, TRUE, 0);

    loadwindow = file_selection_create("Load instrument", instrument_editor_load_xi);
    savewindow = file_selection_create("Save instrument", instrument_editor_save_xi);

    thing = gtk_button_new_with_label("Load XI");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(instrument_editor_load_save_clicked), loadwindow);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label("Save XI");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(instrument_editor_load_save_clicked), savewindow);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
    gtk_widget_set_sensitive(thing, 0);
    gtk_widget_show(thing);
    saveinstbutt=thing;
}

static void
instrument_check_envelope(STEnvelope *env)
{
    int i;
    
    if (env->num_points>ST_MAX_ENVELOPE_POINTS || env->num_points==0) {
        env->num_points=0;
        env->sustain_point=0;
	env->loop_start=0;
	env->loop_end=0;
	env->flags=0;
        return;
    }
    for (i=0; i<env->num_points-1; i++) {
        if (env->points[i].pos>=env->points[i+1].pos) {
            env->sustain_point=0;
            env->loop_start=0;
            env->loop_end=0;
            env->flags=0;
            return;
        }
    }
    if (env->sustain_point>=env->num_points) {
        env->sustain_point=0;
	env->flags&=~EF_SUSTAIN;
    }
    if (env->loop_start>=env->num_points) {
	env->loop_start=0;
    }
    if (env->loop_end>=env->num_points) {
        env->loop_end=env->num_points-1;
    }
    if (env->loop_start>=env->loop_end) {
        env->loop_start=0;
	env->loop_end=env->num_points-1;
    }
}


static void
instrument_editor_load_xi (void)
{
    FILE *f;
    guint8 ih[298];
    const gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(loadwindow));
    int num_samples;

    fprintf(stderr, "\nLoading file '%s'...\n", fn);

    gtk_widget_hide(loadwindow);
    g_return_if_fail(current_instrument != NULL);
    st_clean_instrument(current_instrument, NULL);

    if(!(f = fopen(fn, "r"))) {
	fprintf(stderr, "Can't open '%s' for reading.\n", fn);
	return;
    }

    fread(ih, 1, sizeof(ih), f);
    if (feof(f) || memcmp("Extended Instrument:", ih, 20)) {
	fprintf(stderr, "File is not extended instrument (XI)\n");
        goto errnobuf;
    }

    strncpy(current_instrument->name, ih+20, 21);
    recode_ibmpc_to_latin1(current_instrument->name, 21);
    memcpy(current_instrument->samplemap, ih+66, 96);
    memcpy(current_instrument->vol_env.points, ih+162, 48);
    memcpy(current_instrument->pan_env.points, ih+210, 48);
    
    num_samples = get_le_16(ih + 296);

    if (num_samples) {
	current_instrument->vol_env.num_points = ih[258];
	current_instrument->vol_env.sustain_point = ih[260];
	current_instrument->vol_env.loop_start = ih[261];
	current_instrument->vol_env.loop_end = ih[262];
	current_instrument->vol_env.flags = ih[266];
	current_instrument->pan_env.num_points = ih[259];
	current_instrument->pan_env.sustain_point = ih[263];
	current_instrument->pan_env.loop_start = ih[264];
	current_instrument->pan_env.loop_end = ih[265];
	current_instrument->pan_env.flags = ih[267];

	current_instrument->vibtype = ih[268];
	if(current_instrument->vibtype >= 4) {
	    current_instrument->vibtype = 0;
	    fprintf(stderr, "Invalid vibtype %d, using Sine.\n", current_instrument->vibtype);
	}
	current_instrument->vibrate = ih[271];
	current_instrument->vibdepth = ih[270];
	current_instrument->vibsweep = ih[269];
	
	current_instrument->volfade = get_le_16(ih + 272);

	xm_load_xm_samples(current_instrument->samples, num_samples, f);
    }    

    instrument_check_envelope(&current_instrument->vol_env);
    instrument_check_envelope(&current_instrument->pan_env);
    
    gtk_widget_set_sensitive(saveinstbutt, 1);
    
    gui_play_stop();
    instrument_editor_enable();
    instrument_editor_update();
    sample_editor_update();
    
  errnobuf:
    fclose(f);
    return;
}

static void
instrument_editor_save_xi (void)
{
    const gchar *fn;
    FILE *f;
    guint8 header[298];
    int num_samples;


    gtk_widget_hide(savewindow);

    g_return_if_fail(current_instrument != NULL);

    fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(savewindow));
    f = fopen(fn, "w");
    if(!f) {
	fprintf(stderr, "Can't open '%s' for writing.\n", fn);
	return;
    }
    
    num_samples = st_instrument_num_save_samples(current_instrument);

    memset(header, 0, sizeof(header));
    strncpy(header, "Extended Instrument:", 20);
    strncpy(header + 21, current_instrument->name, 21);
    recode_latin1_to_ibmpc(header + 21, 21);
    put_le_16(header+296, num_samples);

    if(num_samples == 0) {
	fwrite(header, 1, sizeof(header), f);
        fclose(f);
	return;
    }

    memcpy(header+66, current_instrument->samplemap, 96);
    memcpy(header+162, current_instrument->vol_env.points, 48);
    memcpy(header+162, current_instrument->pan_env.points, 48);

    header[258] = current_instrument->vol_env.num_points;
    header[259] = current_instrument->pan_env.num_points;
    header[260] = current_instrument->vol_env.sustain_point;
    header[261] = current_instrument->vol_env.loop_start;
    header[262] = current_instrument->vol_env.loop_end;
    header[263] = current_instrument->pan_env.sustain_point;
    header[264] = current_instrument->pan_env.loop_start;
    header[265] = current_instrument->pan_env.loop_end;
    header[266] = current_instrument->vol_env.flags;
    header[267] = current_instrument->pan_env.flags;

    header[268] = current_instrument->vibtype;
    header[269] = current_instrument->vibsweep;
    header[270] = current_instrument->vibdepth;
    header[271] = current_instrument->vibrate;
	
    put_le_16(header + 272, current_instrument->volfade);

    fwrite(header, 1, sizeof(header), f);

    xm_save_xm_samples(current_instrument->samples, f, num_samples);

    fclose(f);
    return;
}



void
instrument_editor_enable (void)
{
    gtk_widget_set_sensitive(submainbox, 1);
    gtk_widget_set_sensitive(saveinstbutt, 1);
}

void
instrument_editor_set_instrument (STInstrument *i)
{
    current_instrument = i;

    if(!i || st_instrument_num_samples(i) == 0) {
	gtk_widget_set_sensitive(submainbox, 0);
        gtk_widget_set_sensitive(saveinstbutt, 0);
	return;
    }

    instrument_editor_enable();

    instrument_editor_update();
}

STInstrument*
instrument_editor_get_instrument (void)
{
    return current_instrument;
}

void
instrument_page_handle_keys (int shift,
			     int ctrl,
			     int alt,
			     guint32 keyval)
{
    if(!ctrl && !alt && !shift)
	gui_play_note(0, keyval);
}

void
instrument_editor_update (void)
{
    envelope_box_set_envelope(ENVELOPE_BOX(volenv), &current_instrument->vol_env);
    envelope_box_set_envelope(ENVELOPE_BOX(panenv), &current_instrument->pan_env);
    gtk_entry_set_text(GTK_ENTRY(gui_curins_name), current_instrument->name);

    gtk_adjustment_set_value(instrument_page_sliders[0].adjustment1, current_instrument->volfade);
    gtk_adjustment_set_value(instrument_page_sliders[1].adjustment1, current_instrument->vibrate);
    gtk_adjustment_set_value(instrument_page_sliders[2].adjustment1, current_instrument->vibdepth);
    gtk_adjustment_set_value(instrument_page_sliders[3].adjustment1, current_instrument->vibsweep);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(instrument_editor_vibtype_w[current_instrument->vibtype]), TRUE);
}
