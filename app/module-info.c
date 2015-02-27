/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	      	   - Module Info Page -					 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#include <stdio.h>
#include <string.h>

#include <gtk/gtkfeatures.h>

#include "module-info.h"
#include "gui.h"
#include "gui-subs.h"
#include "xm.h"
#include "st-subs.h"
#include "main.h"
#include "sample-editor.h"
#include "instrument-editor.h"

static GtkWidget *ilist, *slist, *songname;
static GtkWidget *freqmode_w[2], *ptmode_toggle;
static int curi = 0, curs = 0;

static void       modinfo_delete_unused_instruments            (void);

static void
ptmode_changed (GtkWidget *widget)
{
    int m = GTK_TOGGLE_BUTTON(widget)->active;
    if(xm) {
	xm->flags &= ~XM_FLAGS_IS_MOD;
	xm->flags |= m * XM_FLAGS_IS_MOD;
    }
}

static void
freqmode_changed (void)
{
    int m = find_current_toggle(freqmode_w, 2);
    if(xm) {
	xm->flags &= ~XM_FLAGS_AMIGA_FREQ;
	xm->flags |= m * XM_FLAGS_AMIGA_FREQ;
    }
}

void
songname_changed (GtkEntry *entry)
{
    strncpy(xm->name, gtk_entry_get_text(entry), 20);
    xm->name[20] = 0;
}

static void
ilist_size_allocate (GtkCList *list)
{
    gtk_clist_set_column_width(list, 0, 30);
    gtk_clist_set_column_width(list, 2, 30);
    gtk_clist_set_column_width(list, 1, list->clist_window_width - 2 * 30 - 2 * 8 - 6);
}

static void
slist_size_allocate (GtkCList *list)
{
    gtk_clist_set_column_width(list, 0, 30);
    gtk_clist_set_column_width(list, 1, list->clist_window_width - 30 - 8 - 7);
}

static void
ilist_select (GtkCList *list,
	      gint row,
	      gint column)
{
    if(row == curi)
	return;
    curi = row;
    gui_set_current_instrument(row + 1);
}

static void
slist_select (GtkCList *list,
	      gint row,
	      gint column)
{
    if(row == curs)
	return;
    curs = row;
    gui_set_current_sample(row);
}

