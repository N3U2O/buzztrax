/* $Id$
 *
 * GtkVumeter
 * Copyright (C) 2003 Todd Goyen <wettoad@knighthoodofbuh.org>
 *               2006 Stefan Kost <ensonic@users.sf.net>
 *
 * This widget has been release as freeware.
 */

#ifndef __GTKVUMETER_H__
#define __GTKVUMETER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_VUMETER                (gtk_vumeter_get_type ())
#define GTK_VUMETER(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_VUMETER, GtkVUMeter))
#define GTK_VUMETER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_VUMETER GtkVUMeterClass))
#define GTK_IS_VUMETER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_VUMETER))
#define GTK_IS_VUMETER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VUMETER))
#define GTK_VUMETER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_VUMETER, GtkVUMeterClass))

typedef struct _GtkVUMeter      GtkVUMeter;
typedef struct _GtkVUMeterClass GtkVUMeterClass;

/**
 * GtkVUMeter:
 *
 * a volume meter widget
 */
struct _GtkVUMeter {
    GtkWidget   widget;

    GdkColormap *colormap;
    gint        colors;

    GdkGC       **f_gc;
    GdkGC       **b_gc;
    GdkColor    *f_colors;
    GdkColor    *b_colors;

    gboolean    vertical;
    gint        rms_level;
    gint        min;
    gint        max;

    gint        peaks_falloff;
    gint        delay_peak_level;
    gint        peak_level;

    gint        scale;
};

struct _GtkVUMeterClass {
    GtkWidgetClass  parent_class;
};

/* this is not yet implemented */
enum {
    GTK_VUMETER_PEAKS_FALLOFF_SLOW,
    GTK_VUMETER_PEAKS_FALLOFF_MEDIUM,
    GTK_VUMETER_PEAKS_FALLOFF_FAST,
    GTK_VUMETER_PEAKS_FALLOFF_LAST
};

enum {
    GTK_VUMETER_SCALE_LINEAR,
    GTK_VUMETER_SCALE_LOG,
    GTK_VUMETER_SCALE_LAST
};

GType    gtk_vumeter_get_type (void) G_GNUC_CONST;
GtkWidget *gtk_vumeter_new (gboolean vertical);
void gtk_vumeter_set_min_max (GtkVUMeter *vumeter, gint min, gint max);
void gtk_vumeter_set_levels (GtkVUMeter *vumeter, gint rms, gint peak);
void gtk_vumeter_set_peaks_falloff (GtkVUMeter *vumeter, gint peaks_falloff);
void gtk_vumeter_set_scale (GtkVUMeter *vumeter, gint scale);

G_END_DECLS

#endif /* __GTKVUMETER_H__ */
