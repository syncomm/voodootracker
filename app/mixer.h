/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       - Mixer Module Definitions -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _ST_MIXER_H
#define _ST_MIXER_H

#include <glib.h>

typedef struct st_mixer_sample_info {
    int type;             /* see ST_MIXER_SAMPLE_TYPE_ defines below */
    guint32 length;       /* length in samples, not in bytes */

    int looptype;         /* see ST_MIXER_SAMPLE_LOOPTYPE_ defines below */
    guint32 loopstart;    /* offset in samples, not in bytes */
    guint32 loopend;      /* offset to first sample not being played */

    void *data;           /* pointer to sample data */
} st_mixer_sample_info;

/* values for st_mixer_sample_info.type */
#define ST_MIXER_SAMPLE_TYPE_8   0
#define ST_MIXER_SAMPLE_TYPE_16  1

/* values for st_mixer_sample_info.looptype */
#define ST_MIXER_SAMPLE_LOOPTYPE_NONE      0
#define ST_MIXER_SAMPLE_LOOPTYPE_AMIGA     1
#define ST_MIXER_SAMPLE_LOOPTYPE_PINGPONG  2

typedef struct st_mixer {
    const char *id;
    const char *description;

    /* set number of channels to be mixed */
    void     (*setnumch)     (int numchannels);

    /* initialize / update / clear sample slot */
    gboolean (*setsample)    (guint16 sample, st_mixer_sample_info *si);

    /* set mixer output format -- signed 16 or 8 (in machine endianness) */
    gboolean (*setmixformat) (int format);

    /* toggle stereo mixing -- interleaved left / right samples */
    gboolean (*setstereo)    (int on);

    /* set mixing frequency */
    void     (*setmixfreq)   (guint16 frequency);

    /* set final amplification factor (0.0 = mute ... 1.0 = normal ... +inf = REAL LOUD! :D)*/
    void     (*setampfactor) (float amplification);

    /* returns true if last mix() call had to clip the signal */
    gboolean (*getclipflag)  (void);

    /* reset internal playing state */
    void     (*reset)        (void);

    /* play sample from the beginning, initialize nothing else */
    void     (*startnote)    (int channel, guint16 sample);

    /* stop note */
    void     (*stopnote)     (int channel);

    /* set curent sample play position */
    void     (*setsmplpos)   (int channel, guint32 offset);

    /* set replay frequency (Hz) */
    void     (*setfreq)      (int channel, float frequency);

    /* set sample volume (0.0 ... 1.0) */
    void     (*setvolume)    (int channel, float volume);

    /* set sample panning (-1.0 ... +1.0) */
    void     (*setpanning)   (int channel, float panning);

    /* do the mix, return pointer to end of dest */
    void*    (*mix)          (void *dest, guint32 count, gint8 *scopebufs[], int scopebuf_offset);

    struct st_mixer *next;
} st_mixer;

typedef enum {
    ST_MIXER_FORMAT_S16_LE = 1,
    ST_MIXER_FORMAT_S16_BE,
    ST_MIXER_FORMAT_S8,
    ST_MIXER_FORMAT_U16_LE,
    ST_MIXER_FORMAT_U16_BE,
    ST_MIXER_FORMAT_U8,
} STMixerFormat;

void      mixer_mix_format             (STMixerFormat, int stereo);
void      mixer_set_frequency          (guint16);
void      mixer_prepare                (void);
void      mixer_mix                    (void *dest,
					guint32 count);

#endif /* _MIXER_H */
