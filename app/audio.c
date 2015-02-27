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

#include <config.h>

#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>
#include "poll.h"

#include <glib.h>

#include "audio.h"
#include "mixer.h"
#include "driver.h"
#include "main.h"
#include "xm-player.h"
#include "endian-conv.h"



int audio_ctlpipe, audio_backpipe;
gint8 player_mute_channels[32];

/* Sampler ring buffer */

gint16 *audio_samplebuf;
int audio_samplebuf_start, audio_samplebuf_end, audio_samplebuf_len = 131072;

/* Player position ring buffer */

audio_player_pos *audio_playerpos = NULL;
int audio_playerpos_start, audio_playerpos_end, audio_playerpos_len = 64;
double audio_playerpos_realtime, audio_playerpos_songtime;

/* Internal variables */

static int ctlpipe, backpipe;
static pthread_t threadid;
static st_driver *driver = NULL;
static int playing = 0;

static st_mixer *mixer = NULL;
static int mixfmt, mixfmt_conv;
static int mixfmt_req, stereo_req, mixfreq_req;

static double pitchbend = +0.0, last_synctime_driver, last_synctime_player;

static pthread_mutex_t audio_locks[AUDIO_LOCK_LAST];

static int set_songpos_wait_for = -1;

#define MIXFMT_16                   1
#define MIXFMT_STEREO               2

#define MIXFMT_CONV_TO_16           1
#define MIXFMT_CONV_TO_8            2
#define MIXFMT_CONV_TO_UNSIGNED     4
#define MIXFMT_CONV_TO_STEREO       8
#define MIXFMT_CONV_TO_MONO        16
#define MIXFMT_CONV_BYTESWAP       32

typedef struct PollInput {
    int fd;
    GdkInputCondition condition;
    GdkInputFunction function;
} PollInput;

static GList *inputs = NULL;

gint8 *scopebufs[32];
double scopebuftime;
int scopebuf_freq;
int scopebuf_len;
int scopebufpt;

int mixer_clipping;

static void
audio_ctlpipe_set_driver (st_driver *d)
{
    g_assert(d != NULL);
    g_assert(!playing);
    
    driver = d;
}

static void
audio_ctlpipe_set_mixer (st_mixer *m)
{
    g_assert(m != NULL);
    g_assert(!playing);
    
    mixer = m;
}

static void
audio_ctlpipe_init_player (void)
{
    g_assert(xm != NULL);

    xmplayer_init_module();
}

static void
audio_set_driver_prefs (st_audio_playback_prefs_types t)
{
    st_audio_playback_prefs *prefs = &st_preferences.audio_playback[t];

    if(prefs->use_from_other) {
	prefs = &st_preferences.audio_playback[t ^ 1];
    }

    driver->setprefs(prefs);
}

static void
audio_ctlpipe_play_song (int songpos,
			 int patpos)
{
    audio_backpipe_id a;

    g_assert(driver != NULL);
    g_assert(mixer != NULL);
    g_assert(xm != NULL);
    g_assert(!playing);

    audio_set_driver_prefs(AUDIO_PLAYBACK_PREFS_TYPE_PLAYING);
    if(driver->open(ST_DRIVER_OPEN_FOR_PLAYING)) {
	driver->prepare();
	playing = 1;
	last_synctime_driver = 0.0;
	last_synctime_player = 0.0;
	xmplayer_init_play_song(songpos, patpos);
	a = AUDIO_BACKPIPE_PLAYING_STARTED;
    } else {
	a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
    }

    audio_playerpos_start = audio_playerpos_end = 0;

    write(backpipe, &a, sizeof(a));
}

static void
audio_ctlpipe_play_pattern (int pattern,
			    int patpos)
{
    audio_backpipe_id a;

    g_assert(driver != NULL);
    g_assert(mixer != NULL);
    g_assert(xm != NULL);
    g_assert(!playing);

    audio_set_driver_prefs(AUDIO_PLAYBACK_PREFS_TYPE_PLAYING);
    if(driver->open(ST_DRIVER_OPEN_FOR_PLAYING)) {
	driver->prepare();
	playing = 1;
	last_synctime_driver = 0.0;
	last_synctime_player = 0.0;
	xmplayer_init_play_pattern(pattern, patpos);
	a = AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED;
    } else {
	a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
    }

    audio_playerpos_start = audio_playerpos_end = 0;

    write(backpipe, &a, sizeof(a));
}

