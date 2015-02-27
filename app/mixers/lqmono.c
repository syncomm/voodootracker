/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	      	- Low-quality Mono Mixer -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "mixer.h"

static int num_channels, mixfreq, amp = 8;
static st_mixer_sample_info samples[16*128];  /* this is uncool. rewrite */
static gint32 *mixbuf = NULL;
static int mixbufsize = 0, clipflag;
static int stereo;

static void         lqmono_setnumch              (int numchannels);
static gboolean     lqmono_setsample             (guint16 sample,
						  st_mixer_sample_info *si);
static gboolean     lqmono_setmixformat          (int format);
static gboolean     lqmono_setstereo             (int on);
static void         lqmono_setmixfreq            (guint16 frequency);
static void         lqmono_setampfactor          (float amplification);
static gboolean     lqmono_getclipflag           (void);
static void         lqmono_reset                 (void);
static void         lqmono_startnote             (int channel,
						  guint16 sample);
static void         lqmono_stopnote              (int channel);
static void         lqmono_setsmplpos            (int channel,
						  guint32 offset);
static void         lqmono_setfreq               (int channel,
						  float frequency);
static void         lqmono_setvolume             (int channel,
						  float volume);
static void         lqmono_setpanning            (int channel,
						  float panning);
static void*        lqmono_mix                   (void *dest,
						  guint32 count,
						  gint8 *scopebufs[],
						  int scopebuf_offset);

typedef struct lqmono_channel {
    void *data;                 /* location of sample data */
    guint32 length;             /* length of sample (converted) */
    int type;                   /* 16 or 8 bits */

    int running;                /* this channel is active */
    guint32 current;            /* current playback position in sample (converted) */
    guint32 speed;              /* sample playback speed (converted) */

    guint32 loopstart;          /* loop start (converted) */
    guint32 loopend;            /* loop end (converted) */
    int loopflags;              /* 0 none, 1 forward, 2 pingpong */
    int direction;              /* current pingpong direction (+1 forward, -1 backward) */

    int volume;                 /* 0..64 */
    float panning;              /* -1.0 .. +1.0 */
} lqmono_channel;

static lqmono_channel channels[32];

#define ACCURACY           12         /* accuracy of the fixed point stuff */

st_mixer mixer_lqmono = {
    "lqmono",
    "Super low fidelity mixer module, mono+stereo",

    lqmono_setnumch,
    lqmono_setsample,
    lqmono_setmixformat,
    lqmono_setstereo,
    lqmono_setmixfreq,
    lqmono_setampfactor,
    lqmono_getclipflag,
    lqmono_reset,
    lqmono_startnote,
    lqmono_stopnote,
    lqmono_setsmplpos,
    lqmono_setfreq,
    lqmono_setvolume,
    lqmono_setpanning,
    lqmono_mix,

    NULL
};

static void
lqmono_setnumch (int n)
{
    g_assert(n >= 1 && n <= 32);

    num_channels = n;
}

static gboolean
lqmono_setsample (guint16 sample,
		  st_mixer_sample_info *si)
{
    g_assert(sample < 16*128);

    if(si) {
	memcpy(&samples[sample], si, sizeof(*si));
    } else {
	memset(&samples[sample], 0, sizeof(*si));
    }
    return TRUE;
}

static gboolean
lqmono_setmixformat (int format)
{
    if(format != 16)
	return FALSE;

    return TRUE;
}

static gboolean
lqmono_setstereo (int on)
{
    stereo = on;
    return TRUE;
}

static void
lqmono_setmixfreq (guint16 frequency)
{
    mixfreq = frequency;
}

static void
lqmono_setampfactor (float amplification)
{
    amp = 8 * amplification;
}

static gboolean
lqmono_getclipflag (void)
{
    return clipflag;
}

static void
lqmono_reset (void)
{
    memset(channels, 0, sizeof(channels));
}

static void
lqmono_startnote (int channel,
		  guint16 sample)
{
    lqmono_channel *c = &channels[channel];
    st_mixer_sample_info *s = &samples[sample];

    c->data = s->data;
    c->type = s->type;
    c->length = s->length << ACCURACY;
    c->running = 1;
    c->speed = 1;
    c->current = 0;
    c->loopstart = s->loopstart << ACCURACY;
    c->loopend = s->loopend << ACCURACY;
    c->loopflags = s->looptype;
    c->direction = 1;
}

static void
lqmono_stopnote (int channel)
{
    lqmono_channel *c = &channels[channel];

    c->running = 0;
}

static void
lqmono_setsmplpos (int channel,
		   guint32 offset)
{
    lqmono_channel *c = &channels[channel];

    if(offset < c->length >> ACCURACY) {
	c->current = offset << ACCURACY;
	c->direction = 1;
    } else {
	c->running = 0;
    }
}

static void
lqmono_setfreq (int channel,
		float frequency)
{
    lqmono_channel *c = &channels[channel];

    if(frequency > (0x7fffffff >> ACCURACY)) {
	frequency = (0x7fffffff >> ACCURACY);
    }

    c->speed = frequency * (1 << ACCURACY) / mixfreq;
}

