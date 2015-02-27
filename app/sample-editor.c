/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       		  - Sample Editor -					 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *
 Broukoid	Jul,9 2000	Corrected WAV loading function
				for different format chunk length

 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>

#include "gtkspinbutton.h"

#include "sample-editor.h"
#include "xm.h"
#include "st-subs.h"
#include "gui.h"
#include "gui-subs.h"
#include "instrument-editor.h"
#include "sample-display.h"
#include "endian-conv.h"
#include "audio.h"

static STSample *current_sample = NULL;

static GtkWidget *spin_volume, *spin_panning, *spin_finetune, *spin_relnote;
static GtkWidget *loadwindow, *savewindow, *savebutton;
static SampleDisplay *sampledisplay;
static GtkWidget *loopradio[3], *resolution_radio[2];
static GtkWidget *spin_loopstart, *spin_loopend, *spin_selstart, *spin_selend;
static GtkWidget *box_loop, *box_params, *box_sel;
static GtkWidget *label_slength;

static SampleDisplay *monitorscope;
static GtkWidget *cancelbutton, *okbutton, *startsamplingbutton;

struct recordbuf {
    struct recordbuf *next;
    int length;
    gint16 data[0];
};

static struct recordbuf *recordbufs, *current;
static int recordbuflen = 10000, currentoffs;
static int recordedlen, sampling;

static void sample_editor_cancel_clicked(void);
static void sample_editor_ok_clicked(void);
static void sample_editor_start_sampling_clicked(void);

static void sample_editor_spin_volume_changed(GtkSpinButton *spin);
static void sample_editor_spin_panning_changed(GtkSpinButton *spin);
static void sample_editor_spin_finetune_changed(GtkSpinButton *spin);
static void sample_editor_spin_relnote_changed(GtkSpinButton *spin);
static void sample_editor_loop_changed(void);
static void sample_editor_spin_selection_changed(void);
static void sample_editor_display_loop_changed(SampleDisplay *, int start, int end);
static void sample_editor_display_selection_changed(SampleDisplay *, int start, int end);
static void sample_editor_reset_selection_clicked(void);
static void sample_editor_clear_clicked(void);
static void sample_editor_show_all_clicked(void);
static void sample_editor_zoom_in_clicked(void);
static void sample_editor_zoom_out_clicked(void);
static void sample_editor_loopradio_changed(void);
static void sample_editor_resolution_changed(void);
static void sample_editor_load_save_clicked(GtkWidget *widget, GtkWidget *fs);
static void sample_editor_monitor_clicked(void);
static void sample_editor_cut_clicked(void);
static void sample_editor_remove_clicked(void);
static void sample_editor_copy_clicked(void);
static void sample_editor_paste_clicked(void);
static void sample_editor_zoom_to_selection_clicked(void);

static void sample_editor_load_wav(void);
static void sample_editor_save_wav(void);

static void
sampler_page_enable_widgets (int enable)
{
    gtk_widget_set_sensitive(okbutton, !enable);
    gtk_widget_set_sensitive(startsamplingbutton, enable);
}

static void
sample_editor_load_save_clicked (GtkWidget *widget,
				 GtkWidget *fs)
{
    gtk_widget_show(fs);
}

GtkWidget*
sampler_sampling_page_create (void)
{
    GtkWidget *box, *thing, *box2;

    box = gtk_vbox_new(FALSE, 2);

    thing = sample_display_new(FALSE);
    gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    monitorscope = SAMPLE_DISPLAY(thing);
    
    box2 = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(box), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    thing = gtk_button_new_with_label("OK");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_ok_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_set_sensitive(thing, 0);
    gtk_widget_show(thing);
    okbutton = thing;

    thing = gtk_button_new_with_label("Start sampling");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_start_sampling_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    startsamplingbutton = thing;

    thing = gtk_button_new_with_label("Cancel");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_cancel_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    cancelbutton = thing;

    return box;
}

static int
sample_editor_find_current_toggle (GtkWidget **widgets,
				   int count)
{
    int i;
    for (i = 0; i < count; i++)
	if (GTK_TOGGLE_BUTTON(*widgets++)->active)
	    return i;
    g_assert_not_reached();
    return -1;
}

