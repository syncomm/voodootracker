/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       	  - XM Support Routines -			 	 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "xm.h"
#include "endian-conv.h"
#include "st-subs.h"
#include "recode.h"

static guint16 npertab[60]={
    /* -> Tuning 0 */
    1712,1616,1524,1440,1356,1280,1208,1140,1076,1016, 960, 906,
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
    428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
    107, 101,  95,  90,  85,  80,  75,  71,  67,  63,  60,  56
};

void
byteswap_16_array (gint16 *data,
		   int count)
{
#ifdef WORDS_BIGENDIAN
    for(; count; count--, data++) {
	gint8 *p = (gint16*)data;
	gint8 a = p[0];
	p[0] = p[1];
	p[1] = a;
    }
#endif
}

static void
xm_load_xm_note (XMNote *note,
		 FILE *f)
{
    guint8 c, d[4];
   
    note->note = 0;
    note->instrument = 0;
    note->volume = 0;
    note->fxtype = 0;
    note->fxparam = 0;

    c = fgetc(f);

    if(c & 0x80) {
	if(c & 0x01)
	    note->note = fgetc(f);
	if(c & 0x02)
	    note->instrument = fgetc(f);
	if(c & 0x04)
	    note->volume = fgetc(f);
	if(c & 0x08)
	    note->fxtype = fgetc(f);
	if(c & 0x10)
	    note->fxparam = fgetc(f);
    } else {
	fread(d, 1, sizeof(d), f);
	note->note = c;
	note->instrument = d[0];
	note->volume = d[1];
	note->fxtype = d[2];
	note->fxparam = d[3];
    }
}

static int
xm_put_xm_note (XMNote *n,
		guint8 *b)
{
    int p = 0;
    guint8 x = 0x80;

    if(n->note != 0) {
	x |= 0x01;
	p++;
    }
    if(n->instrument != 0) {
	x |= 0x02;
	p++;
    }
    if(n->volume != 0) {
	x |= 0x04;
	p++;
    }
    if(n->fxtype != 0) {
	x |= 0x08;
	p++;
    }
    if(n->fxparam != 0) {
	x |= 0x10;
	p++;
    }

    if(p <= 3) { /* FastTracker compares against 4 here */
	*b++ = x;
	if(x & 0x01)
	    *b++ = n->note;
	if(x & 0x02)
	    *b++ = n->instrument;
	if(x & 0x04)
	    *b++ = n->volume;
	if(x & 0x08)
	    *b++ = n->fxtype;
	if(x & 0x10)
	    *b++ = n->fxparam;
	return p + 1;
    } else {
	*b++ = n->note;
	*b++ = n->instrument;
	*b++ = n->volume;
	*b++ = n->fxtype;
	*b++ = n->fxparam;
	return 5;
    }
}

static int
xm_load_xm_pattern (XMPattern *pat,
		    int num_channels,
		    FILE *f)
{
    guint8 ph[9];
    int i, j, len;

    fread(ph, 1, sizeof(ph), f);

    len = get_le_16(ph + 5);
    if(len > 256 || len < 1) {
	fprintf(stderr, "Pattern length out of range: %d.\n", len);
	return 0;
    }

    if(!st_init_pattern_channels(pat, len, num_channels))
	return 0;

    if(get_le_16(ph + 7) == 0)
	return 1;

    /* Read channel data */
    for(j = 0; j < len; j++) {
	for(i = 0; i < num_channels; i++) {
	    xm_load_xm_note(&pat->channels[i][j], f);
	}
    }

    return 1;
}


static int
xm_load_patterns (XMPattern ptr[],
		  int num_patterns,
		  int num_channels,
		  FILE *f,
		  int(*loadfunc)(XMPattern*,int,FILE*))
{
    int i, e;
    
    for(i = 0; i < 256; i++) {
	if(i < num_patterns)
	    e = loadfunc(&ptr[i], num_channels, f);
	else
	    e = st_init_pattern_channels(&ptr[i], 64, num_channels);

	if(!e)
	    return 0;
    }
    
    return 1;
}

