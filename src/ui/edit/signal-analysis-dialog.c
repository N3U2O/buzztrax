/* $Id$
 *
 * Buzztard
 * Copyright (C) 2006 Buzztard team <buzztard-devel@lists.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/**
 * SECTION:btsignalanalysisdialog
 * @short_description: audio analysis window
 *
 * The dialog shows a spectrum analyzer and level-meters for left/right
 * channel. The spectrum analyzer support mono and stereo display. It has a few
 * settings for logarithmic/linear mapping and precission.
 *
 * Right now the analyser-section can be attached to a BtWire.
 *
 * The dialog is not modal.
 */
/* @idea: nice monitoring ideas:
 * http://www.music-software-reviews.com/adobe_audition_2.html
 *
 * @todo: shall we add a volume and panorama control to the dialog as well?
 * - volume to the right of the spectrum
 * - panorama below the spectrum
 *
 * @idea: use own ruler widget or patch up the existing one
 * http://live.gnome.org/GTK%2B/GtkRuler
 *
 * @idea: it would be nice to use this on the sink-machine as well.
 * - need to change it to "analysis-dialog"
 * - the constructor needs variants for machines and wires or we just pass it a
 *   and require that the bin has a "ananlyzers" property
 * - machines need a pre/post hook (idealy we just need it for the sink)
 *   - we have (pre-gain)-level there already
 *   - we have no tee in machines (except the spreader, which we can't misuse
 *     here)
 *   - we could always plug a tee in sink-bin and add the analyzers there
 */
#define BT_EDIT
#define BT_SIGNAL_ANALYSIS_DIALOG_C

#include "bt-edit.h"

//-- property ids

enum {
  SIGNAL_ANALYSIS_DIALOG_ELEMENT=1
};

/* @todo: add more later:
 * waveform (oszilloscope)
 * panorama (spacescope)
 */
typedef enum {
  // needed to buffer
  ANALYZER_QUEUE=0,
  // real analyzers
  ANALYZER_LEVEL,
  ANALYZER_SPECTRUM,
  // this can be 'mis'used for an aoszilloscope by connecting to hand-off
  ANALYZER_FAKESINK,
  // how many elements are used
  ANALYZER_COUNT
} BtWireAnalyzer;

typedef enum {
  MAP_LIN=0,
  MAP_LOG=1
} BtWireAnalyzerMapping;

#define LEVEL_HEIGHT 16
#define LOW_VUMETER_VAL -90.0

#define SPECTRUM_FLOOR -70

#define UPDATE_INTERVAL ((GstClockTime)(0.1*GST_SECOND))

struct _BtSignalAnalysisDialogPrivate {
  /* used to validate if dispose has run */
  gboolean dispose_has_run;

  /* the application */
  BtEditApplication *app;

  /* the item to attach the analyzer to */
  GstBin *element;

  /* the analyzer-graphs */
  GtkWidget *spectrum_drawingarea, *level_drawingarea;
  GtkWidget *spectrum_ruler;
  GdkColor *peak_color,*grid_color;
  cairo_pattern_t *spect_grad;

  /* the gstreamer elements that is used */
  GstElement *analyzers[ANALYZER_COUNT];
  GList *analyzers_list;

  /* the analyzer results (max stereo) */
  gdouble rms[2], peak[2];
  gfloat *spect[2];
  GMutex *lock;

  guint spect_channels;
  guint spect_height;
  guint spect_bands;
  gfloat height_scale;
  /* sampling rate from spectrum.sink caps */
  gint srate;
  BtWireAnalyzerMapping frq_map;
  guint frq_precision;

  GstClock *clock;

  /* up to srat=900000 */
  gdouble grid_log10[6*10];
  gdouble *graph_log10;

  // DEBUG
  //gdouble min_rms,max_rms, min_peak,max_peak;
  // DEBUG
};

static GQuark bus_msg_level_quark=0;
static GQuark bus_msg_spectrum_quark=0;

//-- the class

G_DEFINE_TYPE (BtSignalAnalysisDialog, bt_signal_analysis_dialog, GTK_TYPE_WINDOW);


//-- event handler helper

/*
 * on_signal_analyser_redraw:
 * @user_data: conatins the self pointer
 *
 * Called as idle function to repaint the analyzers. Data is gathered by
 * on_signal_analyser_change()
 */
