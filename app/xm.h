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

#ifndef _XM_H
#define _XM_H

#include <glib.h>
#include <stdio.h>

/* -- Pattern definitions -- */

typedef struct XMNote {
    unsigned char note;
    unsigned char instrument;
    unsigned char volume;
    unsigned char fxtype;
    unsigned char fxparam;
} XMNote;

typedef struct XMPattern {
    int length, alloc_length;
    XMNote *channels[32];
} XMPattern;

/* -- Sample definitions -- */

#define LOOP_NONE       0
#define LOOP_AMIGA      1
#define LOOP_PINGPONG   2

typedef struct STSample {
    char name[23];
    int length;                            /* length of sample data */
    unsigned char type;                    /* 16 or 8 */

    int loopstart;                         /* loop start */
    int loopend;                           /* loop end (offset to first sample that should not be played) */
    unsigned char loop;                    /* loop flags */

    unsigned char volume;                  /* eigenvolume (0..64) */
    char finetune;                         /* finetune (-128 ... 127) */
    unsigned char panning;                 /* panning (0 ... 255) */
    char relnote;

    void *data;
} STSample;

/* -- Instrument definitions -- */

/* values for STEnvelope.flags */
#define EF_ON           1
#define EF_SUSTAIN      2
#define EF_LOOP         4

#define ST_MAX_ENVELOPE_POINTS 12

typedef struct STEnvelopePoint {
    short pos;
    short val;
} STEnvelopePoint;

typedef struct STEnvelope {
    STEnvelopePoint points[ST_MAX_ENVELOPE_POINTS];
    unsigned char num_points;
    unsigned char sustain_point;
    unsigned char loop_start;
    unsigned char loop_end;
    unsigned char flags;
} STEnvelope;

typedef struct STInstrument {
    char name[24];

    STEnvelope vol_env;
    STEnvelope pan_env;

    guint8 vibtype;
    guint16 vibrate;
    guint16 vibdepth;
    guint16 vibsweep;

    guint16 volfade;

    gint8 samplemap[96];
    STSample samples[16];
} STInstrument;

/* -- Main structure -- doesn't have much in common with the real layout of an XM file :) */

typedef struct XM {
    char name[21];

    int flags;                       /* see XM_FLAGS_ defines below */
    int num_channels;
    int tempo;
    int bpm;

    int song_length;
    int restart_position;
    guint8 pattern_order_table[256];

    XMPattern patterns[256];
    STInstrument instruments[128];
} XM;

#define XM_FLAGS_AMIGA_FREQ               1
#define XM_FLAGS_IS_MOD                   2

XM*           XM_Load                                  (const char *filename, const char **error);
int           XM_Save                                  (XM *xm, const char *filename);
XM*           XM_New                                   (void);
void          XM_Free                                  (XM*);

void xm_load_xm_samples (STSample samples[], int num_samples, FILE *f);
void xm_save_xm_samples (STSample samples[], FILE *f, int num_samples);

#endif /* _XM_H */