static void
sample_editor_make_radio_group (const char **labels, GtkWidget *tobox,
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

void
sample_editor_page_create (GtkNotebook *nb)
{
    GtkWidget *box, *thing, *hbox, *vbox;
    static const char *looplabels[] = { "No loop", "Amiga", "PingPong", NULL };
    static const char *resolutionlabels[] = { "8 bits", "16 bits", NULL };

    box = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(box), 10);
    gtk_notebook_append_page(nb, box, gtk_label_new("Sample Editor"));
    gtk_widget_show(box);

    thing = sample_display_new(TRUE);
    gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect(GTK_OBJECT(thing), "loop_changed",
		       GTK_SIGNAL_FUNC(sample_editor_display_loop_changed), NULL);
    gtk_signal_connect(GTK_OBJECT(thing), "selection_changed",
		       GTK_SIGNAL_FUNC(sample_editor_display_selection_changed), NULL);
    sampledisplay = SAMPLE_DISPLAY(thing);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    box_loop = vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);

    sample_editor_make_radio_group(looplabels, vbox, loopradio, FALSE, FALSE, sample_editor_loopradio_changed);
    gui_put_labelled_spin_button(vbox, "Start", 0, 0, &spin_loopstart, sample_editor_loop_changed);
    gui_put_labelled_spin_button(vbox, "End", 0, 0, &spin_loopend, sample_editor_loop_changed);
    sample_editor_loopradio_changed();

    thing = gtk_vseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

    box_params = vbox = gtk_vbox_new(TRUE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);

    gui_put_labelled_spin_button(vbox, "Volume", 0, 64, &spin_volume, sample_editor_spin_volume_changed);
    gui_put_labelled_spin_button(vbox, "Panning", -128, 127, &spin_panning, sample_editor_spin_panning_changed);
    gui_put_labelled_spin_button(vbox, "Finetune", -128, 127, &spin_finetune, sample_editor_spin_finetune_changed);
    sample_editor_make_radio_group(resolutionlabels, vbox, resolution_radio, FALSE, FALSE, sample_editor_resolution_changed);

    thing = gtk_vseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

    box_sel = vbox = gtk_vbox_new(TRUE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);

    gui_put_labelled_spin_button(vbox, "SelStart", 0, 0, &spin_selstart, sample_editor_spin_selection_changed);
    gui_put_labelled_spin_button(vbox, "SelEnd", 0, 0, &spin_selend, sample_editor_spin_selection_changed);
    gtk_widget_set_sensitive(spin_selstart, 0);
    gtk_widget_set_sensitive(spin_selend, 0);
    thing = gtk_button_new_with_label("Reset Sel");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_reset_selection_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gui_put_labelled_spin_button(vbox, "Relnote", -128, 127, &spin_relnote, sample_editor_spin_relnote_changed);
    label_slength = thing = gtk_label_new("Length: 0");
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

    thing = gtk_vseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    loadwindow = file_selection_create("Load WAV...", sample_editor_load_wav);
    savewindow = file_selection_create("Save WAV...", sample_editor_save_wav);

    thing = gtk_button_new_with_label("Load WAV");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_load_save_clicked), loadwindow);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label("Save WAV");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_load_save_clicked), savewindow);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    savebutton = thing;

    thing = gtk_button_new_with_label("Clear");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_clear_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label("Monitor");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_monitor_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    thing = gtk_button_new_with_label("Zoom to selection");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_zoom_to_selection_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label("Show all");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_show_all_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label("Zoom in (+50%)");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_zoom_in_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label("Zoom out (-50%)");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_zoom_out_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    thing = gtk_button_new_with_label("Cut");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_cut_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_widget_set_sensitive(thing, 0);

    thing = gtk_button_new_with_label("Remove");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_remove_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label("Copy");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_copy_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_widget_set_sensitive(thing, 0);

    thing = gtk_button_new_with_label("Paste");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(sample_editor_paste_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_widget_set_sensitive(thing, 0);
}

