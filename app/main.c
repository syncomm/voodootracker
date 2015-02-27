/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	      	   	  - Main Program -					 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#include <stdio.h>

#include <unistd.h>

#include "gui.h"
#include "xm.h"
#include "audio.h"

#include <gtk/gtk.h>

XM *xm = NULL;

int
main (int argc,
      char *argv[])
{
    int pipea[2], pipeb[2];
	audio_ctlpipe_id a;
	gpointer p;
	extern void driver_oss, mixer_lqmono;

    if(pipe(pipea) || pipe(pipeb)) {
	fprintf(stderr, "Cränk. Can't pipe().\n");
	return 1;
    }

    if(!audio_init(pipea[0], pipeb[1])) {
	fprintf(stderr, "Can't init audio thread.\n");
	return 1;
    }

    audio_ctlpipe = pipea[1];
    audio_backpipe = pipeb[0];

    a = AUDIO_CTLPIPE_SET_DRIVER;
    p = &driver_oss;
    write(audio_ctlpipe, &a, sizeof(a));
    write(audio_ctlpipe, &p, sizeof(p));
    a = AUDIO_CTLPIPE_SET_MIXER;
    p = &mixer_lqmono;
    write(audio_ctlpipe, &a, sizeof(a));
    write(audio_ctlpipe, &p, sizeof(p));

    if(gui_init(argc, argv)) {
	gtk_main();
	return 0;
    } else {
	fprintf(stderr, "gui_init() failed.\n");
	return 1;
    }
}