static gboolean redraw_level(gpointer user_data) {
  BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(user_data);

  if(!gtk_widget_get_realized(GTK_WIDGET(self))) return(TRUE);

  //GST_DEBUG("redraw analyzers");
  // draw levels
  if (self->priv->level_drawingarea) {
    GdkRectangle rect = { 0, 0, self->priv->spect_bands, LEVEL_HEIGHT };
    GtkWidget *da=self->priv->level_drawingarea;
    GdkWindow *window = gtk_widget_get_window(da);
    gint middle=self->priv->spect_bands>>1;
    gdouble scl=middle/(-LOW_VUMETER_VAL);
    gdouble rms0,rms1,peak0,peak1;
    cairo_t *cr;

    gdk_window_begin_paint_rect(window, &rect);
    cr=gdk_cairo_create(window);

    cairo_set_source_rgb(cr,0.0,0.0,0.0);
    cairo_rectangle(cr,0,0,self->priv->spect_bands, LEVEL_HEIGHT);
    cairo_fill(cr);
    /* DEBUG
      if((self->priv->rms[0]<self->priv->min_rms) && !isinf(self->priv->rms[0])) {
        GST_DEBUG("levels: rms=%7.4lf",self->priv->rms[0]);
        self->priv->min_rms=self->priv->rms[0];
      }
      if((self->priv->rms[0]>self->priv->max_rms) && !isinf(self->priv->rms[0])) {
        GST_DEBUG("levels: rms=%7.4lf",self->priv->rms[0]);
        self->priv->max_rms=self->priv->rms[0];
      }
      if((self->priv->peak[0]<self->priv->min_peak) && !isinf(self->priv->peak[0])) {
        GST_DEBUG("levels: peak=%7.4lf",self->priv->peak[0]);
        self->priv->min_peak=self->priv->peak[0];
      }
      if((self->priv->peak[0]>self->priv->max_peak) && !isinf(self->priv->peak[0])) {
        GST_DEBUG("levels: peak=%7.4lf",self->priv->peak[0]);
        self->priv->max_peak=self->priv->peak[0];
      }
    // DEBUG */

    // use RMS or peak or both?
    rms0=self->priv->rms[0]*scl;
    rms1=self->priv->rms[1]*scl;
    peak0=self->priv->peak[0]*scl;
    peak1=self->priv->peak[1]*scl;
    cairo_set_source_rgb(cr,1.0,1.0,1.0);
    cairo_rectangle(cr, middle    -rms0, 0, rms0+rms1, LEVEL_HEIGHT);
    cairo_fill(cr);
    cairo_set_source_rgb(cr,self->priv->peak_color->red/65535.0,self->priv->peak_color->green/65535.0,self->priv->peak_color->blue/65535.0);
    cairo_rectangle(cr, middle    -peak0, 0, 2, LEVEL_HEIGHT);
    cairo_fill(cr);
    cairo_rectangle(cr, (middle-1)+peak1, 0, 2, LEVEL_HEIGHT);
    cairo_fill(cr);

    /* @todo: if stereo draw pan-meter (L-R, R-L) */

    cairo_destroy(cr);
    gdk_window_end_paint(window);
  }
  return(TRUE);
}

static gboolean redraw_spectrum(gpointer user_data) {
  BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(user_data);

  if(!gtk_widget_get_realized(GTK_WIDGET(self))) return(TRUE);

  // draw spectrum
  if(self->priv->spectrum_drawingarea) {
    guint i,c;
    gdouble x,y,v,f;
    gdouble inc,end,srat2;
    guint spect_bands=self->priv->spect_bands;
    guint spect_height=self->priv->spect_height;
    GdkRectangle rect = { 0, 0, spect_bands, self->priv->spect_height };
    GtkWidget *da=self->priv->spectrum_drawingarea;
    GdkWindow *window = gtk_widget_get_window(da);
    gdouble *grid_log10 = self->priv->grid_log10;
    gdouble grid_dash_pattern[]={1.0};
    cairo_t *cr;

    gdk_window_begin_paint_rect(window,&rect);
    cr=gdk_cairo_create(window);

    cairo_set_source_rgb(cr,0.0,0.0,0.0);
    cairo_rectangle(cr,0,0,spect_bands,spect_height);
    cairo_fill(cr);
    /* draw grid lines
     * the bin center frequencies are f(i)=i*srat/spect_bands
     */
    cairo_set_source_rgb(cr,self->priv->grid_color->red/65535.0,self->priv->grid_color->green/65535.0,self->priv->grid_color->blue/65535.0);
    cairo_set_line_width(cr, 1.0);
    cairo_set_dash(cr,grid_dash_pattern,1,0.0);
    y=0.5+spect_height/2.0;
    cairo_move_to(cr,0,y);cairo_line_to(cr,spect_bands,y);
    if(self->priv->frq_map==MAP_LIN) {
      for(f=0.05;f<1.0;f+=0.05) {
        x=0.5+spect_bands*f;
        cairo_move_to(cr,x,0);cairo_line_to(cr,x,spect_height);
      }
    }
    else {
      srat2=self->priv->srate/2.0;
      v=spect_bands/log10(srat2);
      i=0;f=1.0;inc=1.0;end=10.0;
      while(f<srat2) {
        x=0.5+v*grid_log10[i++];
        cairo_move_to(cr,x,0);cairo_line_to(cr,x,spect_height);
        f+=inc;
        if(f>=end) {
          f=inc=end;
          end*=10.0;
        }
      }
    }
    cairo_stroke(cr);
    // draw frequencies
    g_mutex_lock(self->priv->lock);
    for(c=0;c<self->priv->spect_channels;c++) {
      if(self->priv->spect[c]) {
        gfloat *spect=self->priv->spect[c];
        gdouble *graph_log10=self->priv->graph_log10;
        gdouble prec=self->priv->frq_precision;

        // set different color for different channels
        // maybe we also want a different gradient
        if(self->priv->spect_channels==1) {
          cairo_set_source_rgb(cr,1.0,1.0,1.0);
        } else {
          if(c==0) {
            cairo_set_source_rgba(cr,1.0,0.0,0.7,0.7);
          } else {
            cairo_set_source_rgba(cr,0.6,0.6,1.0,0.7);
          }
        }
        cairo_set_line_width(cr, 2.0);
        cairo_set_dash(cr,NULL,0,0.0);
        cairo_move_to(cr,0,spect_height);
        if(self->priv->frq_map==MAP_LIN) {
          for(i=0;i<(spect_bands*prec);i++) {
            cairo_line_to(cr,(i/prec),spect_height-spect[i]);
          }
        }
        else {
          srat2=self->priv->srate/2.0;
          v=spect_bands/log10(srat2);
          for(i=0;i<(spect_bands*prec);i++) {
            // db value
            //y=-20.0*log10(-spect[i]);
            x=0.5+v*graph_log10[i];
            cairo_line_to(cr,x,spect_height-spect[i]);
          }
        }
        cairo_line_to(cr,spect_bands-1,spect_height);
        cairo_line_to(cr,0,spect_height);
        cairo_stroke_preserve(cr);
        // in case the gradient is too slow:
        //cairo_set_source_rgb(cr,0.7,0.7,0.7);
        cairo_set_source(cr,self->priv->spect_grad);
        cairo_fill(cr);
      }
    }
    g_mutex_unlock(self->priv->lock);

    cairo_destroy(cr);
    gdk_window_end_paint(window);
  }
  return(TRUE);
}

