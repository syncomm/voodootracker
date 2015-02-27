/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	      	  - OSS (mixing) Driver -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#include <config.h>

#if DRIVER_OSS

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/soundcard.h>

#include <glib.h>
#include "driver.h"
#include "mixer.h"
#include "preferences.h"

static int soundfd = -1;
static void *sndbuf = NULL;
static int bufpos;
static double current_time, buftime, buftime2;
static int nsamples;

static int p_resolution, p_channels, p_mixfreq, p_fragsize;

static int playrate;
static int stereo;
static int bits;
static int fragsize;

static void         oss_release                    (void);
static gboolean     oss_open                       (int mode);
static void         oss_prepare                    (void);
static gboolean     oss_sync                       (double time);
static void         oss_stop                       (void);
static void         oss_setprefs                   (st_audio_playback_prefs *);

static  void        oss_poll_ready_sampling (gpointer data,
					     gint source,
					     GdkInputCondition condition);
static  void        oss_poll_ready_playing (gpointer data,
					    gint source,
					    GdkInputCondition condition);
static gpointer polltag = NULL;
static int firstpoll;

st_driver driver_oss = {
    "ossm",
    "Open Sound System mixing driver",

    oss_open,
    oss_release,
    oss_setprefs,

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    oss_prepare,
    oss_sync,
    oss_stop,

    NULL
};

static gboolean
oss_try_format (int f)
{
    int format = f;

    if(ioctl(soundfd, SNDCTL_DSP_SETFMT, &format) == -1) {
	perror("SNDCTL_DSP_SETFMT");
	return FALSE;
    }  
    
    return format == f;
}

static gboolean
oss_try_stereo (int f)
{
    int format = f;

    if(ioctl(soundfd, SNDCTL_DSP_STEREO, &format) == -1) {
	perror("SNDCTL_DSP_STEREO");
	return FALSE;
    }  
    
    return format == f;
}

static void
oss_setprefs (st_audio_playback_prefs *p)
{
    p_resolution = p->resolution;
    p_channels = p->channels;
    p_mixfreq = p->mixfreq;
    p_fragsize = prefs_fragsize_log2(p->fragsize);

    if(soundfd != -1) {
	fprintf(stderr, "change prefs on the fly!\n");
    }
}