static void
sample_editor_block_loop_spins (int block)
{
    void (*func) (GtkObject*, GtkSignalFunc, gpointer);

    func = block ? gtk_signal_handler_block_by_func : gtk_signal_handler_unblock_by_func;

    func(GTK_OBJECT(spin_loopstart), sample_editor_loop_changed, NULL);
    func(GTK_OBJECT(spin_loopend), sample_editor_loop_changed, NULL);
}

static void
sample_editor_block_sel_spins (int block)
{
    void (*func) (GtkObject*, GtkSignalFunc, gpointer);

    func = block ? gtk_signal_handler_block_by_func : gtk_signal_handler_unblock_by_func;

    func(GTK_OBJECT(spin_selstart), sample_editor_spin_selection_changed, NULL);
    func(GTK_OBJECT(spin_selend), sample_editor_spin_selection_changed, NULL);
}

static void
sample_editor_blocked_set_loop_spins (int start,
				      int end)
{
    sample_editor_block_loop_spins(1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_loopstart), start);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_loopend), end);
    sample_editor_block_loop_spins(0);
}

static void
sample_editor_blocked_set_sel_spins (int start,
				     int end)
{
    sample_editor_block_sel_spins(1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_selstart), start);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_selend), end);
    sample_editor_block_sel_spins(0);
}

static void
sample_editor_blocked_set_display_loop (int start,
					int end)
{
    gtk_signal_handler_block_by_func(GTK_OBJECT(sampledisplay), GTK_SIGNAL_FUNC(sample_editor_display_loop_changed), NULL);
    sample_display_set_loop(sampledisplay, start, end);
    gtk_signal_handler_unblock_by_func(GTK_OBJECT(sampledisplay), GTK_SIGNAL_FUNC(sample_editor_display_loop_changed), NULL);
}

void
sample_editor_update (void)
{
    STSample *s = current_sample;
    char buf[20];

    if(!current_sample || !current_sample->data) {
	gtk_widget_set_sensitive(box_loop, 0);
	gtk_widget_set_sensitive(box_params, 0);
	gtk_widget_set_sensitive(box_sel, 0);
	gtk_widget_hide(savewindow);
	gtk_widget_set_sensitive(savebutton, 0);
	sample_display_set_data_16(sampledisplay, NULL, 0, FALSE);
	return;
    }

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_volume), s->volume);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_panning), s->panning - 128);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_finetune), s->finetune);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_relnote), s->relnote);
    gtk_entry_set_text(GTK_ENTRY(gui_cursmpl_name), s->name);

    if(s->type == 16)
	sample_display_set_data_16(sampledisplay, s->data, s->length, FALSE);
    else
	sample_display_set_data_8(sampledisplay, s->data, s->length, FALSE);

    if(s->loop != LOOP_NONE)
	sample_editor_blocked_set_display_loop(s->loopstart, s->loopend);

    gtk_widget_set_sensitive(box_loop, 1);
    gtk_widget_set_sensitive(box_params, 1);
    gtk_widget_set_sensitive(box_sel, 1);
    gtk_widget_set_sensitive(savebutton, 1);

    sprintf(buf, "Length: %d", s->length);
    gtk_label_set(GTK_LABEL(label_slength), buf);

    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(resolution_radio[(s->type / 8) - 1]), TRUE);

    sample_editor_block_loop_spins(1);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(loopradio[s->loop]), TRUE);
    gui_update_spin_adjustment(GTK_SPIN_BUTTON(spin_loopstart), 0, s->length - 1);
    gui_update_spin_adjustment(GTK_SPIN_BUTTON(spin_loopend), 1, s->length);
    sample_editor_block_loop_spins(0);

    sample_editor_blocked_set_loop_spins(s->loopstart, s->loopend);

    sample_editor_block_sel_spins(1);
    gui_update_spin_adjustment(GTK_SPIN_BUTTON(spin_selstart), -1, s->length - 1);
    gui_update_spin_adjustment(GTK_SPIN_BUTTON(spin_selend), 1, s->length);
    sample_editor_block_sel_spins(0);
    sample_editor_blocked_set_sel_spins(-1, 0);
}