static void
xm_save_xm_pattern (XMPattern *p,
		    int num_channels,
		    FILE *f)
{
    int i, j;
    guint8 sh[9];
    static guint8 buf[32 * 256 * 5];
    int bp;

    bp = 0;
    for(j = 0; j < p->length; j++) {
	for(i = 0; i < num_channels; i++) {
	    bp += xm_put_xm_note(&p->channels[i][j], buf + bp);
	}
    }

    put_le_32(sh + 0, 9);
    sh[4] = 0;
    put_le_16(sh + 5, p->length);
    put_le_16(sh + 7, bp);

    if(bp == p->length * num_channels) {
	/* pattern is empty */
	put_le_16(sh + 7, 0);
	fwrite(sh, 1, sizeof(sh), f);
    } else {
	fwrite(sh, 1, sizeof(sh), f);
	fwrite(buf, 1, bp, f);
    }
}

void
xm_load_xm_samples (STSample samples[],
		    int num_samples,
		    FILE *f)
{
    int i, j;
    guint8 sh[40];
    STSample *s;
    guint16 p;
    gint16 *d16;
    gint8 *d8;

    for(i = 0; i < num_samples; i++) {
	s = &samples[i];
	fread(sh, 1, sizeof(sh), f);
	s->length = get_le_32(sh + 0);
	s->loopstart = get_le_32(sh + 4);
	s->loopend = get_le_32(sh + 8); /* this is really the loop _length_ */
	s->volume = sh[12];
	s->finetune = sh[13];
	s->loop = sh[14];
	s->panning = sh[15];
	s->relnote = sh[16];
	strncpy(s->name, sh + 18, 22);
	s->name[22] = '\0';
	recode_ibmpc_to_latin1(s->name, 22);
    }

    for(i = 0; i < num_samples; i++) {
	s = &samples[i];
	if(s->length == 0) {
	    s->data = NULL;
	    continue;
	}
	s->type = s->loop & 0x10 ? 16 : 8;
	s->loop &= 3;

	if(s->type == 16) {
	    /* 16 bit sample */
	    s->length >>= 1;
	    s->loopstart >>= 1;
	    s->loopend >>= 1;

	    d16 = s->data = malloc(2 * s->length);
	    fread(d16, 1, 2 * s->length, f);
	    byteswap_16_array(d16, s->length);

	    for(j = s->length, p = 0; j; j--) {
		p += *d16;
		*d16++ = p;
	    }
	} else {
	    /* 8 bit sample */
	    d8 = s->data = malloc(s->length);
	    fread(d8, 1, s->length, f);

	    for(j = s->length, p = 0; j; j--) {
		p += *d8;
		*d8++ = p;
	    }
	}

	if(s->loopend == 0) {
	    s->loop = 0;
	} else {
	    s->loopend += s->loopstart; /* now it is the loop _end_ */
	}

	if(s->loop == LOOP_NONE) {
	    s->loopstart = 0;
	    s->loopend = 1;
	}
    }
}

void
xm_save_xm_samples (STSample samples[],
		    FILE *f,
		    int num_samples)
{
    int i, k;
    guint8 sh[40];
    STSample *s;

    for(i = 0; i < num_samples; i++) {
	/* save sample header */
	s = &samples[i];
	memset(sh, 0, sizeof(sh));
	put_le_32(sh + 0, s->length * (s->type / 8));
	put_le_32(sh + 4, s->loopstart * (s->type / 8));
	put_le_32(sh + 8, (s->loopend - s->loopstart) * (s->type / 8));
	sh[12] = s->volume;
	sh[13] = s->finetune;
	sh[14] = s->loop | (s->type == 16 ? 0x10 : 0);
	sh[15] = s->panning;
	sh[16] = s->relnote;
	sh[17] = 0;
	strncpy(sh + 18, s->name, 22);
	recode_latin1_to_ibmpc(sh + 18, 22);
	fwrite(sh, 1, sizeof(sh), f);
    }
    
    for(i = 0; i < num_samples; i++) {
	s = &samples[i];

	if(s->type == 16) {
	    gint16 *packbuf, *d16, *ss;
	    gint16 p, d;

	    packbuf = malloc(s->length * 2);
	    d16 = s->data;
	    ss = packbuf;

	    for(k = s->length, p = 0, d = 0; k; k--) {
		d = *d16 - p;
		*ss++ = d;
		p = *d16++;
	    }

	    fwrite(packbuf, 1, s->length * 2, f);
	    free(packbuf);
	} else {
	    gint8 *packbuf, *d8, *ss;
	    gint8 p, d;

	    packbuf = malloc(s->length);
	    d8 = s->data;
	    ss = packbuf;

	    for(k = s->length, p = 0, d = 0; k; k--) {
		d = *d8 - p;
		*ss++ = d;
		p = *d8++;
	    }

	    fwrite(packbuf, 1, s->length, f);
	    free(packbuf);
	}
    }
}