static gboolean
oss_open (int mode)
{
    int i;
    int mf;

    if(mode == ST_DRIVER_OPEN_FOR_PLAYING) {
	if((soundfd = open("/dev/dsp", O_WRONLY)) < 0) {
	    perror("Couldn't open /dev/dsp for sound output");
	    goto out;
	}

	bits = 0;
	mf = 0;
	if(p_resolution == 16) {
	    if(oss_try_format(AFMT_S16_LE)) {
		bits = 16;
		mf = ST_MIXER_FORMAT_S16_LE;
	    } else if(oss_try_format(AFMT_S16_BE)) {
		bits = 16;
		mf = ST_MIXER_FORMAT_S16_BE;
	    } else if(oss_try_format(AFMT_U16_LE)) {
		bits = 16;
		mf = ST_MIXER_FORMAT_U16_LE;
	    } else if(oss_try_format(AFMT_U16_BE)) {
		bits = 16;
		mf = ST_MIXER_FORMAT_U16_BE;
	    }
	}
	if(bits != 16) {
	    if(oss_try_format(AFMT_S8)) {
		bits = 8;
		mf = ST_MIXER_FORMAT_S8;
	    } else if(oss_try_format(AFMT_U8)) {
		bits = 8;
		mf = ST_MIXER_FORMAT_U8;
	    } else {
		fprintf(stderr, "Required sound output format not supported.\n");
		goto out;
	    }
	}

	if(p_channels == 2 && oss_try_stereo(1)) {
	    stereo = 1;
	} else if(oss_try_stereo(0)) {
	    stereo = 0;
	}

	mixer_mix_format(mf, stereo);

	playrate = p_mixfreq;
	ioctl(soundfd, SNDCTL_DSP_SPEED, &playrate);
	mixer_set_frequency(playrate);
	
	i = 0x00020000 + p_fragsize + stereo + (bits / 8 - 1);
	ioctl(soundfd, SNDCTL_DSP_SETFRAGMENT, &i);
	ioctl(soundfd, SNDCTL_DSP_GETBLKSIZE, &fragsize);

	sndbuf = calloc(1, fragsize);

	if(stereo == 1) {
	    fragsize /= 2;
	}
	if(bits == 16) {
	    fragsize /= 2;
	}

	polltag = audio_poll_add(soundfd, GDK_INPUT_WRITE, oss_poll_ready_playing);
	firstpoll = TRUE;

	return TRUE;
    } else if(mode == ST_DRIVER_OPEN_FOR_SAMPLING) {
	if((soundfd = open("/dev/dsp", O_RDONLY, 0)) < 0) {
	    perror("Couldn't open /dev/dsp for sampling");
	    goto out;
	}

	if(!oss_try_format(16)) {
	    fprintf(stderr, "oss.c: sampling only supported in S16_LE.\n");
	    goto out;
	}

	if(!oss_try_stereo(0)) {
	    fprintf(stderr, "oss.c: sampling only supported in Mono.\n");
	    goto out;
	}
	
	playrate = 44100;
	ioctl(soundfd, SNDCTL_DSP_SPEED, &playrate);
	if(playrate != 44100) {
	    fprintf(stderr, "oss.c: sampling only supported in 44100 Hz.\n");
	    goto out;
	}

	i = 0x0004000a;
	ioctl(soundfd, SNDCTL_DSP_SETFRAGMENT, &i);
	ioctl(soundfd, SNDCTL_DSP_GETBLKSIZE, &fragsize);

	sndbuf = malloc(2 * fragsize);

	polltag = audio_poll_add(soundfd, GDK_INPUT_READ, oss_poll_ready_sampling);
	firstpoll = TRUE;

	return TRUE;
    }

  out:
    oss_release();
    return FALSE;
}

static void
oss_release (void)
{
    free(sndbuf);
    sndbuf = NULL;

    audio_poll_remove(polltag);
    polltag = NULL;

    if(soundfd >= 0) {
	close(soundfd);
	soundfd = -1;
    }
}

static void
oss_prepare (void)
{
    mixer_prepare();
    current_time = 0;
    nsamples = 0;
    bufpos = 0;
    buftime = buftime2 = 0;
}

/* Call the mixer. Return TRUE when buffer is full. */
static gboolean
oss_mix (void)
{
    int m;

    m = nsamples > (fragsize - bufpos) ? fragsize - bufpos : nsamples;

    if(bits == 16) {
	mixer_mix((gint16*)sndbuf + bufpos * (stereo + 1), m);
    } else {
	mixer_mix((gint8*)sndbuf + bufpos * (stereo + 1), m);
    }
    bufpos += m;
    nsamples -= m;

    return nsamples != 0;
}

static gboolean
oss_sync (double time)
{
    nsamples = (time - current_time) * playrate;
    current_time += (double)nsamples / playrate;
    return oss_mix();
}

static void
oss_poll_ready_playing (gpointer data,
			gint source,
			GdkInputCondition condition)
{
    int w;
    int size;

    if(firstpoll) {
	firstpoll = FALSE;
    } else {
	size = (stereo + 1) * (bits / 8) * fragsize;

	g_assert(bufpos == fragsize);

	if((w = write(soundfd, sndbuf, size) != size)) {
	    if(w == -1) {
		fprintf(stderr, "driver_oss: write() returned -1.\n");
	    } else {
		fprintf(stderr, "driver_oss: write not completely done.\n");
	    }
	}

	audio_time(buftime);
	buftime = buftime2;
	buftime2 = current_time;
    }
    
    bufpos = 0;
    if(!oss_mix()) {
	audio_play();
    }
}

static void
oss_poll_ready_sampling (gpointer data,
			 gint source,
			 GdkInputCondition condition)
{
    read(soundfd, sndbuf, fragsize * 2);
    audio_sampled(sndbuf, fragsize);
}

static void
oss_stop (void)
{
}

#endif /* DRIVER_OSS */