void
sampler_page_handle_keys (int shift,
			  int ctrl,
			  int alt,
			  guint32 keyval)
{
    if(!ctrl && !alt && !shift)
	gui_play_note(0, keyval);
}

void
sample_editor_set_sample (STSample *s)
{
    current_sample = s;
    sample_editor_update();
}

void
sample_editor_update_mixer_position (void)
{
//    int i = 0;
    int offset = -1;

/*    for(i = 0; i < sizeof(public_channels) / sizeof(public_channels[0]); i++) {
	if(current_sample && public_channels[i].sample == current_sample)
	    offset = public_channels[i].offset;
    }
    */
    sample_display_set_mixer_position(sampledisplay, offset);
}

static void
sample_editor_spin_volume_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->volume = gtk_spin_button_get_value_as_int(spin);
}

static void
sample_editor_spin_panning_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->panning = gtk_spin_button_get_value_as_int(spin) + 128;
}

static void
sample_editor_spin_finetune_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->finetune = gtk_spin_button_get_value_as_int(spin);
}

static void
sample_editor_spin_relnote_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->relnote = gtk_spin_button_get_value_as_int(spin);
}

static void
sample_editor_loop_changed ()
{
    int s = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_loopstart)),
	e = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_loopend));

    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->data != NULL);
    g_return_if_fail(current_sample->loop != LOOP_NONE);

    if(s != current_sample->loopstart || e != current_sample->loopend) {
	if(e <= s) {
	    e = s + 1;
	    sample_editor_blocked_set_loop_spins(s, e);
	}
	current_sample->loopend = e;
	current_sample->loopstart = s;
	sample_editor_blocked_set_display_loop(s, e);
    }
}

static void
sample_editor_spin_selection_changed (void)
{

}

static void
sample_editor_display_loop_changed (SampleDisplay *sample_display,
				    int start,
				    int end)
{
    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->data != NULL);
    g_return_if_fail(current_sample->loop != LOOP_NONE);
    g_return_if_fail(start < end);

    if(start != current_sample->loopstart || end != current_sample->loopend) {
	sample_editor_blocked_set_loop_spins(start, end);
	current_sample->loopend = end;
	current_sample->loopstart = start;
    }
}

static void
sample_editor_reset_selection_clicked (void)
{
    sample_display_set_selection(sampledisplay, -1, 1);
}

static void
sample_editor_display_selection_changed (SampleDisplay *sample_display,
					 int start,
					 int end)
{
    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->data != NULL);
    g_return_if_fail(start < end);

    sample_editor_blocked_set_sel_spins(start, end);
}

static void
sample_editor_loopradio_changed (void)
{
    int n = sample_editor_find_current_toggle(loopradio, 3);

    gtk_widget_set_sensitive(spin_loopstart, n != 0);
    gtk_widget_set_sensitive(spin_loopend, n != 0);
    
    if(current_sample != NULL) {
	current_sample->loop = n;
	if(n != LOOP_NONE) {
	    sample_editor_blocked_set_display_loop(current_sample->loopstart, current_sample->loopend);
	} else {
	    sample_editor_blocked_set_display_loop(-1, 1);
	}
    }
}

static void
sample_editor_resolution_changed (void)
{
    void *newdata;
    STSample *s = current_sample;
    int n = sample_editor_find_current_toggle(resolution_radio, 2);
    int i;
    gint16 *d16;
    gint8 *d8;

    if(current_sample != NULL && (s->type / 8) - 1 != n) {
	newdata = malloc(s->length * (n + 1));
	if(!newdata)
	    return;

	gui_play_stop();

	sample_editor_set_sample(NULL);

	if(n == 1) {
	    /* convert to 16 bit */
	    d16 = newdata;
	    d8 = s->data;
	    for(i = s->length; i; i--)
		*d16++ = (*d8++ << 8);
	} else {
	    /* convert to 8 bit */
	    d8 = newdata;
	    d16 = s->data;
	    for(i = s->length; i; i--)
		*d8++ = (*d16++ >> 8);
	}

	free(s->data);

	s->data = newdata;
	s->type = (n + 1) * 8;

	sample_editor_set_sample(s);
    }
}