static void update_spectrum_ruler(const BtSignalAnalysisDialog *self) {
  GtkRuler *ruler=GTK_RULER(self->priv->spectrum_ruler);

  gtk_ruler_set_range(ruler,0.0,self->priv->srate/20.0,-10.0,200.0);

#if 0
  // gtk_ruler_set_metric needs an enum value :/
  // we can't register own types and there is no setter for custom metrics either
  if(self->priv->frq_map==MAP_LIN) {
    //gtk_ruler_set_metric(ruler,&ruler_metrics[0]);
  }
  else {
    static const GtkRulerMetric ruler_metrics[] =
    {
      {"Frequency", "Hz", 1.0, { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 }, { 1, 2, 4, 8, 16 }},
    };
    //gtk_ruler_set_metric(ruler,&ruler_metrics[0]);
  }
#endif
}

static void update_spectrum_graph_log10(BtSignalAnalysisDialog *self) {
  guint i, spect_bands=self->priv->spect_bands*self->priv->frq_precision;
  gdouble *graph_log10=self->priv->graph_log10;
  gdouble srat2=self->priv->srate/2.0;

  for(i=0;i<spect_bands;i++) {
    graph_log10[i]=log10(1.0+(i*srat2)/spect_bands);
  }
}

static void update_spectrum_analyzer(BtSignalAnalysisDialog *self) {
  guint spect_bands;

  g_mutex_lock(self->priv->lock);

  spect_bands=self->priv->spect_bands*self->priv->frq_precision;

  g_free(self->priv->spect[0]);
  self->priv->spect[0]=g_new0(gfloat, spect_bands);
  g_free(self->priv->spect[1]);
  self->priv->spect[1]=g_new0(gfloat, spect_bands);

  g_free(self->priv->graph_log10);
  self->priv->graph_log10=g_new(gdouble, spect_bands);
  update_spectrum_graph_log10(self);

  if (self->priv->analyzers[ANALYZER_SPECTRUM]) {
    g_object_set((GObject*)(self->priv->analyzers[ANALYZER_SPECTRUM]),
      "bands", spect_bands, NULL);
  }

  g_mutex_unlock(self->priv->lock);
}

//-- event handler

static void bt_signal_analysis_dialog_realize(GtkWidget *widget,gpointer user_data) {
  BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(user_data);

  GST_DEBUG("dialog realize");
  self->priv->peak_color=bt_ui_resources_get_gdk_color(BT_UI_RES_COLOR_ANALYZER_PEAK);
  self->priv->grid_color=bt_ui_resources_get_gdk_color(BT_UI_RES_COLOR_GRID_LINES);
}

static gboolean bt_signal_analysis_dialog_level_expose(GtkWidget *widget,GdkEventExpose *event,gpointer user_data) {
  BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(user_data);

  redraw_level((gpointer)self);
  return(TRUE);
}

static gboolean bt_signal_analysis_dialog_spectrum_expose(GtkWidget *widget,GdkEventExpose *event,gpointer user_data) {
  BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(user_data);

  redraw_spectrum((gpointer)self);
  return(TRUE);
}

