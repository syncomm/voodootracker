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

#ifndef _ST_PREFERENCES_H
#define _ST_PREFERENCES_H

#include <gtk/gtk.h>

typedef struct st_audio_playback_prefs {
    int resolution;
    int channels;
    int mixfreq;
    int fragsize;
    int use_from_other;
} st_audio_playback_prefs;

typedef enum st_audio_playback_prefs_types {
    AUDIO_PLAYBACK_PREFS_TYPE_EDITING = 0,
    AUDIO_PLAYBACK_PREFS_TYPE_PLAYING,
    AUDIO_PLAYBACK_PREFS_TYPE_LAST,
} st_audio_playback_prefs_types;

typedef struct st_prefs {
    st_audio_playback_prefs audio_playback[AUDIO_PLAYBACK_PREFS_TYPE_LAST];
} st_prefs;

extern st_prefs st_preferences;

void      prefs_page_create              (GtkNotebook *nb);

void      prefs_page_handle_keys         (int shift,
					  int ctrl,
					  int alt,
					  guint32 keyval);

int       prefs_fragsize_log2            (int fragsize);

#endif /* _ST_PREFERENCES_H */