static void
sample_editor_clear_clicked (void)
{
    STInstrument *instr;

    gui_play_stop();

    st_clean_sample(current_sample, NULL);

    instr = instrument_editor_get_instrument();
    if(st_instrument_num_samples(instr) == 0)
	st_clean_instrument(instr, NULL);

    instrument_editor_update();
    sample_editor_update();
}

static void
sample_editor_show_all_clicked (void)
{
    if(current_sample == NULL || current_sample->data == NULL)
	return;
    sample_display_set_window(sampledisplay, 0, current_sample->length);
}

static void
sample_editor_zoom_in_clicked (void)
{
    int ns = sampledisplay->win_start,
	ne = sampledisplay->win_start + sampledisplay->win_length;
    int l;
    
    if(current_sample == NULL || current_sample->data == NULL)
	return;

    l = sampledisplay->win_length / 4;

    ns += l;
    ne -= l;

    if(ne <= ns)
	ne = ns + 1;

    sample_display_set_window(sampledisplay, ns, ne);
}

static void
sample_editor_zoom_out_clicked (void)
{
    int ns = sampledisplay->win_start,
	ne = sampledisplay->win_start + sampledisplay->win_length;
    int l;
    
    if(current_sample == NULL || current_sample->data == NULL)
	return;

    l = sampledisplay->win_length / 2;

    if(ns > l)
	ns -= l;
    else
	ns = 0;

    if(ne <= current_sample->length - l)
	ne += l;
    else
	ne = current_sample->length;

    sample_display_set_window(sampledisplay, ns, ne);
}

static void
sample_editor_zoom_to_selection_clicked (void)
{
    if(current_sample == NULL || current_sample->data == NULL || sampledisplay->sel_start == -1)
	return;
    sample_display_set_window(sampledisplay, sampledisplay->sel_start, sampledisplay->sel_end);
}

static void
sample_editor_cut_clicked (void)
{
}

static void
sample_editor_remove_clicked (void)
{
    int cutlen, newlen;
    void *newsample;
    STSample *oldsample = current_sample;
    int ss, se, mult;

    if(oldsample == NULL || sampledisplay->sel_start == -1)
	return;

    ss = sampledisplay->sel_start;
    se = sampledisplay->sel_end;
    mult = oldsample->type / 8;

    cutlen = se - ss;
    newlen = oldsample->length - cutlen;

    if(newlen == 0) {
	sample_editor_clear_clicked();
	return;
    }

    newsample = malloc(newlen * mult);
    if(!newsample)
	return;

    gui_play_stop();

    sample_editor_set_sample(NULL);

    memcpy(newsample,
	   oldsample->data,
	   ss * mult);
    memcpy(newsample + (ss * mult),
	   oldsample->data + (se * mult),
	   (oldsample->length - se) * mult);

    free(oldsample->data);

    oldsample->data = newsample;
    oldsample->length = newlen;

    st_sample_fix_loop(oldsample);

    sample_editor_set_sample(oldsample);
}

static void
sample_editor_copy_clicked (void)
{
}

static void
sample_editor_paste_clicked (void)
{
}

#if 0
/* struct wav_hdr borrowed from sidplay by Michael Schwendt */
struct wav_hdr                   // little endian
{
0     gint8 main_chunk[4];         // 'RIFF'
4     guint32 length;              // filelength
8     gint8 chunk_type[4];         // 'WAVE'
12    gint8 sub_chunk[4];          // 'fmt '
16    guint32 clength;             // length of sub_chunk, always 16 bytes
20    guint16 format;              // currently always = 1 = PCM-Code
22    guint16 modus;               // 1 = mono, 2 = stereo
24    guint32 samplefreq;          // sample-frequency
28    guint32 bytespersec;         // frequency * bytespersmpl  
32    guint16 bytespersmpl;        // bytes per sample; 1 = 8-bit, 2 = 16-bit
34    guint16 bitspersmpl;
36    gint8 data_chunk[4];         // keyword, begin of data chunk; = 'data'
40    guint32 data_length;         // length of data
};
#endif

