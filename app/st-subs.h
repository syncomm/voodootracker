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

#ifndef _ST_SUBS_H
#define _ST_SUBS_H

#include "xm.h"

void          st_free_all_pattern_channels             (XM *xm);
int           st_init_pattern_channels                 (XMPattern *p, int length, int num_channels);
int           st_instrument_num_save_samples           (STInstrument *instr);
int           st_num_save_instruments                  (XM *xm);
int           st_num_save_patterns                     (XM *xm);

XMPattern*    st_dup_pattern                           (XMPattern*);
void          st_free_pattern_channels                 (XMPattern *pat);
void          st_clear_pattern                         (XMPattern*);

XMNote*       st_dup_track                             (XMNote*, int length);
void          st_clear_track                           (XMNote*, int length);

int           st_instrument_num_samples                (STInstrument *i);
void          st_clean_instrument                      (STInstrument *i, const char *name);
void          st_clean_sample                          (STSample *s, const char *name);
void          st_clean_song                            (XM*);

void          st_set_num_channels                      (XM *, int);
void          st_set_pattern_length                    (XMPattern *, int);

void          st_sample_fix_loop                       (STSample *s);

gboolean      st_instrument_used_in_song               (XM *xm, int instr);

#endif /* _ST_SUBS_H */
