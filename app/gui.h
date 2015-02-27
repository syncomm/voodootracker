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

#ifndef _GUI_H
#define _GUI_H

#include <gtk/gtk.h>

/* values for gui_playing_mode */
enum {
    PLAYING_SONG = 1,
    PLAYING_PATTERN,
    PLAYING_NOTE,     /* is overridden by PLAYING_SONG / PLAYING_PATTERN */
};

extern int gui_playing_mode; /* one of the above or 0 */

#define GUI_ENABLED (gui_playing_mode != PLAYING_SONG && gui_playing_mode != PLAYING_PATTERN)
#define GUI_EDITING (GTK_TOGGLE_BUTTON(editing_toggle)->active)

extern GtkWidget *editing_toggle, *curins_spin, *spin_octave;
extern GtkWidget *gui_curins_name, *gui_cursmpl_name;

int                  gui_init                         (int argc, char *argv[]);

GtkWidget*           file_selection_create            (const gchar *title,
						       void(*clickfunc)());

void                 gui_play_note                    (int channel,
						       guint gdkkey);
void                 gui_play_stop                    (void);
void                 gui_start_sampling               (void);
void                 gui_stop_sampling                (void);

void                 gui_get_text_entry               (int length,
						       void(*changedfunc)(),
						       GtkWidget **widget);
void                 gui_update_spin_adjustment       (GtkSpinButton *spin,
						       int min,
						       int max);

void                 gui_set_current_instrument       (int);
void                 gui_set_current_sample           (int);

void                 gui_update_player_pos            (double songtime,
						       int nsongpos,
						       int npatpos);

#endif /* _GUI_H */