static void
sample_editor_modify_wav_sample (STSample *s)
{
    if(s->type == 16) {
	byteswap_16_array(s->data, s->length);
    } else {
	gint8 *data = s->data;
	int length = s->length;
	while(length) {
	    *data = *data++ + 128;
	    length--;
	}
    }
}

static void
sample_editor_load_wav (void)
{
    const gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(loadwindow));
    FILE *f;
    void *sbuf;
    STInstrument *instr;
    const char *samplename;
    guint8 wh[46];
    int len, tmplen, fmtsize;

    fprintf(stderr, "\nLoading file '%s'...\n", fn);

    gtk_widget_hide(loadwindow);

    g_return_if_fail(current_sample != NULL);

    if(!(f = fopen(fn, "r"))) {
	fprintf(stderr, "Can't open '%s' for reading.\n", fn);
	return;
    }

    fread(wh, 1, sizeof(wh), f);
    if(feof(f) || memcmp(wh + 0, "RIFF", 4) || memcmp(wh + 8, "WAVE", 4) || memcmp(wh + 12, "fmt ", 4)) {
	fprintf(stderr, "'%s' is no WAV file.\n", fn);
	goto errnobuf;
    }

    if(get_le_16(wh + 22) != 1) {
	fprintf(stderr, "Can only handle mono samples.\n");
	goto errnobuf;
    }

    if(get_le_16(wh + 34) != 8 && get_le_16(wh + 34) != 16) {
	fprintf(stderr, "Hmm, strange WAV. I am a bug. Report me.\n");
	goto errnobuf;
    }
    fmtsize=get_le_32(wh + 16);
    len = get_le_32(wh + 24 + fmtsize);
    fprintf(stderr, "Data length: %dbytes.\n", len);
    if(!(sbuf = malloc(len))) {
	fprintf(stderr, "Out of memory (%dbytes).\n", len);
	goto errnobuf;
    }

    fseek(f, 28 + fmtsize, SEEK_SET);
    if((tmplen=fread(sbuf, 1, len, f)) != len) {
	fprintf(stderr, "File error. (was len=%dbytes).\n", tmplen);
	goto errnodata;
    }

    samplename = strrchr(fn, '/');
    if(!samplename)
	samplename = fn;
    else
	samplename++;

    gui_play_stop();

    st_clean_sample(current_sample, NULL);

    instr = instrument_editor_get_instrument();
    if(st_instrument_num_samples(instr) == 0)
	st_clean_instrument(instr, samplename);

    st_clean_sample(current_sample, samplename);

    current_sample->data = sbuf;
    current_sample->type = get_le_16(wh + 34);
    current_sample->length = len / (current_sample->type / 8);
    current_sample->volume = 64;
    current_sample->finetune = 0;
    current_sample->panning = 128;
    current_sample->relnote = 0;

    sample_editor_modify_wav_sample(current_sample);

    instrument_editor_enable();
    sample_editor_update();
    instrument_editor_update();
    
    fclose(f);
    return;

  errnodata:
    free(sbuf);
  errnobuf:
    fclose(f);
    return;
}

static void
sample_editor_save_wav (void)
{
    const gchar *fn;
    FILE *f;
    int type;
    guint8 wh[44];

    gtk_widget_hide(savewindow);

    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->data != NULL);

    fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(savewindow));
    f = fopen(fn, "w");
    if(!f) {
	fprintf(stderr, "Can't open '%s' for writing.\n", fn);
	return;
    }

    type = current_sample->type / 8;

    memset(wh, 0, sizeof(wh));
    memcpy(wh + 0, "RIFF", 4);
    memcpy(wh + 8, "WAVE", 4);
    memcpy(wh + 12, "fmt ", 4);
    put_le_32(wh + 16, 16);
    put_le_16(wh + 20, 1);
    put_le_16(wh + 22, 1);
    put_le_32(wh + 24, 44100);
    put_le_32(wh + 28, 44100);
    put_le_16(wh + 32, type);
    put_le_16(wh + 34, type * 8);
    memcpy(wh + 36, "data", 4);
    put_le_32(wh + 40, current_sample->length * type);
    put_le_32(wh + 4, current_sample->length * type + 44 - 8);

    fwrite(wh, 1, sizeof(wh), f);

    sample_editor_modify_wav_sample(current_sample);

    fwrite(current_sample->data, 1, current_sample->length * type, f);

    sample_editor_modify_wav_sample(current_sample);

    if(ferror(f)) {
	fprintf(stderr, "Error while writing '%s'\n", fn);
    }

    fclose(f);

    return;
}

