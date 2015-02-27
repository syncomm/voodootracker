/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       - Driver Module Definitions -			 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _ST_DRIVER_H
#define _ST_DRIVER_H

#include <glib.h>
#include <gdk/gdktypes.h>

#include "mixer.h"
#include "preferences.h"

typedef struct st_driver {
    const char *id;
    const char *description;

    /* open the driver; see ST_DRIVER_OPEN_FOR_ defines below */
    gboolean (*open)          (int mode);

    /* release the audio device */
    void     (*release)       (void);

    /* set playback preferences, should be called once before open()ing */
    void     (*setprefs)      (st_audio_playback_prefs *);

    /* set number of channels to be played */
    void     (*setnumch)      (int numchannels);

    /* initialize / update / clear sample slot */
    gboolean (*setsample)     (guint16 sample, st_mixer_sample_info *si);

    /* play sample from the beginning, initialize nothing else */
    void     (*startnote)     (int channel, guint16 sample);

    /* stop note */
    void     (*stopnote)      (int channel);

    /* set curent sample play position */
    void     (*setsmplpos)    (int channel, guint32 offset);

    /* set replay frequency (Hz) */
    void     (*setfreq)       (int channel, float frequency);

    /* set sample volume (0.0 ... 1.0) */
    void     (*setvolume)     (int channel, float volume);

    /* set sample panning (-1.0 ... +1.0) */
    void     (*setpanning)    (int channel, float panning);

    /* prepare for playing */
    void     (*prepare)       (void);

    /* wait for the specified time, return TRUE if buffer is full,
       which means that the audio thread should go poll()ing. */
    gboolean (*sync)          (double time);

    /* stop playing */
    void     (*stop)          (void);

    struct st_driver *next;
} st_driver;

#define ST_DRIVER_OPEN_FOR_PLAYING    1
#define ST_DRIVER_OPEN_FOR_SAMPLING   2

/* --- Functions called by the driver */

/* Install / remove poll() handlers, similar to gdk_input_add() */
gpointer audio_poll_add       (int fd,
			       GdkInputCondition cond,
			       GdkInputFunction func);
void     audio_poll_remove    (gpointer poll);

/* Called by the driver to indicate that it accepts new data */
void     audio_play           (void);

/* Called by the playing driver to indicate that the samples belonging
   to the specified time are about to be played immediately. (This is
   used to synchronize the oscilloscopes) */
void     audio_time           (double);

/* Called by the sampling driver to deliver new data to the audio
   subsystem.  This API is going to be changed, though (the current
   one doesn't permit data other than 16bit/44.1kHz */
void     audio_sampled        (gint16 *data,
			       int count);

/* --- Functions called by the player */

void     driver_setnumch      (int numchannels);
gboolean driver_setsample     (guint16 sample,
			       st_mixer_sample_info *si);
void     driver_startnote     (int channel,
			       guint16 sample);
void     driver_stopnote      (int channel);
void     driver_setsmplpos    (int channel,
			       guint32 offset);
void     driver_setfreq       (int channel,
			       float frequency);
void     driver_setvolume     (int channel,
			       float volume);
void     driver_setpanning    (int channel,
			       float panning);

#endif /* _ST_DRIVER_H */