static gboolean on_delayed_idle_signal_analyser_change(gpointer user_data) {
  gconstpointer * const params=(gconstpointer *)user_data;
  BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(params[0]);
  GstMessage *message=(GstMessage *)params[1];
  const GstStructure *structure=gst_message_get_structure(message);
  const GQuark name_id=gst_structure_get_name_id(structure);

  if(!self)
    goto done;

  g_object_remove_weak_pointer((gpointer)self,(gpointer *)&params[0]);

  if(name_id==bus_msg_level_quark) {
    const GValue *l_cur,*l_peak;
    guint i;
    gdouble val;

    //GST_INFO("get level data");
    //l_cur=(GValue *)gst_structure_get_value(structure, "rms");
    l_cur=(GValue *)gst_structure_get_value(structure, "peak");
    //l_peak=(GValue *)gst_structure_get_value(structure, "peak");
    l_peak=(GValue *)gst_structure_get_value(structure, "decay");
    // size of list is number of channels
    switch(gst_value_list_get_size(l_cur)) {
      case 1: // mono
          val=g_value_get_double(gst_value_list_get_value(l_cur,0));
          if(isinf(val) || isnan(val)) val=LOW_VUMETER_VAL;
          self->priv->rms[0]=-(LOW_VUMETER_VAL-val);
          self->priv->rms[1]=self->priv->rms[0];
          val=g_value_get_double(gst_value_list_get_value(l_peak,0));
          if(isinf(val) || isnan(val)) val=LOW_VUMETER_VAL;
          self->priv->peak[0]=-(LOW_VUMETER_VAL-val);
          self->priv->peak[1]=self->priv->peak[0];
        break;
      case 2: // stereo
        for(i=0;i<2;i++) {
          val=g_value_get_double(gst_value_list_get_value(l_cur,i));
          if(isinf(val) || isnan(val)) val=LOW_VUMETER_VAL;
          self->priv->rms[i]=-(LOW_VUMETER_VAL-val);
          val=g_value_get_double(gst_value_list_get_value(l_peak,i));
          if(isinf(val) || isnan(val)) val=LOW_VUMETER_VAL;
          self->priv->peak[i]=-(LOW_VUMETER_VAL-val);
        }
        break;
    }
    gtk_widget_queue_draw(self->priv->level_drawingarea);
  }
  else if(name_id==bus_msg_spectrum_quark) {
    const GValue *data;
    const GValue *value;
    guint i, size, spect_bands, spect_height=self->priv->spect_height;
    gfloat height_scale=self->priv->height_scale;
    gfloat *spect;

    g_mutex_lock(self->priv->lock);
    spect_bands=self->priv->spect_bands*self->priv->frq_precision;
    //GST_INFO("get spectrum data");
    if((data = gst_structure_get_value (structure, "magnitude"))) {
      if(GST_VALUE_HOLDS_LIST(data)) {
        size=gst_value_list_get_size(data);
        if(size==spect_bands) {
          spect=self->priv->spect[0];
          for(i=0;i<spect_bands;i++) {
            value=gst_value_list_get_value(data,i);
            spect[i]=spect_height-height_scale*g_value_get_float(value);
          }
          gtk_widget_queue_draw(self->priv->spectrum_drawingarea);
        }
      } else if(GST_VALUE_HOLDS_ARRAY(data)) {
        const GValue *cdata;
        guint c=gst_value_array_get_size(data);

        self->priv->spect_channels=MIN(2,c);
        for (c=0;c<self->priv->spect_channels;c++) {
          cdata=gst_value_array_get_value(data,c);
          size=gst_value_array_get_size(cdata);
          if(size!=spect_bands)
            break;
          spect=self->priv->spect[c];
          for(i=0;i<spect_bands;i++) {
            value=gst_value_array_get_value(cdata,i);
            spect[i]=spect_height-height_scale*g_value_get_float(value);
          }
        }
        if (c==self->priv->spect_channels) {
          gtk_widget_queue_draw(self->priv->spectrum_drawingarea);
        }
      }
    }
    else if((data = gst_structure_get_value (structure, "spectrum"))) {
      size=gst_value_list_get_size(data);
      if(size==spect_bands) {
        spect=self->priv->spect[0];
        for(i=0;i<spect_bands;i++) {
          value=gst_value_list_get_value(data,i);
          spect[i]=spect_height-height_scale*(gfloat)g_value_get_uchar(value);
        }
        gtk_widget_queue_draw(self->priv->spectrum_drawingarea);
      }
    }
    g_mutex_unlock(self->priv->lock);
  }

done:
  gst_message_unref(message);
  g_slice_free1(2*sizeof(gconstpointer),params);
  return(FALSE);
}

static gboolean on_delayed_signal_analyser_change(GstClock *clock,GstClockTime time,GstClockID id,gpointer user_data) {
  // the callback is called from a clock thread
  if(GST_CLOCK_TIME_IS_VALID(time))
    g_idle_add(on_delayed_idle_signal_analyser_change,user_data);
  else {
    gconstpointer * const params=(gconstpointer *)user_data;
    GstMessage *message=(GstMessage *)params[1];
    gst_message_unref(message);
    g_slice_free1(2*sizeof(gconstpointer),user_data);
  }
  return(TRUE);
}