/* ============================ Sampling functions coming up -------- */

static void
sample_editor_monitor_clicked (void)
{
    gui_start_sampling();
}

void
sample_editor_sampling_mode_activated (void)
{
    sampler_page_enable_widgets(TRUE);

    recordbufs = NULL;
    sampling = 0;
    recordedlen = 0;
    current = NULL;
    currentoffs = recordbuflen;
}

void
sample_editor_sampling_mode_deactivated (void)
{
    sampler_page_enable_widgets(TRUE);
}

void
sample_editor_sampling_tick (const int count)
{
    int n, x;
    static gint16 *buf = NULL;
    static int bufsize = 0;
    gint16 *b;

    audio_lock(AUDIO_LOCK_SAMPLEBUF);

    if(!audio_samplebuf)
	goto out;

    if(bufsize < count) {
	bufsize = count;
	free(buf);
	buf = malloc(2 * bufsize);
    }

    b = buf;
    n = count;
    while(n) {
	x = MIN(n, audio_samplebuf_len - audio_samplebuf_start);
	
	if(sampling) {
	    if(currentoffs == recordbuflen) {
		struct recordbuf *newbuf = malloc(sizeof(struct recordbuf) + recordbuflen * 2);
		if(!newbuf) {
		    fprintf(stderr, "out of memory while sampling...\n");
		    sampling = 0;
		    break;
		}
		newbuf->next = NULL;
		newbuf->length = 0;
		currentoffs = 0;
		if(!recordbufs)
		    recordbufs = newbuf;
		else
		    current->next = newbuf;
		current = newbuf;
	    }

	    x = MIN(x, recordbuflen - currentoffs);
	    memcpy(current->data + currentoffs, audio_samplebuf + audio_samplebuf_start, x * 2);

	    current->length += x;
	    currentoffs += x;
	    recordedlen += n;
	}

	memcpy(b, audio_samplebuf + audio_samplebuf_start, x * 2);
	b += x;

	n -= x;
	audio_samplebuf_start += x;
	if(audio_samplebuf_start == audio_samplebuf_len)
	    audio_samplebuf_start = 0;
    }

    sample_display_set_data_16(monitorscope, buf, count, FALSE);

  out:
    audio_unlock(AUDIO_LOCK_SAMPLEBUF);
}

static void
sample_editor_cancel_clicked (void)
{
    struct recordbuf *r, *r2;

    gui_stop_sampling();

    /* clear the recorded sample */
    for(r = recordbufs; r; r = r2) {
	r2 = r->next;
	free(r);
    }
}

static void
sample_editor_ok_clicked (void)
{
    STInstrument *instr;
    struct recordbuf *r, *r2;
    gint16 *sbuf;
    static const char *samplename = "<just sampled>";

    gui_stop_sampling();

    sampler_page_enable_widgets(TRUE);

    g_return_if_fail(current_sample != NULL);

    st_clean_sample(current_sample, NULL);

    instr = instrument_editor_get_instrument();
    if(st_instrument_num_samples(instr) == 0)
	st_clean_instrument(instr, samplename);

    st_clean_sample(current_sample, samplename);
    
    sbuf = malloc(recordedlen * 2);
    current_sample->data = sbuf;
    
    for(r = recordbufs; r; r = r2) {
	r2 = r->next;
	memcpy(sbuf, r->data, r->length * 2);
	sbuf += r->length;
	free(r);
    }

    current_sample->length = recordedlen;
    current_sample->volume = 64;
    current_sample->finetune = 0;
    current_sample->panning = 128;
    current_sample->relnote = 0;
    current_sample->type = 16;

    instrument_editor_update();
    sample_editor_update();
}

static void
sample_editor_start_sampling_clicked (void)
{
    sampler_page_enable_widgets(FALSE);
    sampling = 1;
}