static void
xm_load_xm_instrument (STInstrument *instr,
		       FILE *f)
{
    guint8 a[29], b[38];
    int num_samples;

    st_clean_instrument(instr, NULL);

    fread(a, 1, sizeof(a), f);

    strncpy(instr->name, a + 4, 22);
    recode_ibmpc_to_latin1(instr->name, 22);
    num_samples = get_le_16(a + 27);

    if(num_samples == 0) {
	fread(a, 1, 4, f);
    } else {
	fread(a, 1, 4, f);
	if(get_le_32(a) != 40) {
	    fprintf(stderr, "instrument header size != 40\n");
	    exit(1);
	}
	fread(instr->samplemap, 1, 96, f);
	fread(instr->vol_env.points, 1, 48, f);
	fread(instr->pan_env.points, 1, 48, f);

	fread(b, 1, sizeof(b), f);
	instr->vol_env.num_points = b[0];
	instr->vol_env.sustain_point = b[2];
	instr->vol_env.loop_start = b[3];
	instr->vol_env.loop_end = b[4];
	instr->vol_env.flags = b[8];
	instr->pan_env.num_points = b[1];
	instr->pan_env.sustain_point = b[5];
	instr->pan_env.loop_start = b[6];
	instr->pan_env.loop_end = b[7];
	instr->pan_env.flags = b[9];

	instr->vibtype = b[10];
	if(instr->vibtype >= 4) {
	    instr->vibtype = 0;
	    fprintf(stderr, "Invalid vibtype %d, using Sine.\n", instr->vibtype);
	}
	instr->vibrate = b[13];
	instr->vibdepth = b[12];
	instr->vibsweep = b[11];
	
	instr->volfade = b[14];

	xm_load_xm_samples(instr->samples, num_samples, f);
    }
}

static void
xm_save_xm_instrument (STInstrument *instr,
		       FILE *f)
{
    guint8 header[29];
    guint8 b[38];
    int num_samples;

    num_samples = st_instrument_num_save_samples(instr);

    memset(header, 0, sizeof(header));
    strncpy(header + 4, instr->name, 22);
    recode_latin1_to_ibmpc(header + 4, 22);
    header[27] = num_samples;

    if(num_samples == 0) {
	header[0] = 33;
	header[1] = 0;
	fwrite(header, 1, 29, f);
	put_le_32(header, 40);
	fwrite(header, 1, 4, f);
	return;
    }

    put_le_16(header + 0, 263);
    fwrite(header, 1, 29, f);
    put_le_32(header, 40);
    fwrite(header, 1, 4, f);

    fwrite(instr->samplemap, 1, 96, f);
    fwrite(instr->vol_env.points, 1, 48, f);
    fwrite(instr->pan_env.points, 1, 48, f);

    memset(&b, 0, sizeof(b));
    b[0] = instr->vol_env.num_points;
    b[2] = instr->vol_env.sustain_point;
    b[3] = instr->vol_env.loop_start;
    b[4] = instr->vol_env.loop_end;
    b[8] = instr->vol_env.flags;
    b[1] = instr->pan_env.num_points;
    b[5] = instr->pan_env.sustain_point;
    b[6] = instr->pan_env.loop_start;
    b[7] = instr->pan_env.loop_end;
    b[9] = instr->pan_env.flags;

    b[10] = instr->vibtype;
    b[13] = instr->vibrate;
    b[12] = instr->vibdepth;
    b[11] = instr->vibsweep;
	
    b[14] = instr->volfade;

    fwrite(&b, 1, sizeof(b), f);

    xm_save_xm_samples(instr->samples, f, num_samples);
}