static void on_signal_analyser_change(GstBus * bus, GstMessage * message, gpointer user_data) {
  const GstStructure *structure=gst_message_get_structure(message);
  const GQuark name_id=gst_structure_get_name_id(structure);

  if((name_id==bus_msg_level_quark) || (name_id==bus_msg_spectrum_quark)) {
    BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(user_data);
    GstElement *meter=GST_ELEMENT(GST_MESSAGE_SRC(message));

    if((meter==self->priv->analyzers[ANALYZER_LEVEL]) ||
      (meter==self->priv->analyzers[ANALYZER_SPECTRUM])) {
      GstClockTime timestamp, duration;
      GstClockTime waittime=GST_CLOCK_TIME_NONE;

      if(gst_structure_get_clock_time (structure, "running-time", &timestamp) &&
        gst_structure_get_clock_time (structure, "duration", &duration)) {
        /* wait for middle of buffer */
        waittime=timestamp+duration/2;
      }
      else if(gst_structure_get_clock_time (structure, "endtime", &timestamp)) {
        if(name_id==bus_msg_level_quark) {
          /* level sends endtime as stream_time and not as running_time */
          waittime=gst_segment_to_running_time(&GST_BASE_TRANSFORM(meter)->segment, GST_FORMAT_TIME, timestamp);
        }
        else {
          waittime=timestamp;
        }
      }
      if(GST_CLOCK_TIME_IS_VALID(waittime)) {
        gconstpointer *params=(gconstpointer *)g_slice_alloc(2*sizeof(gconstpointer));
        GstClockID clock_id;
        GstClockTime basetime=gst_element_get_base_time(meter);

        //GST_WARNING("received %s update for waittime %"GST_TIME_FORMAT,
        //  g_quark_to_string(name_id),GST_TIME_ARGS(waittime));

        params[0]=(gpointer)self;
        params[1]=(gpointer)gst_message_ref(message);
        g_object_add_weak_pointer((gpointer)self,(gpointer *)&params[0]);

        clock_id=gst_clock_new_single_shot_id(self->priv->clock,waittime+basetime);
        if(gst_clock_id_wait_async(clock_id,on_delayed_signal_analyser_change,(gpointer)params)!=GST_CLOCK_OK) {
          gst_message_unref(message);
          g_slice_free1(2*sizeof(gconstpointer),params);
        }
        gst_clock_id_unref(clock_id);
      }
    }
  }
}

static void on_size_allocate(GtkWidget *widget,GtkAllocation *allocation,gpointer user_data) {
  BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(user_data);
  guint old_heigth=self->priv->spect_height;
  guint old_bands=self->priv->spect_bands;

  /*GST_INFO ("%d x %d", allocation->width, allocation->height); */
  self->priv->spect_height = allocation->height;
  self->priv->height_scale = (gdouble)allocation->height / (gdouble)SPECTRUM_FLOOR;
  self->priv->spect_bands = allocation->width;

  if (old_heigth!=self->priv->spect_height || !self->priv->spect_grad) {
    if(self->priv->spect_grad)
      cairo_pattern_destroy(self->priv->spect_grad);
    // this looks nice, but seems to be expensive
    self->priv->spect_grad=cairo_pattern_create_linear(0.0, self->priv->spect_height, 0.0, 0.0);
    cairo_pattern_add_color_stop_rgba(self->priv->spect_grad, 0.00,  0.8, 0.8, 0.8, 0.7);
    cairo_pattern_add_color_stop_rgba(self->priv->spect_grad, 1.00,  0.8, 0.8, 0.8, 0.0);
  }
  if (old_bands!=self->priv->spect_bands) {
    update_spectrum_analyzer(self);
  }
  gtk_widget_queue_draw(self->priv->spectrum_drawingarea);
}

static void on_caps_negotiated(GstPad *pad,GParamSpec *arg,gpointer user_data) {
  BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(user_data);
  GstCaps *caps;

  if((caps=(GstCaps *)gst_pad_get_negotiated_caps(pad))) {
    if(GST_CAPS_IS_SIMPLE(caps)) {
      GstStructure *structure;
      if((structure=gst_caps_get_structure(caps,0))) {
        gint old_srate=self->priv->srate;
        gst_structure_get_int(structure,"rate",&self->priv->srate);
        if(self->priv->srate!=old_srate) {
          update_spectrum_ruler(self);
          update_spectrum_graph_log10(self);
        }
      }
    }
    gst_caps_unref(caps);
  }
}

static void on_spectrum_frequency_mapping_changed(GtkComboBox *combo, gpointer user_data) {
  BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(user_data);

  self->priv->frq_map=gtk_combo_box_get_active(combo);
  update_spectrum_ruler(self);
  gtk_widget_queue_draw(self->priv->spectrum_drawingarea);
}

static void on_spectrum_frequency_precision_changed(GtkComboBox *combo, gpointer user_data) {
  BtSignalAnalysisDialog *self=BT_SIGNAL_ANALYSIS_DIALOG(user_data);

  self->priv->frq_precision=1+gtk_combo_box_get_active(combo);
  update_spectrum_analyzer(self);
}

//-- helper methods

/*
 * bt_signal_analysis_dialog_make_element:
 * @self: the signal-analysis dialog
 * @part: which analyzer element to create
 * @factory_name: the element-factories name (also used to build the elemnt name)
 *
 * Create analyzer elements. Store them in the analyzers array and link them into the list.
 */
static gboolean bt_signal_analysis_dialog_make_element(const BtSignalAnalysisDialog *self,BtWireAnalyzer part, const gchar *factory_name) {
  gboolean res=FALSE;
  gchar *name;

  // add analyzer element
  name=g_alloca(strlen("analyzer_")+strlen(factory_name)+16);g_sprintf(name,"analyzer_%s_%p",factory_name,self->priv->element);
  if(!(self->priv->analyzers[part]=gst_element_factory_make(factory_name,name))) {
    GST_ERROR("failed to create %s",factory_name);goto Error;
  }
  self->priv->analyzers_list=g_list_prepend(self->priv->analyzers_list,self->priv->analyzers[part]);
  res=TRUE;
Error:
  return(res);
}

