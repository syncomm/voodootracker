/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	       - GTK+ Sample Display widget -			 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _SAMPLE_DISPLAY_H
#define _SAMPLE_DISPLAY_H

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#define SAMPLE_DISPLAY(obj)          GTK_CHECK_CAST (obj, sample_display_get_type (), SampleDisplay)
#define SAMPLE_DISPLAY_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, sample_display_get_type (), SampleDisplayClass)
#define IS_SAMPLE_DISPLAY(obj)       GTK_CHECK_TYPE (obj, sample_display_get_type ())

typedef struct _SampleDisplay       SampleDisplay;
typedef struct _SampleDisplayClass  SampleDisplayClass;

enum {
    SAMPLE_DISPLAYCOL_BG,
    SAMPLE_DISPLAYCOL_FG,
    SAMPLE_DISPLAYCOL_LOOP,
    SAMPLE_DISPLAYCOL_MIXERPOS,
    SAMPLE_DISPLAYCOL_LAST
};

struct _SampleDisplay
{
    GtkWidget widget;
    int edit;                                     /* enable loop / selection editing */

    int width, height;
    GdkGC *bg_gc, *fg_gc, *loop_gc, *mixerpos_gc;

    void *data;
    int datalen;
    int datatype;
    gboolean datacopy;                            /* do we have our own copy */
    int datacopylen;

    int win_start, win_length;
    int complete_redraw;

    int mixerpos, old_mixerpos;                   /* current playing offset of the sample */

    /* selection handling */
    int sel_start, sel_end;                       /* offsets into the sample data or -1 */
    int old_ss, old_se;
    int button;                                   /* button index which started the selection */
    int selecting;

    /* loop points */
    int loop_start, loop_end;                     /* offsets into the sample data or -1 */
};

struct _SampleDisplayClass
{
    GtkWidgetClass parent_class;

    GdkColor colors[SAMPLE_DISPLAYCOL_LAST];

    void (*selection_changed)(SampleDisplay *s, int start, int end);
    void (*loop_changed)(SampleDisplay *s, int start, int end);
};

guint          sample_display_get_type            (void);
GtkWidget*     sample_display_new                 (gboolean edit);

void           sample_display_set_data_16         (SampleDisplay *s, gint16 *data, int len, gboolean copy);
void           sample_display_set_data_8          (SampleDisplay *s, gint8 *data, int len, gboolean copy);
void           sample_display_set_loop            (SampleDisplay *s, int start, int end);
void           sample_display_set_selection       (SampleDisplay *s, int start, int end);
void           sample_display_set_mixer_position  (SampleDisplay *s, int offset);

void           sample_display_set_window          (SampleDisplay *s, int start, int end);

#endif /* _SAMPLE_DISPLAY_H */