static void
lqmono_setvolume (int channel,
		  float volume)
{
    lqmono_channel *c = &channels[channel];

    c->volume = 64 * volume;
}

static void
lqmono_setpanning (int channel,
		   float panning)
{
    lqmono_channel *c = &channels[channel];

    c->panning = panning;
}

static void *
lqmono_mix (void *dest,
	    guint32 count,
	    gint8 *scopebufs[],
	    int scopebuf_offset)
{
    int todo;
    int i, j, t, *m, v;
    lqmono_channel *c;
    int done, s;
    int offs2end, oflcnt, looplen;
    int val;
    gint16 *sndbuf;
    gint8 *scopedata;
    int panleftmul = 0;
    int panrightmul = 0;

    if(count > mixbufsize) {
	free(mixbuf);
	mixbuf = malloc((stereo + 1) * 4 * count);
    }
    memset(mixbuf, 0, (stereo + 1) * 4 * count);

    for(i = 0; i < num_channels; i++) {
	c = &channels[i];
	t = count;
	m = mixbuf;
	v = c->volume;

	scopedata = scopebufs[i] + scopebuf_offset;

	if(!c->running) {
	    memset(scopedata, 0, count);
	    continue;
	}

	while(t) {
	    /* Check how much of the sample we can fill in one run */
	    if(c->loopflags) {
		looplen = c->loopend - c->loopstart;
		g_assert(looplen > 0);
		if(c->loopflags == ST_MIXER_SAMPLE_LOOPTYPE_AMIGA) {
		    offs2end = c->loopend - c->current;
		    if(offs2end <= 0) {
			oflcnt = - offs2end / looplen;
			offs2end += oflcnt * looplen;
			c->current = c->loopstart - offs2end;
			offs2end = c->loopend - c->current;
		    }			
		} else /* if(c->loopflags == ST_MIXER_SAMPLE_LOOPTYPE_PINGPONG) */ {
		    if(c->direction == 1)
			offs2end = c->loopend - c->current;
		    else 
			offs2end = c->current - c->loopstart;
		    
		    if(offs2end <= 0) {
			oflcnt = - offs2end / looplen;
			offs2end += oflcnt * looplen;
			if((oflcnt && 1) ^ (c->direction == -1)) {
			    c->current = c->loopstart - offs2end;
			    offs2end = c->loopend - c->current;
			    c->direction = 1;
			} else {
			    c->current = c->loopend + offs2end;
			    if(c->current == c->loopend)
				c->current--;
			    offs2end = c->current - c->loopstart;
			    c->direction = -1;
			}
		    }
		}
		g_assert(offs2end >= 0);
		g_assert(c->speed != 0);
		done = offs2end / c->speed + 1;
	    } else /* if(c->loopflags == LOOP_NO) */ {
		done = (c->length - c->current) / c->speed;
		if(!done) {
		    c->running = 0;
		    break;
		}
	    }

	    g_assert(done > 0);

	    if(done > t)
		done = t;
	    t -= done;

	    g_assert(c->current >= 0 && (c->current >> ACCURACY) < c->length);

	    if(stereo) {
		panleftmul = 64 - ((c->panning + 1.0) * 32);
		panrightmul = (c->panning + 1.0) * 32;
	    }

	    /* This one does the actual mixing */
	    /* One of the few occasions where templates would be really useful :) */
	    if(c->type == ST_MIXER_SAMPLE_TYPE_16) {
		gint16 *data = c->data;
		if(stereo) {
		    for(j = c->current, s = c->speed * c->direction; done; done--, j += s) {
			val = v * data[j >> ACCURACY];
			*m++ += panleftmul * val >> 6;
			*m++ += panrightmul * val >> 6;
			*scopedata++ = val >> 8 >> 6;
		    }
		} else {
		    for(j = c->current, s = c->speed * c->direction; done; done--, j += s) {
			val = v * data[j >> ACCURACY];
			*m++ += val;
			*scopedata++ = val >> 8 >> 6;
		    }
		}
	    } else {
		gint8 *data = c->data;
		if(stereo) {
		    for(j = c->current, s = c->speed * c->direction; done; done--, j += s) {
			val = v * (data[j >> ACCURACY] << 8);
			*m++ += panleftmul * val >> 6;
			*m++ += panrightmul * val >> 6;
			*scopedata++ = val >> 8 >> 6;
		    }
		} else {
		    for(j = c->current, s = c->speed * c->direction; done; done--, j += s) {
			val = v * (data[j >> ACCURACY] << 8);
			*m++ += val;
			*scopedata++ = val >> 8 >> 6;
		    }
		}
	    }
		
	    c->current = j;
	}
    }

    /* modules with many channels get additional amplification here */
    t = (4 * log(num_channels) / log(4)) * 64 * 8;

    for(sndbuf = dest, clipflag = 0, todo = 0; todo < (stereo + 1) * count; todo++) {
	gint32 a, b;

	a = mixbuf[todo];
	a *= amp;                  /* amplify */
	a /= t;

	b = MAX(-32768, MIN(32767, a));
	if(a != b) {
	    clipflag = 1;
	}

	*sndbuf++ = b;
    }

    return dest + (stereo + 1) * 2 * count;
}