static void
audio_ctlpipe_play_note (int channel,
			 int note,
			 int instrument)
{
    audio_backpipe_id a = AUDIO_BACKPIPE_PLAYING_NOTE_STARTED;

    if(!playing) {
	driver->setprefs(&st_preferences.audio_playback[AUDIO_PLAYBACK_PREFS_TYPE_EDITING]);
	if(driver->open(ST_DRIVER_OPEN_FOR_PLAYING)) {
	    driver->prepare();
	    playing = 1;
	    last_synctime_driver = 0.0;
	    last_synctime_player = 0.0;
	} else {
	    a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
	}
    }

    write(backpipe, &a, sizeof(a));

    if(!playing)
	return;

    xmplayer_play_note(channel, note, instrument);
}

static void
audio_ctlpipe_stop_playing (void)
{
    audio_backpipe_id a = AUDIO_BACKPIPE_PLAYING_STOPPED;

    if(playing == 1) {
	xmplayer_stop();
	driver->stop();
	driver->release();
	playing = 0;
    }

    g_assert(set_songpos_wait_for == -1);

    write(backpipe, &a, sizeof(a));
}

static void
audio_ctlpipe_set_songpos (int songpos)
{
    g_assert(playing);

    xmplayer_set_songpos(songpos);
    if(set_songpos_wait_for != -1) {
	/* confirm previous request */
	audio_backpipe_id a = AUDIO_BACKPIPE_SONGPOS_CONFIRM;
	double dummy;
	write(backpipe, &a, sizeof(a));
	write(backpipe, &dummy, sizeof(dummy));
    }
    set_songpos_wait_for = songpos;
}

static void
audio_ctlpipe_set_pattern (int pattern)
{
    g_assert(playing);

    xmplayer_set_pattern(pattern);
}

static void
audio_ctlpipe_set_amplification (float af)
{
    g_assert(mixer != NULL);

    mixer->setampfactor(af);
}

static void
audio_ctlpipe_sample (void)
{
    audio_backpipe_id a = AUDIO_BACKPIPE_SAMPLING_STARTED;

    g_assert(!playing);

    if(driver->open(ST_DRIVER_OPEN_FOR_SAMPLING)) {
    } else {
	a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
    }

    audio_samplebuf = malloc(2 * audio_samplebuf_len);
    audio_samplebuf_start = audio_samplebuf_end = 0;

    write(backpipe, &a, sizeof(a));
}

static void
audio_ctlpipe_stop_sampling (void)
{
    audio_backpipe_id a = AUDIO_BACKPIPE_SAMPLING_STOPPED;

    driver->release();

    /* scope-group.c might still be working on the audio buffer, so lock it */
    audio_lock(AUDIO_LOCK_SAMPLEBUF);
    free(audio_samplebuf);
    audio_samplebuf = NULL;
    audio_unlock(AUDIO_LOCK_SAMPLEBUF);

    write(backpipe, &a, sizeof(a));
}

/* read()s on pipes are always non-blocking, so when we want to read 8
   bytes, we might only get 4. this routine is a workaround. seems
   like i can't put a pipe into blocking mode. alternatively the other
   end of the pipe could send packets in one big chunk instead of
   using single write()s. */
void
readpipe (int fd, void *p, int count)
{
    struct pollfd pfd =	{ fd, POLLIN, 0 };
    int n;

    while(count) {
	n = read(fd, p, count);
	count -= n;
	p += n;
	if(count != 0) {
	    pfd.revents = 0;
	    poll(&pfd, 1, -1);
	}
    }
}