static void
xm_load_mod_note (XMNote *dest,
		  FILE *f)
{
    guint8 c[4];
    int note, period;

    fread(c, 1, 4, f);

    period = ((c[0] & 0x0f) << 8) | c[1];
    note = 0;
    
    if(period) {
	for(note = 0; note < 60; note++)
	    if(period >= npertab[note])
		break;
	note++;
	if(note==61)
	    note=0;
    }

    dest->note = note ? note + 24 : 0;
    dest->instrument = (c[0] & 0xf0) | (c[2] >> 4);
    dest->volume = 0;
    dest->fxtype = c[2] & 0x0f;
    dest->fxparam = c[3];
}

static int
xm_load_mod_pattern (XMPattern *pat,
		     int num_channels,
		     FILE *f)
{
    int i, j, len;

    len = 64;

    pat->length = pat->alloc_length = len;

    if(!st_init_pattern_channels(pat, len, num_channels))
	return 0;

    /* Read channel data */
    for(j = 0; j < len; j++) {
	for(i = 0; i < num_channels; i++) {
	    xm_load_mod_note(&pat->channels[i][j], f);
	}
    }

    return 1;
}

static XM *
xm_load_mod (FILE *f,
	     const char **error)
{
    XM *xm;
    guint8 sh[31][8];
    int i, n;
    guint8 mh[8];

    xm = calloc(1, sizeof(XM));
    if(!xm)
	goto fileerr;

    fread(xm->name, 1, 20, f);

    for(i = 0; i < 31; i++) {
	fread(xm->instruments[i].name, 1, 22, f);
	fread(sh[i], 1, 8, f);
    }

    fread(mh, 1, 2, f);
    xm->song_length = mh[0];

    fread(xm->pattern_order_table, 1, 128, f);
    fread(mh, 1, 4, f);
    
    if(memcmp("M.K.", mh, 4)) {
	*error = "No FastTracker XM and no ProTracker MOD!";
	goto ende;
    }

    for(i = 0, n = 0; i < 128; i++) {
	if(xm->pattern_order_table[i] > n)
	    n = xm->pattern_order_table[i];
    }

    xm->num_channels = 4;
    xm->tempo = 6;
    xm->bpm = 125;
    xm->flags = XM_FLAGS_IS_MOD | XM_FLAGS_AMIGA_FREQ;

    if(!xm_load_patterns(xm->patterns, n + 1, xm->num_channels, f, xm_load_mod_pattern)) {
	*error = "Error while loading patterns.";
	goto ende;
    }

    for(i = 0; i < 31; i++) {
	STSample *s = &xm->instruments[i].samples[0];

	s->length = get_be_16(sh[i] + 0) << 1;

	if(s->length > 0) {
	    s->finetune = (sh[i][2] & 0x0f) << 4;
	    s->volume = sh[i][3];
	    s->loopstart = get_be_16(sh[i] + 4) << 1;
	    s->loopend = s->loopstart + (get_be_16(sh[i] + 6) << 1);
	    s->type = 8;
	    s->panning = 128;
	    
	    if(get_be_16(sh[i] + 6) > 1)
		s->loop = LOOP_AMIGA;
	    if(s->loopend > s->length)
		s->loopend = s->length;
	    if(s->loopstart >= s->loopend) {
		fprintf(stderr, "Warning: Wrong loop start parameter. Don't know how to handle this.\n");
		s->loopstart = 0;
		s->loopend = 1;
		s->loop = 0;
	    }
	    
	    s->data = malloc(s->length);
	    fread(s->data, 1, s->length, f);
	}
    }

    return xm;

  ende:
    XM_Free(xm);
  fileerr:
    fclose(f);
    return NULL;
}