static GtkWidget *
get_clist_in_scrolled_window (int n,
			      gchar **tp,
			      GtkWidget *hbox)
{
    GtkWidget *list;
#if ((GTK_MAJOR_VERSION * 0x10000) + (GTK_MINOR_VERSION * 0x100) + GTK_MICRO_VERSION) >= 0x010105
    GtkWidget *sw;
    list = gtk_clist_new_with_titles(n, tp);
    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_clist_set_shadow_type(GTK_CLIST(list), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(sw), list);
    gtk_widget_show(sw);
    gtk_box_pack_start(GTK_BOX(hbox), sw, TRUE, TRUE, 0);
    gtk_widget_show(list);
#else
    list = gtk_clist_new_with_titles(n, tp);
    gtk_clist_set_policy(GTK_CLIST(list), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_clist_set_border(GTK_CLIST(list), GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start(GTK_BOX(hbox), list, TRUE, TRUE, 0);
    gtk_widget_show(list);
#endif
    return list;
}

void
modinfo_page_create (GtkNotebook *nb)
{
    GtkWidget *hbox, *thing, *vbox;
    static gchar *ititles[3] = { "n", "Instrument Name", "#smpl" };
    static gchar *stitles[2] = { "n", "Sample Name" };
    static const char *freqlabels[] = { "Linear", "Amiga", NULL };
    char buf[5];
    gchar *insertbuf[3] = { buf, "", "0" };
    int i;

    hbox = gtk_hbox_new(TRUE, 10);
    gtk_container_border_width(GTK_CONTAINER(hbox), 10);
    gtk_notebook_append_page(nb, hbox, gtk_label_new("Module Info"));
    gtk_widget_show(hbox);

    ilist = get_clist_in_scrolled_window(3, ititles, hbox);
    gtk_clist_set_selection_mode(GTK_CLIST(ilist), GTK_SELECTION_BROWSE);
    gtk_clist_column_titles_passive(GTK_CLIST(ilist));
    gtk_clist_set_column_justification(GTK_CLIST(ilist), 0, GTK_JUSTIFY_CENTER);
    gtk_clist_set_column_justification(GTK_CLIST(ilist), 1, GTK_JUSTIFY_LEFT);
    gtk_clist_set_column_justification(GTK_CLIST(ilist), 2, GTK_JUSTIFY_CENTER);
    for(i = 1; i <= 128; i++) {
	sprintf(buf, "%d", i);
	gtk_clist_append(GTK_CLIST(ilist), insertbuf);
    }
    gtk_signal_connect_after(GTK_OBJECT(ilist), "size_allocate",
			     GTK_SIGNAL_FUNC(ilist_size_allocate), NULL);
    gtk_signal_connect_after(GTK_OBJECT(ilist), "select_row",
			     GTK_SIGNAL_FUNC(ilist_select), NULL);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    slist = get_clist_in_scrolled_window(2, stitles, vbox);
    gtk_clist_set_selection_mode(GTK_CLIST(slist), GTK_SELECTION_BROWSE);
    gtk_clist_column_titles_passive(GTK_CLIST(slist));
    gtk_clist_set_column_justification(GTK_CLIST(slist), 0, GTK_JUSTIFY_CENTER);
    gtk_clist_set_column_justification(GTK_CLIST(slist), 1, GTK_JUSTIFY_LEFT);
    for(i = 0; i < 16; i++) {
	sprintf(buf, "%d", i);
	gtk_clist_append(GTK_CLIST(slist), insertbuf);
    }
    gtk_signal_connect_after(GTK_OBJECT(slist), "size_allocate",
			     GTK_SIGNAL_FUNC(slist_size_allocate), NULL);
    gtk_signal_connect_after(GTK_OBJECT(slist), "select_row",
			     GTK_SIGNAL_FUNC(slist_select), NULL);

    thing = gtk_button_new_with_label("Delete unused instruments");
    gtk_signal_connect(GTK_OBJECT(thing), "clicked",
		       GTK_SIGNAL_FUNC(modinfo_delete_unused_instruments), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_label_new("Songname:");
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    gui_get_text_entry(20, songname_changed, &songname);
    gtk_box_pack_start(GTK_BOX(hbox), songname, TRUE, TRUE, 0);
    gtk_signal_connect_after(GTK_OBJECT(songname), "changed",
			     GTK_SIGNAL_FUNC(songname_changed), NULL);
    gtk_widget_show(songname);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    add_empty_hbox(hbox);
    thing = make_labelled_radio_group_box("Frequencies:", freqlabels, freqmode_w, freqmode_changed);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    add_empty_hbox(hbox);
    
    ptmode_toggle = gtk_check_button_new_with_label("ProTracker Mode");
    gtk_box_pack_start(GTK_BOX(hbox), ptmode_toggle, FALSE, TRUE, 0);
    gtk_widget_show(ptmode_toggle);
    gtk_signal_connect (GTK_OBJECT(ptmode_toggle), "toggled",
			GTK_SIGNAL_FUNC(ptmode_changed), NULL);

    add_empty_hbox(hbox);

}

static void
modinfo_delete_unused_instruments (void)
{
    int i;

    for(i = 0; i < sizeof(xm->instruments) / sizeof(xm->instruments[0]); i++) {
	if(!st_instrument_used_in_song(xm, i + 1)) {
	    st_clean_instrument(&xm->instruments[i], NULL);
	}
    }

    sample_editor_update();
    instrument_editor_update();
    modinfo_update_all();
}

void
modinfo_page_handle_keys (int shift,
			  int ctrl,
			  int alt,
			  guint32 keyval)
{
    if(!ctrl && !alt && !shift)
	gui_play_note(0, keyval);
}

void
modinfo_update_instrument (int n)
{
    int i;
    GtkCList *list = GTK_CLIST(ilist);
    char buf[5];

    gtk_clist_set_text(list, n, 1, xm->instruments[n].name);
    sprintf(buf, "%d", st_instrument_num_samples(&xm->instruments[n]));
    gtk_clist_set_text(list, n, 2, buf);

    if(n == curi) {
	for(i = 0; i < 16; i++)
	    modinfo_update_sample(i);
    }
}

void
modinfo_update_sample (int n)
{
    GtkCList *list = GTK_CLIST(slist);

    gtk_clist_set_text(list, n, 1, xm->instruments[curi].samples[n].name);
}

void
modinfo_update_all (void)
{
    int i;

    for(i = 0; i < 128; i++)
	modinfo_update_instrument(i);

    gtk_entry_set_text(GTK_ENTRY(songname), xm->name);

    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ptmode_toggle), xm->flags & XM_FLAGS_IS_MOD);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(freqmode_w[xm->flags & XM_FLAGS_AMIGA_FREQ]), TRUE);
}

void
modinfo_set_current_instrument (int n)
{
    int i;

    g_return_if_fail(n >= 0 && n <= 127);
    curi = n;
    gtk_clist_select_row(GTK_CLIST(ilist), n, 1);
    for(i = 0; i < 16; i++)
	modinfo_update_sample(i);
}

void
modinfo_set_current_sample (int n)
{
    g_return_if_fail(n >= 0 && n <= 15);
    curs = n;
    gtk_clist_select_row(GTK_CLIST(slist), n, 1);
}
