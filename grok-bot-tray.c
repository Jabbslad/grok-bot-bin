#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libappindicator/app-indicator.h>

static GPid grok_pid;
static const char *grok_executable;

static void show_grok(GtkMenuItem *item, gpointer data) {
  gchar *argv[] = {(gchar *)grok_executable, NULL};
  GError *error = NULL;

  (void)item;
  (void)data;

  /* Grok is single-instance; launching it again presents the existing window. */
  if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL,
                     &error)) {
    g_warning("Could not show Grok Bot: %s", error->message);
    g_error_free(error);
  }
}

static void copy_version(GtkMenuItem *item, gpointer data) {
  (void)item;

  gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), data, -1);
}

static void open_data_folder(GtkMenuItem *item, gpointer data) {
  gchar *argv[] = {"xdg-open", (gchar *)data, NULL};
  GError *error = NULL;

  (void)item;

  if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
                     &error)) {
    g_warning("Could not open data folder: %s", error->message);
    g_error_free(error);
  }
}

static void quit_grok(GtkMenuItem *item, gpointer data) {
  (void)item;
  (void)data;

  kill(grok_pid, SIGTERM);
  gtk_main_quit();
}

static gboolean watch_grok(gpointer data) {
  (void)data;

  if (kill(grok_pid, 0) == -1 && errno == ESRCH) {
    gtk_main_quit();
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

int main(int argc, char **argv) {
  char *end = NULL;
  long parsed_pid;
  AppIndicator *indicator;
  GtkWidget *menu;
  GtkWidget *version_item;
  GtkWidget *separator;
  GtkWidget *show_item;
  GtkWidget *open_folder_item;
  GtkWidget *quit_item;
  gchar *version_label;
  gchar *data_dir;

  if (argc != 4)
    return EXIT_FAILURE;

  parsed_pid = strtol(argv[1], &end, 10);
  if (*argv[1] == '\0' || *end != '\0' || parsed_pid <= 0)
    return EXIT_FAILURE;

  grok_pid = (GPid)parsed_pid;
  grok_executable = argv[2];

  gtk_init(&argc, &argv);

  indicator = app_indicator_new("grok-bot", "grok-bot",
                                APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
  app_indicator_set_title(indicator, "Grok Bot");
  app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);

  menu = gtk_menu_new();
  version_label = g_strdup_printf("Version %s", argv[3]);
  version_item = gtk_menu_item_new_with_label(version_label);
  separator = gtk_separator_menu_item_new();
  show_item = gtk_menu_item_new_with_label("Show");
  open_folder_item = gtk_menu_item_new_with_label("Open Data Folder");
  quit_item = gtk_menu_item_new_with_label("Quit");
  data_dir = g_build_filename(g_get_user_config_dir(), "Grok Bot", NULL);
  g_signal_connect(show_item, "activate", G_CALLBACK(show_grok), NULL);
  g_signal_connect(open_folder_item, "activate", G_CALLBACK(open_data_folder),
                   data_dir);
  g_signal_connect(quit_item, "activate", G_CALLBACK(quit_grok), NULL);
  /* Clicking the version item copies the bare version string. */
  g_signal_connect(version_item, "activate", G_CALLBACK(copy_version),
                   argv[3]);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), show_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), open_folder_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), version_item);
  gtk_widget_show_all(menu);
  app_indicator_set_menu(indicator, GTK_MENU(menu));
  /* Middle-click on the tray icon activates Show. */
  app_indicator_set_secondary_activate_target(indicator, show_item);
  g_free(version_label);

  g_timeout_add(500, watch_grok, NULL);
  gtk_main();

  g_object_unref(indicator);
  return EXIT_SUCCESS;
}