static gboolean bt_signal_analysis_dialog_init_ui(const BtSignalAnalysisDialog *self) {
  BtMainWindow *main_window;
  BtSong *song;
  GstBin *bin;
  GstPad *pad;
  GstBus *bus;
  gchar *title=NULL;
  //GdkPixbuf *window_icon=NULL;
  GtkWidget *vbox, *hbox, *table;
  GtkWidget *ruler,*combo;
  gboolean res=TRUE;

  gtk_widget_set_name(GTK_WIDGET(self),"wire analysis");

  g_object_get(self->priv->app,"main-window",&main_window,"song",&song,NULL);
  gtk_window_set_transient_for(GTK_WINDOW(self),GTK_WINDOW(main_window));

  /* @todo: create and set *proper* window icon (analyzer, scope)
  if((window_icon=bt_ui_resources_get_pixbuf_by_wire(self->priv->element))) {
    gtk_window_set_icon(GTK_WINDOW(self),window_icon);
  }
  */

  /* DEBUG
  self->priv->min_rms=1000.0;
  self->priv->max_rms=-1000.0;
  self->priv->min_peak=1000.0;
  self->priv->max_peak=-1000.0;
  // DEBUG */

  // leave the choice of width to gtk
  gtk_window_set_default_size(GTK_WINDOW(self),-1,200);

  // TODO: different names for wire or sink machine
  if(BT_IS_WIRE(self->priv->element)) {
    BtMachine *src_machine,*dst_machine;
    gchar *src_id,*dst_id;

    g_object_get(self->priv->element,"src",&src_machine,"dst",&dst_machine,NULL);
    g_object_get(src_machine,"id",&src_id,NULL);
    g_object_get(dst_machine,"id",&dst_id,NULL);
    // set dialog title : machine -> machine analysis
    title=g_strdup_printf(_("%s -> %s analysis"),src_id,dst_id);
    g_object_unref(src_machine);
    g_object_unref(dst_machine);
    g_free(src_id);g_free(dst_id);
  } else if(BT_IS_SINK_MACHINE(self->priv->element)) {
    title=g_strdup(_("master analysis"));
  } else {
    GST_WARNING("unsupported object for signal analyser: %s,%p", G_OBJECT_TYPE_NAME(self->priv->element), self->priv->element);
  }
  if (title) {
    gtk_window_set_title(GTK_WINDOW(self),title);
    g_free(title);
  }

  vbox=gtk_vbox_new(FALSE, 0);

  /* add scales for spectrum analyzer drawable */
  /* @todo: we need to use a gtk_table() and also add a vruler with levels */
  ruler=gtk_hruler_new();
  GTK_RULER_GET_CLASS(ruler)->draw_pos = NULL;
  gtk_widget_set_size_request(ruler,-1,30);
  gtk_box_pack_start(GTK_BOX(vbox), ruler, FALSE, FALSE,0);
  self->priv->spectrum_ruler=ruler;
  update_spectrum_ruler(self);

  /* add spectrum canvas */
  self->priv->spectrum_drawingarea=gtk_drawing_area_new();
  gtk_widget_set_size_request(self->priv->spectrum_drawingarea, self->priv->spect_bands, self->priv->spect_height);
  g_signal_connect(G_OBJECT (self->priv->spectrum_drawingarea), "size-allocate",
      G_CALLBACK(on_size_allocate), (gpointer) self);
  gtk_box_pack_start(GTK_BOX(vbox), self->priv->spectrum_drawingarea, TRUE, TRUE, 0);

  /* spacer */
  gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(" "), FALSE, FALSE, 0);

  /* add scales for level meter */
  hbox = gtk_hbox_new(FALSE, 0);
  ruler=gtk_hruler_new();
  gtk_ruler_set_range(GTK_RULER(ruler),100.0,0.0,-10.0,30.0);
  //gtk_ruler_set_metric(GTK_RULER(ruler),&ruler_metrics[0]);
  GTK_RULER_GET_CLASS(ruler)->draw_pos = NULL;
  gtk_widget_set_size_request(ruler,-1,30);
  gtk_box_pack_start(GTK_BOX(hbox), ruler, TRUE, TRUE, 0);
  ruler=gtk_hruler_new();
  gtk_ruler_set_range(GTK_RULER(ruler),0.0,100.5,-10.0,30.0);
  //gtk_ruler_set_metric(GTK_RULER(ruler),&ruler_metrics[0]);
  GTK_RULER_GET_CLASS(ruler)->draw_pos = NULL;
  gtk_widget_set_size_request(ruler,-1,30);
  gtk_box_pack_start(GTK_BOX(hbox), ruler, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  /* add level-meter canvas */
  self->priv->level_drawingarea=gtk_drawing_area_new();
  gtk_widget_set_size_request(self->priv->level_drawingarea, self->priv->spect_bands, LEVEL_HEIGHT);
  gtk_box_pack_start(GTK_BOX(vbox), self->priv->level_drawingarea, FALSE, FALSE, 0);

  /* spacer */
  gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(" "), FALSE, FALSE, 0);

  /* settings */
  table=gtk_table_new(2,2,FALSE);

  combo=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),_("lin."));
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),_("log."));
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo),0);
  g_signal_connect(combo, "changed", G_CALLBACK(on_spectrum_frequency_mapping_changed), (gpointer)self);
  gtk_table_attach(GTK_TABLE(table),gtk_label_new(_("spectrum frequency")), 0,1, 0,1, 0,0, 3,3);
  gtk_table_attach(GTK_TABLE(table),combo, 1,2, 0,1, GTK_EXPAND|GTK_FILL,0, 3,3);

  combo=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),_("single"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),_("double"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),_("tripple"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo),0);
  g_signal_connect(combo, "changed", G_CALLBACK(on_spectrum_frequency_precision_changed), (gpointer)self);
  gtk_table_attach(GTK_TABLE(table),gtk_label_new(_("spectrum frequency")), 0,1, 1,2, 0,0, 3,3);
  gtk_table_attach(GTK_TABLE(table),combo, 1,2, 1,2, GTK_EXPAND|GTK_FILL,0, 3,3);

  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

  gtk_container_set_border_width(GTK_CONTAINER(self), 6);
  gtk_container_add(GTK_CONTAINER(self), vbox);

  /* @todo: better error handling
   * - don't fail if we miss only spectrum or level
   * - also don't return false, but instead add a label with the error message
   */

  // create fakesink
  if(!bt_signal_analysis_dialog_make_element(self,ANALYZER_FAKESINK,"fakesink")) {
    res=FALSE;
    goto Error;
  }
  g_object_set (self->priv->analyzers[ANALYZER_FAKESINK],
      "sync", FALSE, "qos", FALSE, "silent", TRUE, "async", FALSE,
      NULL);
  // create spectrum analyzer
  if(!bt_signal_analysis_dialog_make_element(self,ANALYZER_SPECTRUM,"spectrum")) {
    res=FALSE;
    goto Error;
  }
  // added in gst-plugin-good 0.10.29
  if(g_object_class_find_property(G_OBJECT_GET_CLASS(self->priv->analyzers[ANALYZER_SPECTRUM]),"multi-channel")) {
    g_object_set (self->priv->analyzers[ANALYZER_SPECTRUM],
        "interval",UPDATE_INTERVAL,"message",TRUE,
        "bands", self->priv->spect_bands*self->priv->frq_precision,
        "threshold", SPECTRUM_FLOOR,
        "multi-channel", TRUE,
        NULL);
  } else {
    g_object_set (self->priv->analyzers[ANALYZER_SPECTRUM],
        "interval",UPDATE_INTERVAL,"message",TRUE,
        "bands", self->priv->spect_bands*self->priv->frq_precision,
        "threshold", SPECTRUM_FLOOR,
        NULL);
  }
  if((pad=gst_element_get_static_pad(self->priv->analyzers[ANALYZER_SPECTRUM],"sink"))) {
    g_signal_connect(pad,"notify::caps",G_CALLBACK(on_caps_negotiated),(gpointer)self);
    gst_object_unref(pad);
  }

  // create level meter
  if(!bt_signal_analysis_dialog_make_element(self,ANALYZER_LEVEL,"level")) {
    res=FALSE;
    goto Error;
  }
  g_object_set(self->priv->analyzers[ANALYZER_LEVEL],
      "interval",UPDATE_INTERVAL,"message",TRUE,
      "peak-ttl",UPDATE_INTERVAL*3,"peak-falloff", 80.0,
      NULL);
  // create queue
  if(!bt_signal_analysis_dialog_make_element(self,ANALYZER_QUEUE,"queue")) {
    res=FALSE;
    goto Error;
  }
  // leave "max-size-buffer >> 1, if 1 every buffer gets marked as discont!
  g_object_set(self->priv->analyzers[ANALYZER_QUEUE],
      "max-size-buffers",10,"max-size-bytes",0,"max-size-time",G_GUINT64_CONSTANT(0),
      "leaky",2,NULL);

  if(BT_IS_WIRE(self->priv->element)) {
    g_object_set(self->priv->element,"analyzers",self->priv->analyzers_list,NULL);
  } else if(BT_IS_SINK_MACHINE(self->priv->element)) {
    // FIXME: handle sink-bin: get machine, set analyzers there?
    // or should sink-machine just handle that (that saves us the checking here and below)
  }

  g_object_get(song,"bin", &bin, NULL);
  bus=gst_element_get_bus(GST_ELEMENT(bin));
  g_signal_connect(bus, "message::element", G_CALLBACK(on_signal_analyser_change), (gpointer)self);
  gst_object_unref(bus);
  self->priv->clock=gst_pipeline_get_clock (GST_PIPELINE(bin));
  gst_object_unref(bin);

  // allocate visual ressources after the window has been realized
  g_signal_connect((gpointer)self,"realize",G_CALLBACK(bt_signal_analysis_dialog_realize),(gpointer)self);
  // redraw when needed
  g_signal_connect(self->priv->level_drawingarea,"expose-event",G_CALLBACK(bt_signal_analysis_dialog_level_expose),(gpointer)self);
  g_signal_connect(self->priv->spectrum_drawingarea,"expose-event",G_CALLBACK(bt_signal_analysis_dialog_spectrum_expose),(gpointer)self);

