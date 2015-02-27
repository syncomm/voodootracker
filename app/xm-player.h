/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       	 		- XM Player -				 	 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/ 

#ifndef _ST_XMPLAYER_H
#define _ST_XMPLAYER_H

#include "driver.h"

extern int player_songpos, player_patpos;

void        xmplayer_init_module       (void);
gboolean    xmplayer_init_play_song    (int songpos, int patpos);
gboolean    xmplayer_init_play_pattern (int pattern, int patpos);
gboolean    xmplayer_play_note         (int channel, int note, int instrument);
double      xmplayer_play              (void);
void        xmplayer_stop              (void);
void        xmplayer_set_songpos       (int songpos);
void        xmplayer_set_pattern       (int pattern);

#endif /* _ST_XMPLAYER_H */
