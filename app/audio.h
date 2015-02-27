/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *		- Audio Handling Thread -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _ST_AUDIO_H
#define _ST_AUDIO_H

#include <sys/time.h>

#include <glib.h>
#include <pthread.h>

/* === Thread communication stuff */

typedef enum audio_ctlpipe_id {
    AUDIO_CTLPIPE_SET_DRIVER=2000,     /* st_driver *driver */
    AUDIO_CTLPIPE_SET_MIXER,           /* st_mixer *mixer */
    AUDIO_CTLPIPE_INIT_PLAYER,         /* void */
    AUDIO_CTLPIPE_PLAY_SONG,           /* int songpos, int patpos */
    AUDIO_CTLPIPE_PLAY_PATTERN,        /* int pattern, int patpos */
    AUDIO_CTLPIPE_PLAY_NOTE,           /* int channel, int note, int instrument */
    AUDIO_CTLPIPE_STOP_PLAYING,        /* void */
    AUDIO_CTLPIPE_SAMPLE,              /* void */
    AUDIO_CTLPIPE_STOP_SAMPLING,       /* void */
    AUDIO_CTLPIPE_SET_SONGPOS,         /* int songpos */
    AUDIO_CTLPIPE_SET_PATTERN,         /* int pattern */
    AUDIO_CTLPIPE_SET_AMPLIFICATION,   /* float */
    AUDIO_CTLPIPE_SET_PITCHBEND,       /* float */
} audio_ctlpipe_id;

typedef enum audio_backpipe_id {
    AUDIO_BACKPIPE_DRIVER_OPEN_FAILED=1000,
    AUDIO_BACKPIPE_PLAYING_STARTED,
    AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED,
    AUDIO_BACKPIPE_PLAYING_NOTE_STARTED,
    AUDIO_BACKPIPE_PLAYING_STOPPED,
    AUDIO_BACKPIPE_SAMPLING_STARTED,
    AUDIO_BACKPIPE_SAMPLING_STOPPED,
    AUDIO_BACKPIPE_SONGPOS_CONFIRM,    /* double time */
    AUDIO_BACKPIPE_SAMPLING_TICK,      /* int count */
} audio_backpipe_id;

extern int audio_ctlpipe, audio_backpipe;

typedef enum audio_lock_id {
    AUDIO_LOCK_SAMPLEBUF = 0,          /* sampler ring buffer */
    AUDIO_LOCK_PLAYER_TIME,            /* player time variables */
    AUDIO_LOCK_LAST
} audio_lock_id;

void      audio_lock               (audio_lock_id);
void      audio_unlock             (audio_lock_id);

/* === Sampler ring buffer */

extern gint16 *audio_samplebuf;
extern int audio_samplebuf_start, audio_samplebuf_end, audio_samplebuf_len;

/* === Oscilloscope stuff */

extern gint8 *scopebufs[32];
extern double scopebuftime;
extern int scopebuf_freq;
extern int scopebuf_len, scopebufpt;
extern int mixer_clipping;

/* === Player position ring buffer */

typedef struct audio_player_pos {
    double time;
    int songpos;
    int patpos;
} audio_player_pos;

extern audio_player_pos *audio_playerpos;
extern int audio_playerpos_start, audio_playerpos_end, audio_playerpos_len;

/* Lock these with AUDIO_LOCK_PLAYER_TIME before using: */
extern double audio_playerpos_realtime, audio_playerpos_songtime;

/* === Other stuff */

extern gint8 player_mute_channels[32];

gboolean     audio_init               (int ctlpipe, int backpipe);

void         readpipe                 (int fd, void *p, int count);

#endif /* _ST_AUDIO_H */
