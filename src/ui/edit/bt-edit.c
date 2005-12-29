// $Id: bt-edit.c,v 1.28 2005-12-29 21:10:39 ensonic Exp $
/**
 * SECTION:btedit
 * @short_description: buzztard graphical editor application
 *
 * Implements the body of the buzztard GUI editor.
 * 
 * You can try to run the uninstalled program via
 * <informalexample><programlisting>
 *   libtool --mode=execute bt-edit
 * </programlisting></informalexample>
 * to enable debug output add:
 * <informalexample><programlisting>
 *  --gst-debug="*:2,bt-*:3" for not-so-much-logdata or
 *  --gst-debug="*:2,bt-*:4" for a-lot-logdata
 * </programlisting></informalexample>
 *
 * Example songs can be found in <filename>./test/songs/</filename>.
 */

#define BT_EDIT
#define BT_EDIT_C

#include "bt-edit.h"

static void usage(int argc, char **argv, GOptionContext *ctx) {
  //poptPrintUsage(context,stdout,0);
  //poptFreeContext(context);
  //exit(0);
}

int main(int argc, char **argv) {
  gboolean res=FALSE;
  gboolean arg_version=FALSE;
  gchar *command=NULL,*input_file_name=NULL;
  BtEditApplication *app;
  GOptionContext *ctx;
  GError *err=NULL;
  
  GOptionEntry options[] = {
    {"version",     '\0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE,     &arg_version,     N_("Show version"),    NULL },
    {"command",     '\0', 0,                    G_OPTION_ARG_STRING,   &command,         N_("Command name"),    N_("{load}") },
    {"input-file",  '\0', 0,                    G_OPTION_ARG_FILENAME, &input_file_name, N_("Input file name"), N_("SONGFILE") },
    POPT_TABLEEND
  };
  
  // in case we ever want to use a custom theme for buzztard:
  // gtk_rc_parse(DATADIR""G_DIR_SEPARATOR_S"themes"G_DIR_SEPARATOR_S"buzztard"G_DIR_SEPARATOR_S"gtk-2.0"G_DIR_SEPARATOR_S"gtkrc");
  
  /*
  if(!g_thread_supported()) {  // are g_threads() already initialized
    g_thread_init(NULL);
  }
  gdk_threads_init();
  bt_threads_init();
  */

  // init libraries
  ctx = g_option_context_new(NULL);
  g_option_context_add_main_entries (ctx, options, PACKAGE_NAME);
  bt_init_add_option_groups(ctx);
  g_option_context_add_group(ctx, gtk_get_option_group(TRUE));
  if(!g_option_context_parse(ctx, &argc, &argv, &err)) {
    g_print("Error initializing: %s\n", safe_string(err->message));
    exit(1);
  }

  GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "bt-edit", 0, "music production environment / editor ui");
  
  add_pixmap_directory(DATADIR""G_DIR_SEPARATOR_S"pixmaps"G_DIR_SEPARATOR_S);

  if(arg_version) {
    g_printf("%s from "PACKAGE_STRING"\n",argv[0]);
    exit(0);
  }

  //gdk_threads_enter();
  app=bt_edit_application_new();
  if(command) {
    // depending on the popt options call the correct method
    if(!strncmp(command,"load",4)) {
      if(!input_file_name) {
        usage(argc, argv, ctx);
        // if commandline options where wrong, just start
        res=bt_edit_application_run(app);
      }
      else {
        res=bt_edit_application_load_and_run(app,input_file_name);
      }
    }
    else {
      usage(argc, argv, ctx);
      // if commandline options where wrong, just start
      res=bt_edit_application_run(app);
    }
  }
  else {
    res=bt_edit_application_run(app);
  }
  //gdk_threads_leave();
  
  // free application
  GST_INFO("app->ref_ct=%d",G_OBJECT(app)->ref_count);
  g_option_context_free(ctx);
  g_object_unref(app);
  return(!res);
}