static void
audio_thread (void)
{
    struct pollfd pfd[5] = {
	{ ctlpipe, POLLIN, 0 },
    };
    GList *pl;
    PollInput *pi;
    audio_ctlpipe_id c;
    int a[3], i, npl;
    float af;
    gpointer p;

    nice(-10);

  loop:
    pfd[0].revents = 0;

    for(pl = inputs, npl = 1; pl; pl = pl->next, npl++) {
	pi = pl->data;
	if(pi->fd == -1) {
	    inputs = g_list_remove(inputs, pi);
	    goto loop;
	}
	pfd[npl].events = pfd[npl].revents = 0;
	pfd[npl].fd = pi->fd;
	if(pi->condition & GDK_INPUT_READ)
	    pfd[npl].events |= POLLIN;
	if(pi->condition & GDK_INPUT_WRITE)
	    pfd[npl].events |= POLLOUT;
    }

    if(poll(pfd, npl, -1) == -1) {
	perror("audio_thread:poll():");
	pthread_exit(NULL);
    }

    if(pfd[0].revents & POLLIN) {
	readpipe(ctlpipe, &c, sizeof(c));
	switch(c) {
	case AUDIO_CTLPIPE_SET_DRIVER:
	    read(ctlpipe, &p, sizeof(p));
	    audio_ctlpipe_set_driver(p);
	    break;
	case AUDIO_CTLPIPE_SET_MIXER:
	    read(ctlpipe, &p, sizeof(p));
	    audio_ctlpipe_set_mixer(p);
	    break;
	case AUDIO_CTLPIPE_INIT_PLAYER:
	    audio_ctlpipe_init_player();
	    break;
	case AUDIO_CTLPIPE_PLAY_SONG:
	    readpipe(ctlpipe, a, 2 * sizeof(a[0]));
	    audio_ctlpipe_play_song(a[0], a[1]);
	    break;
	case AUDIO_CTLPIPE_PLAY_PATTERN:
	    readpipe(ctlpipe, a, 2 * sizeof(a[0]));
	    audio_ctlpipe_play_pattern(a[0], a[1]);
	    break;
	case AUDIO_CTLPIPE_PLAY_NOTE:
	    readpipe(ctlpipe, a, 3 * sizeof(a[0]));
	    audio_ctlpipe_play_note(a[0], a[1], a[2]);
	    break;
	case AUDIO_CTLPIPE_STOP_PLAYING:
	    audio_ctlpipe_stop_playing();
	    break;
	case AUDIO_CTLPIPE_SET_SONGPOS:
	    read(ctlpipe, a, 1 * sizeof(a[0]));
	    audio_ctlpipe_set_songpos(a[0]);
	    break;
	case AUDIO_CTLPIPE_SET_PATTERN:
	    read(ctlpipe, a, 1 * sizeof(a[0]));
	    audio_ctlpipe_set_pattern(a[0]);
	    break;
	case AUDIO_CTLPIPE_SET_AMPLIFICATION:
	    read(ctlpipe, &af, sizeof(af));
	    audio_ctlpipe_set_amplification(af);
	    break;
	case AUDIO_CTLPIPE_SET_PITCHBEND:
	    read(ctlpipe, &af, sizeof(af));
	    pitchbend = af;
	    break;
	case AUDIO_CTLPIPE_SAMPLE:
	    audio_ctlpipe_sample();
	    break;
	case AUDIO_CTLPIPE_STOP_SAMPLING:
	    audio_ctlpipe_stop_sampling();
	    break;
	default:
	    fprintf(stderr, "\n\n*** audio_thread: unknown ctlpipe id %d\n\n\n", c);
	    pthread_exit(NULL);
	    break;
	}
    }

    for(pl = inputs, i = 1; i < npl; pl = pl->next, i++) {
	pi = pl->data;
	if(pi->fd == -1)
	    continue;
	if(pfd[i].revents & pfd[i].events) {
	    int x = 0;
	    if(pfd[i].revents & POLLIN)
		x |= GDK_INPUT_READ;
	    if(pfd[i].revents & POLLOUT)
		x |= GDK_INPUT_WRITE;
	    pi->function(NULL, pi->fd, x);
	}
    }

    goto loop;
}

