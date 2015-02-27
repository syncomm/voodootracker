/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       		  - Sample Editor -					 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _SAMPLE_EDITOR_H
#define _SAMPLE_EDITOR_H

#include <gtk/gtk.h>
#include "xm.h"

void         sample_editor_page_create               (GtkNotebook *nb);
GtkWidget*   sampler_sampling_page_create            (void);

void         sampler_page_handle_keys                (int shift,
						      int ctrl,
						      int alt,
						      guint32 keyval);

void         sample_editor_set_sample                (STSample *);
void         sample_editor_update_mixer_position     (void);
void         sample_editor_update                    (void);

void         sample_editor_sampling_mode_activated   (void);
void         sample_editor_sampling_mode_deactivated (void);
void         sample_editor_sampling_tick             (int count);

#endif /* _SAMPLE_EDITOR_H */