Error:
  g_object_unref(song);
  g_object_unref(main_window);
  return(res);
}

//-- constructor methods

/**
 * bt_signal_analysis_dialog_new:
 * @element: the wire/machine object to create the dialog for
 *
 * Create a new instance
 *
 * Returns: the new instance or %NULL in case of an error
 */
BtSignalAnalysisDialog *bt_signal_analysis_dialog_new(const GstBin *element) {
  BtSignalAnalysisDialog *self;

  self=BT_SIGNAL_ANALYSIS_DIALOG(g_object_new(BT_TYPE_SIGNAL_ANALYSIS_DIALOG,"element",element,NULL));
  // generate UI
  if(!bt_signal_analysis_dialog_init_ui(self)) {
    goto Error;
  }
  gtk_widget_show_all(GTK_WIDGET(self));
  GST_DEBUG("dialog created and shown");
  return(self);
Error:
  gtk_widget_destroy(GTK_WIDGET(self));
  return(NULL);
}

//-- methods

//-- wrapper

//-- class internals

static void bt_signal_analysis_dialog_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
  BtSignalAnalysisDialog *self = BT_SIGNAL_ANALYSIS_DIALOG(object);
  return_if_disposed();
  switch (property_id) {
    case SIGNAL_ANALYSIS_DIALOG_ELEMENT: {
      g_object_try_unref(self->priv->element);
      self->priv->element = g_value_dup_object(value);
    } break;
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

static void bt_signal_analysis_dialog_dispose(GObject *object) {
  BtSignalAnalysisDialog *self = BT_SIGNAL_ANALYSIS_DIALOG(object);
  BtSong *song;

  return_if_disposed();
  self->priv->dispose_has_run = TRUE;

  GST_DEBUG("!!!! self=%p",self);

  /* DEBUG
  GST_DEBUG("levels: rms =%7.4lf .. %7.4lf",self->priv->min_rms ,self->priv->max_rms);
  GST_DEBUG("levels: peak=%7.4lf .. %7.4lf",self->priv->min_peak,self->priv->max_peak);
  // DEBUG */

  if(self->priv->clock)
    gst_object_unref(self->priv->clock);

  if(self->priv->spect_grad)
    cairo_pattern_destroy(self->priv->spect_grad);

  GST_DEBUG("!!!! removing signal handler");

  g_object_get(self->priv->app,"song",&song,NULL);
  if(song) {
    GstBin *bin;
    GstBus *bus;

    g_object_get(song,"bin", &bin, NULL);

    bus=gst_element_get_bus(GST_ELEMENT(bin));
    g_signal_handlers_disconnect_matched(bus,G_SIGNAL_MATCH_FUNC,0,0,NULL,on_signal_analyser_change,NULL);
    gst_object_unref(bus);
    gst_object_unref(bin);
    g_object_unref(song);
  }

  // this destroys the analyzers too
  GST_DEBUG("!!!! free analyzers");
  if(BT_IS_WIRE(self->priv->element)) {
    g_object_set(self->priv->element,"analyzers",NULL,NULL);
  } else {
    // FIXME: handle sink-bin
  }

  g_object_unref(self->priv->element);
  g_object_unref(self->priv->app);

  GST_DEBUG("!!!! done");

  G_OBJECT_CLASS(bt_signal_analysis_dialog_parent_class)->dispose(object);
}

static void bt_signal_analysis_dialog_finalize(GObject *object) {
  BtSignalAnalysisDialog *self = BT_SIGNAL_ANALYSIS_DIALOG(object);

  GST_DEBUG("!!!! self=%p",self);

  g_free(self->priv->spect[0]);
  g_free(self->priv->spect[1]);
  g_free(self->priv->graph_log10);
  g_list_free(self->priv->analyzers_list);
  g_mutex_free(self->priv->lock);

  GST_DEBUG("!!!! done");

  G_OBJECT_CLASS(bt_signal_analysis_dialog_parent_class)->finalize(object);
}

static void bt_signal_analysis_dialog_init(BtSignalAnalysisDialog *self) {
  gdouble *grid_log10;
  guint i;
  gdouble f,inc,end;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, BT_TYPE_SIGNAL_ANALYSIS_DIALOG, BtSignalAnalysisDialogPrivate);
  GST_DEBUG("!!!! self=%p",self);
  self->priv->app = bt_edit_application_new();

  self->priv->lock = g_mutex_new();

  self->priv->spect_height = 64;
  self->priv->spect_bands = 256;
  self->priv->height_scale = 1.0;

  self->priv->frq_map = MAP_LIN;
  self->priv->frq_precision = 1;

  update_spectrum_analyzer(self);

  self->priv->srate = GST_AUDIO_DEF_RATE;

  /* precalc some log10 values */
  grid_log10 = self->priv->grid_log10;
  i=0;f=1.0;inc=1.0;end=10.0;
  while(i<60) {
    grid_log10[i++]=log10(1.0+f);
    f+=inc;
    if(f>=end) {
      f=inc=end;
      end*=10.0;
    }
  }
}

static void bt_signal_analysis_dialog_class_init(BtSignalAnalysisDialogClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  bus_msg_level_quark=g_quark_from_static_string("level");
  bus_msg_spectrum_quark=g_quark_from_static_string("spectrum");

  g_type_class_add_private(klass,sizeof(BtSignalAnalysisDialogPrivate));

  gobject_class->set_property = bt_signal_analysis_dialog_set_property;
  gobject_class->dispose      = bt_signal_analysis_dialog_dispose;
  gobject_class->finalize     = bt_signal_analysis_dialog_finalize;

  g_object_class_install_property(gobject_class,SIGNAL_ANALYSIS_DIALOG_ELEMENT,
                                  g_param_spec_object("element",
                                     "element construct prop",
                                     "Set wire/machine object, the dialog handles",
                                     GST_TYPE_BIN, /* object type */
                                     G_PARAM_CONSTRUCT_ONLY|G_PARAM_WRITABLE|G_PARAM_STATIC_STRINGS));

}