gboolean
audio_init (int c,
	    int b)
{
    int i;

    ctlpipe = c;
    backpipe = b;

    scopebuf_len = 10000;
    for(i = 0; i < 32; i++) {
	scopebufs[i] = malloc(scopebuf_len);
	if(!scopebufs[i])
	    return FALSE;
    }

    memset(player_mute_channels, 0, sizeof(player_mute_channels));

    audio_playerpos = malloc(audio_playerpos_len * sizeof(audio_player_pos));
    if(!audio_playerpos)
	return FALSE;

    for(i = 0; i < AUDIO_LOCK_LAST; i++)
	pthread_mutex_init(&audio_locks[i], NULL);

    if(0 == pthread_create(&threadid, NULL, (void*(*)(void*))audio_thread, NULL))
	return TRUE;

    return FALSE;
}

void
audio_lock (audio_lock_id i)
{
    pthread_mutex_lock(&audio_locks[i]);
}

void
audio_unlock (audio_lock_id i)
{
    pthread_mutex_unlock(&audio_locks[i]);
}

void
mixer_mix_format (STMixerFormat m, int s)
{
    mixfmt_req = m;
    stereo_req = s;

    if(!mixer)
	return;

    mixfmt_conv = 0;
    mixfmt = 0;

    switch(m) {
    case ST_MIXER_FORMAT_S16_BE:
    case ST_MIXER_FORMAT_S16_LE:
    case ST_MIXER_FORMAT_U16_BE:
    case ST_MIXER_FORMAT_U16_LE:
	if(mixer->setmixformat(16)) {
	    mixfmt = MIXFMT_16;
	} else if(mixer->setmixformat(8)) {
	    mixfmt_conv |= MIXFMT_CONV_TO_16;
	} else {
	    g_error("Weird mixer. No 8 or 16 bits modes.\n");
	}
	break;
    case ST_MIXER_FORMAT_S8:
    case ST_MIXER_FORMAT_U8:
	if(mixer->setmixformat(8)) {
	} else if(mixer->setmixformat(16)) {
	    mixfmt = MIXFMT_16;
	    mixfmt_conv |= MIXFMT_CONV_TO_8;
	} else {
	    g_error("Weird mixer. No 8 or 16 bits modes.\n");
	}
	break;
    default:
	g_error("Unknown argument for STMixerFormat.\n");
	break;
    }

    if(mixfmt & MIXFMT_16) {
	switch(m) {
#ifdef WORDS_BIGENDIAN
	case ST_MIXER_FORMAT_S16_LE:
	case ST_MIXER_FORMAT_U16_LE:
#else
	case ST_MIXER_FORMAT_S16_BE:
	case ST_MIXER_FORMAT_U16_BE:
#endif
	    mixfmt_conv |= MIXFMT_CONV_BYTESWAP;
	    break;
	default:
	    break;
	}
    }

    switch(m) {
    case ST_MIXER_FORMAT_U16_LE:
    case ST_MIXER_FORMAT_U16_BE:
    case ST_MIXER_FORMAT_U8:
	mixfmt_conv |= MIXFMT_CONV_TO_UNSIGNED;
	break;
    default:
	break;
    }

    mixfmt |= s ? MIXFMT_STEREO : 0;
    if(!mixer->setstereo(s)) {
	if(s) {
	    mixfmt &= ~MIXFMT_STEREO;
	    mixfmt_conv |= MIXFMT_CONV_TO_STEREO;
	} else {
	    mixfmt_conv |= MIXFMT_CONV_TO_MONO;
	}
    }
}

void
mixer_set_frequency (guint16 f)
{
    mixfreq_req = f;

    if(mixer) {
	mixer->setmixfreq(f);
    }
}

void
mixer_prepare (void)
{
    g_assert(mixer != NULL);

    mixer->reset();
    mixer_mix_format(mixfmt_req, stereo_req);
    mixer_set_frequency(mixfreq_req);

    scopebuftime = 0;
    scopebufpt = 0;
    scopebuf_freq = mixfreq_req;
}