XM *
XM_Load (const char *filename,
	 const char **error)
{
    XM *xm;
    FILE *f;
    guint8 xh[80];
    int i, num_patterns, num_instruments;

    f = fopen(filename, "r");
    if(!f) {
	*error = "Can't open file";
	return NULL;
    }

    memset(xh, 0, sizeof(xh));

    if(fread(xh + 0, 1, sizeof(xh), f) != sizeof(xh)
       || strncmp(xh + 0, "Extended Module: ", 17) != 0
       || xh[37] != 0x1a) {
	*error = "No XM module!";
	fseek(f, 0, SEEK_SET);
	return xm_load_mod(f, error);
    }

    if(get_le_32(xh + 60) != 276) {
	fprintf(stderr, "Warning: XM header length != 276. Maybe a pre-0.0.12 SoundTracker module? :-)\n");
    }

    if(get_le_16(xh + 58) != 0x0104) {
	*error = "Version != 0x0104.";
	goto fileerr;
    }

    xm = calloc(1, sizeof(XM));
    if(!xm)
	goto fileerr;

    strncpy(xm->name, xh + 17, 20);
    recode_ibmpc_to_latin1(xm->name, 20);
    xm->song_length = get_le_16(xh + 64);
    xm->restart_position = get_le_16(xh + 66);
    xm->num_channels = get_le_16(xh + 68);
    num_patterns = get_le_16(xh + 70);
    num_instruments = get_le_16(xh + 72);
    if(get_le_16(xh + 74) != 1) {
	xm->flags |= XM_FLAGS_AMIGA_FREQ;
    }
    xm->tempo = get_le_16(xh + 76);
    xm->bpm = get_le_16(xh + 78);
    fread(xm->pattern_order_table, 1, 256, f);

    if(!xm_load_patterns(xm->patterns, num_patterns, xm->num_channels, f, xm_load_xm_pattern)) {
	*error = "Error while loading patterns.";
	goto ende;
    }
    
    for(i = 0; i < num_instruments; i++)
	xm_load_xm_instrument(&xm->instruments[i], f);

    return xm;
    
  ende:
    XM_Free(xm);
  fileerr:
    fclose(f);
    return NULL;
}

int XM_Save(XM* xm, const char *filename)
{
    FILE *f;
    int i;
    guint8 xh[80];
    int num_patterns, num_instruments;

    f = fopen(filename, "w");
    if(!f)
	return 0;

    num_patterns = st_num_save_patterns(xm);
    num_instruments = st_num_save_instruments(xm);

    memcpy(xh + 0, "Extended Module: ", 17);
    memcpy(xh + 17, xm->name, 20);
    recode_latin1_to_ibmpc(xh + 17, 20);
    xh[37] = 0x1a;
    memcpy(xh + 38, "rst's SoundTracker  ", 20);
    put_le_16(xh + 58, 0x104);
    put_le_32(xh + 60, 276);
    put_le_16(xh + 64, xm->song_length);
    put_le_16(xh + 66, xm->restart_position);
    put_le_16(xh + 68, xm->num_channels);
    put_le_16(xh + 70, num_patterns);
    put_le_16(xh + 72, num_instruments);
    put_le_16(xh + 74, xm->flags & XM_FLAGS_AMIGA_FREQ ? 0 : 1);
    put_le_16(xh + 76, xm->tempo);
    put_le_16(xh + 78, xm->bpm);

    fwrite(&xh, 1, sizeof(xh), f);
    fwrite(xm->pattern_order_table, 1, 256, f);

    for(i = 0; i < num_patterns; i++)
	xm_save_xm_pattern(&xm->patterns[i], xm->num_channels, f);

    for(i = 0; i < num_instruments; i++)
	xm_save_xm_instrument(&xm->instruments[i], f);
   
    if(ferror(f)) {
	fclose(f);
	return 0;
    }
	
    fclose(f);
    return 1;
}

XM *XM_New()
{
    XM *xm;

    xm = calloc(1, sizeof(XM));
    if(!xm) goto ende;

    xm->song_length = 1;
    xm->num_channels = 8;
    xm->tempo = 6;
    xm->bpm = 125;
    if(!xm_load_patterns(xm->patterns, 0, xm->num_channels, NULL, NULL))
	goto ende;

    return xm;

  ende:
    XM_Free(xm);
    return NULL;
}

void XM_Free(XM *xm)
{
    if(xm) {
	st_free_all_pattern_channels(xm);
	free(xm);
    }
}
