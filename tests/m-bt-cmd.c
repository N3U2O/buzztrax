/* $Id: m-bt-cmd.c,v 1.5 2005-08-05 09:36:19 ensonic Exp $
 * command line app unit tests
 */

#define BT_CHECK
 
#include "bt-check.h"

GST_DEBUG_CATEGORY(GST_CAT_DEFAULT);

extern Suite *bt_cmd_application_suite(void);

guint test_argc=1;
gchar *test_arg0="check_buzzard";
gchar *test_argv[1];
gchar **test_argvptr;

/* start the test run */
int main(int argc, char **argv) {
  int nf; 
  SRunner *sr;

  g_type_init();
  setup_log(argc,argv);
  setup_log_capture();
  test_argv[0]=test_arg0;
  test_argvptr=test_argv;
  
  g_log_set_always_fatal(G_LOG_LEVEL_WARNING);
  GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "bt-check", 0, "music production environment / unit tests");
  gst_debug_set_threshold_for_name("GST_*",GST_LEVEL_WARNING); // set this to e.g. DEBUG to see more from gst in the log
  gst_debug_set_threshold_for_name("bt-*",GST_LEVEL_DEBUG);
  gst_debug_category_set_threshold(bt_check_debug,GST_LEVEL_DEBUG);

  sr=srunner_create(bt_cmd_application_suite());
  // this make tracing errors with gdb easier
  //srunner_set_fork_status(sr,CK_NOFORK);
  srunner_run_all(sr,CK_NORMAL);
  nf=srunner_ntests_failed(sr);
  srunner_free(sr);

  return(nf==0) ? EXIT_SUCCESS : EXIT_FAILURE; 
}