void
mixer_mix_and_handle_scopes (void *dest,
			     guint32 count)
{
    int n;

    /* The scope data is stored on a per-channel basis in 32 ring
       buffers (finite circular lists in array form). The main code
       (scope-group.c) gets notified through the audio_time()
       mechanism when a specific sample is being played; the correct
       ring position is calculated there. */

    while(count) {
	n = scopebuf_len - scopebufpt;
	if(n > count)
	    n = count;

	dest = mixer->mix(dest, n, scopebufs, scopebufpt);
	scopebufpt += n;
	count -= n;

	if(scopebufpt == scopebuf_len) {
	    scopebuftime += (double)scopebuf_len / mixfreq_req;
	    scopebufpt = 0;
	}
    }

    mixer_clipping = mixer->getclipflag();
}

void
mixer_mix (void *dest,
	   guint32 count)
{
    static int bufsize = 0;
    static void *buf = NULL;
    int b, i, c, d;

    if(count == 0)
	return;

    g_assert(mixer != NULL);

    /* The mixer doesn't have to support a format that the driver
       requires. This routine converts between any formats if
       necessary: 16 bits / 8 bits, mono / stereo, little endian / big
       endian, unsigned / signed */

    if(mixfmt_conv == 0) {
	mixer_mix_and_handle_scopes(dest, count);
	return;
    }

    {
	static int l=-1, m=-1;
	if(l != mixfmt_conv || m != mixfmt) {
	    l = mixfmt_conv;
	    m = mixfmt;

	    printf("audio.c: mixer_mix() conv %d/%d. Please tell me when sound output is not okay!\n", mixfmt, mixfmt_conv);
	}
    }

    b = count;
    c = 8;
    d = 1;
    if(mixfmt & MIXFMT_16) {
	b *= 2;
	c = 16;
    }
    if(mixfmt & MIXFMT_STEREO) {
	b *= 2;
	d = 2;
    }

    if(b > bufsize) {
	bufsize = b;
	free(buf);
	buf = malloc(bufsize);
    }

    g_assert(buf != NULL);
    mixer_mix_and_handle_scopes(buf, count);

    if(mixfmt_conv & MIXFMT_CONV_TO_MONO) {
	if(mixfmt & MIXFMT_16) {
	    gint16 *a = buf, *b = buf;
	    for(i = 0; i < count; i++, a+=2, b+=1)
		*b = (a[0] + a[1]) / 2;
	} else {
	    gint8 *a = buf, *b = buf;
	    for(i = 0; i < count; i++, a+=2, b+=1)
		*b = (a[0] + a[1]) / 2;
	}
	d = 1;
    }

    if(mixfmt_conv & MIXFMT_CONV_TO_8) {
	gint16 *a = buf;
	gint8 *b = dest;
	for(i = 0; i < count * d; i++)
	    *b++ = *a++ >> 8;
	c = 8;
    } else if(mixfmt_conv & MIXFMT_CONV_TO_16) {
	gint8 *a = buf;
	gint16 *b = dest;
	for(i = 0; i < count * d; i++)
	    *b++ = *a++ << 8;
	c = 16;
    } else {
	memcpy(dest, buf, count * d * (c / 8));
    }
    
    if(mixfmt_conv & MIXFMT_CONV_TO_UNSIGNED) {
	if(c == 16) {
	    gint16 *a = dest;
	    guint16 *b = dest;
	    for(i = 0; i < count * d; i++)
		*b++ = *a++ + 32768;
	} else {
	    gint8 *a = dest;
	    guint8 *b = dest;
	    for(i = 0; i < count * d; i++)
		*b++ = *a++ + 128;
	}
    }

    if(mixfmt_conv & MIXFMT_CONV_BYTESWAP) {
	byteswap_16_array(dest, count * d);
    }

    if(mixfmt_conv & MIXFMT_CONV_TO_STEREO) {
	g_assert(d == 1);
	if(c == 16) {
	    gint16 *a = dest, *b = dest;
	    for(i = 0, a += count, b += 2 * count; i < count; i++, a-=1, b-=2)
	        b[-1] = b[-2] = a[-1];
	} else {
	    gint8 *a = dest, *b = dest;
	    for(i = 0, a += count, b += 2 * count; i < count; i++, a-=1, b-=2)
	        b[-1] = b[-2] = a[-1];
	}
    }
}

void
driver_setnumch (int numchannels)
{
    mixer->setnumch(numchannels);
}

