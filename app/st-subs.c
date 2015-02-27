/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       - General Support Routines -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#include <stdlib.h>
#include <string.h>

#include "st-subs.h"
#include "xm.h"

int
st_init_pattern_channels (XMPattern *p,
			  int length,
			  int num_channels)
{
    int i;
    
    p->length = p->alloc_length = length;
    for(i = 0; i < num_channels; i++) {
	if(!(p->channels[i] = calloc(1, length * sizeof(XMNote)))) {
	    st_free_pattern_channels(p);
	    return 0;
	}
    }
	    
    return 1;
}

void
st_free_pattern_channels (XMPattern *pat)
{
    int i;
    
    for(i = 0; i < 32; i++) {
	free(pat->channels[i]);
	pat->channels[i] = NULL;
    }
}

void
st_free_all_pattern_channels (XM *xm)
{
    int i;
    
    for(i = 0; i < 256; i++)
	st_free_pattern_channels(&xm->patterns[i]);
}

XMPattern *
st_dup_pattern (XMPattern *p)
{
    XMPattern *r;
    int i;

    r = malloc(sizeof(XMPattern));
    r->length = p->length;

    for(i = 0; i < 32; i++)
	r->channels[i] = st_dup_track(p->channels[i], p->length);

    return r;
}

XMNote *
st_dup_track (XMNote *n,
	      int length)
{
    XMNote *r;

    if(!n)
	return NULL;

    r = malloc(length * sizeof(XMNote));
    memcpy(r, n, length * sizeof(XMNote));
    return r;
}

void
st_clear_track (XMNote *n,
		int length)
{
    if(!n)
	return;

    while(length--) {
	memset(&n[length], 0, sizeof(n[length]));
    }
}

void
st_clear_pattern (XMPattern *p)
{
    int i;

    for(i = 0; i < 32; i++) {
	st_clear_track(p->channels[i], p->length);
    }
}

gboolean
st_instrument_used_in_song (XM *xm,
			    int instr)
{
    int i, j, k;
    XMPattern *p;
    XMNote *c;

    for(i = 0; i < xm->song_length; i++) {
	p = &xm->patterns[(int)xm->pattern_order_table[i]];
	for(j = 0; j < xm->num_channels; j++) {
	    c = p->channels[j];
	    for(k = 0; k < p->length; k++) {
		if(c[k].instrument == instr)
		    return TRUE;
	    }
	}
    }

    return FALSE;
}

int
st_instrument_num_samples (STInstrument *instr)
{
    int i, n;

    for(i = 0, n = 0; i < sizeof(instr->samples) / sizeof(instr->samples[0]); i++) {
	if(instr->samples[i].length != 0)
	    n++;
    }
    return n;
}

int
st_instrument_num_save_samples (STInstrument *instr)
{
    int i, n;

    for(i = 0, n = 0; i < sizeof(instr->samples) / sizeof(instr->samples[0]); i++) {
	if(instr->samples[i].length != 0 || instr->samples[i].name[0] != 0)
	    n = i + 1;
    }
    return n;
}

int
st_num_save_instruments (XM *xm)
{
    int i, n;

    for(i = 0, n = 0; i < 128; i++) {
	if(st_instrument_num_save_samples(&xm->instruments[i]) != 0 || xm->instruments[i].name[0] != 0)
	    n = i + 1;
    }
    return n;
}

int
st_num_save_patterns (XM *xm)
{
    int i, n;

    for(i = 0, n = 0; i < 256; i++) {
	if(xm->pattern_order_table[i] > n)
	    n = xm->pattern_order_table[i];
    }
    return n + 1;
}

void
st_clean_instrument (STInstrument *instr,
		     const char *name)
{
    int i;

    for(i = 0; i < sizeof(instr->samples) / sizeof(instr->samples[0]); i++)
	st_clean_sample(&instr->samples[i], NULL);

    if(!name)
	memset(instr->name, 0, sizeof(instr->name));
    else
	strncpy(instr->name, name, 22);
    memset(instr->samplemap, 0, sizeof(instr->samplemap));
    memset(&instr->vol_env, 0, sizeof(instr->vol_env));
    memset(&instr->pan_env, 0, sizeof(instr->pan_env));
    instr->vol_env.num_points = 1;
    instr->vol_env.points[0].val = 64;
    instr->pan_env.num_points = 1;
    instr->pan_env.points[0].val = 128;
}

void
st_clean_sample (STSample *s,
		 const char *name)
{
    free(s->data);
    memset(s, 0, sizeof(STSample));
    if(name)
	strncpy(s->name, name, 22);
    s->loopend = 1;
}

void
st_clean_song (XM *xm)
{
    int i;

    st_free_all_pattern_channels(xm);

    xm->song_length = 1;
    xm->restart_position = 0;
    xm->num_channels = 8;
    xm->tempo = 6;
    xm->bpm = 125;
    memset(xm->pattern_order_table, 0, sizeof(xm->pattern_order_table));

    for(i = 0; i < 256; i++)
	st_init_pattern_channels(&xm->patterns[i], 64, xm->num_channels);
}

void
st_set_num_channels (XM *xm,
		     int n)
{
    int i, j;
    XMPattern *pat;

    for(i = 0; i < sizeof(xm->patterns) / sizeof(xm->patterns[0]); i++) {
	pat = &xm->patterns[i];
	for(j = 0; j < n; j++) {
	    if(!pat->channels[j]) {
		pat->channels[j] = calloc(1, sizeof(XMNote) * pat->alloc_length);
	    }
	}
    }

    xm->num_channels = n;
}

void
st_set_pattern_length (XMPattern *pat,
		       int l)
{
    int i;
    XMNote *n;

    if(l > pat->alloc_length) {
	for(i = 0; i < 32 && pat->channels[i] != NULL; i++) {
	    n = calloc(1, sizeof(XMNote) * l);
	    memcpy(n, pat->channels[i], sizeof(XMNote) * pat->length);
	    free(pat->channels[i]);
	    pat->channels[i] = n;
	}
	pat->alloc_length = l;
    }

    pat->length = l;
}

void
st_sample_fix_loop (STSample *s)
{
    if(s->loopend > s->length)
	s->loopend = s->length;
    if(s->loopstart >= s->loopend)
	s->loopstart = s->loopend - 1;
}