gboolean
driver_setsample (guint16 sample,
		  st_mixer_sample_info *si)
{
    return mixer->setsample(sample, si);
}

void
driver_startnote (int channel,
		  guint16 sample)
{
    mixer->startnote(channel, sample);
}

void
driver_stopnote (int channel)
{
    mixer->stopnote(channel);
}

void
driver_setsmplpos (int channel,
		   guint32 offset)
{
    mixer->setsmplpos(channel, offset);
}

void
driver_setfreq (int channel,
		float frequency)
{
    mixer->setfreq(channel, frequency * ((100.0 + pitchbend) / 100.0));
}

void
driver_setvolume (int channel,
		  float volume)
{
    g_assert(volume >= 0.0 && volume <= 1.0);
    
    mixer->setvolume(channel, volume);
}

void
driver_setpanning (int channel,
		   float panning)
{
    g_assert(panning >= -1.0 && panning <= +1.0);

    mixer->setpanning(channel, panning);
}

gpointer
audio_poll_add (int fd,
		GdkInputCondition cond,
		GdkInputFunction func)
{
    PollInput *input;

    g_assert(g_list_length(inputs) < 5);

    input = g_new(PollInput, 1);
    input->fd = fd;
    input->condition = cond;
    input->function = func;

    inputs = g_list_prepend(inputs, input);
    return input;
}

void
audio_poll_remove (gpointer input)
{
    if(input) {
	((PollInput*)input)->fd = -1;
    }
}

static gboolean
driver_sync (double time)
{

    /* A call of this function indicates that the next driver_
       commands should come into effect just at the next tick of the
       music specified by 'time'. These three lines, and the stuff in
       driver_setfreq() contains all necessary code to handle the
       pitchbending feature. */

    last_synctime_driver += (time - last_synctime_player) * (100.0 / (100.0 + pitchbend));
    last_synctime_player = time;
    return driver->sync(last_synctime_driver);
}

void
audio_play (void)
{
    double t;
    audio_player_pos *p;

    do {
	p = &audio_playerpos[audio_playerpos_end];
	p->time = last_synctime_driver;
	p->songpos = player_songpos;
	p->patpos = player_patpos;
	if(++audio_playerpos_end == audio_playerpos_len) {
	    audio_playerpos_end = 0;
	}
	t = xmplayer_play();

	if(set_songpos_wait_for != -1 && p->songpos == set_songpos_wait_for) {
	    audio_backpipe_id a = AUDIO_BACKPIPE_SONGPOS_CONFIRM;
	    set_songpos_wait_for = -1;
	    write(backpipe, &a, sizeof(a));
	    write(backpipe, &p->time, sizeof(p->time));
	}
    } while(!driver_sync(t));
}

void
audio_time (double time)
{
    struct timeval tv;

    audio_lock(AUDIO_LOCK_PLAYER_TIME);
    gettimeofday(&tv, NULL);
    audio_playerpos_songtime = time;
    audio_playerpos_realtime = tv.tv_sec + tv.tv_usec / 1e6;
    audio_unlock(AUDIO_LOCK_PLAYER_TIME);
}

void
audio_sampled (gint16 *data,
	       int count)
{
    /* assuming S16_LE mono 44.1kHz */
    audio_backpipe_id a = AUDIO_BACKPIPE_SAMPLING_TICK;
    int b = count;

    g_assert(audio_samplebuf != NULL);

    /* Just like the oscilloscopes, sampling uses a ring buffer,
       notifying the main thread when new data has arrived. The main
       thread (sample-editor.c, in this case) is responsible for
       handling this data. */

    while(count) {
	int n = audio_samplebuf_len - audio_samplebuf_end;
	if(n > count)
	    n = count;
	memcpy(audio_samplebuf + audio_samplebuf_end,
	       data,
	       2 * n);
	count -= n;
	data += n;
	audio_samplebuf_end += n;
	if(audio_samplebuf_end == audio_samplebuf_len)
	    audio_samplebuf_end = 0;
    }

    write(backpipe, &a, sizeof(a));
    write(backpipe, &b, sizeof(b));
}
