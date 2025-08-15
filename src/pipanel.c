/*============================================================================
Copyright (c) 2014-2025 Raspberry Pi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "pipanel.h"

#include "desktop.h"
#include "taskbar.h"
#include "system.h"
#include "defaults.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#define MAX_X_DESKTOPS 2

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

static GtkBuilder *builder;

/* Dialogs */
static GtkWidget *main_dlg, *msg_dlg;

/* Current configuration */
Config cur_conf;

/* Flag to indicate window manager in use */
wm_type wm;

/* Monitor list for combos */
static GtkListStore *mons;
GtkTreeModel *sortmons;

/* Number of desktops */
int ndesks;

/* Is new theme available? */
gboolean trix_theme = FALSE;

#ifndef PLUGIN_NAME
static gulong draw_id;

/* Starting tab value read from command line */
static char *st_tab;

/* Original theme in use */
static int orig_darkmode;
#endif

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void update_greeter (void);
static int n_desktops (void);
static gboolean ok_clicked (GtkButton *button, gpointer data);
static void init_config (void);
#ifndef PLUGIN_NAME
static void backup_file (char *filepath);
static void backup_config_files (void);
static int restore_file (char *filepath);
static int restore_config_files (void);
static gpointer restore_thread (gpointer ptr);
static gboolean ok_main (GtkButton *button, gpointer data);
static gboolean cancel_main (GtkButton *button, gpointer data);
static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data);
static gboolean init_window (gpointer data);
static gboolean draw (GtkWidget *wid, cairo_t *cr, gpointer data);
#endif

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

int vsystem (const char *fmt, ...)
{
    char *cmdline;
    int res;

    va_list arg;
    va_start (arg, fmt);
    g_vasprintf (&cmdline, fmt, arg);
    va_end (arg);
    res = system (cmdline);
    g_free (cmdline);
    return res;
}

char *get_string (char *cmd)
{
    char *line = NULL, *res = NULL;
    size_t len = 0;
    FILE *fp = popen (cmd, "r");

    if (fp == NULL) return g_strdup ("");
    if (getline (&line, &len, fp) > 0)
    {
        res = line;
        while (*res++) if (g_ascii_isspace (*res)) *res = 0;
        res = g_strdup (line);
    }
    pclose (fp);
    g_free (line);
    return res ? res : g_strdup ("");
}

char *get_quoted_string (char *cmd)
{
    char *line = NULL, *res = NULL;
    size_t len = 0;
    FILE *fp = popen (cmd, "r");

    if (fp == NULL) return g_strdup ("");
    if (getline (&line, &len, fp) > 0)
    {
        res = line;
        while (*res++) if (*res == '\'') *res = 0;
        res = g_strdup (line + 1);
    }
    pclose (fp);
    g_free (line);
    return res ? res : g_strdup ("");
}

char *rgba_to_gdk_color_string (GdkRGBA *col)
{
    int r, g, b;
    r = col->red * 255;
    g = col->green * 255;
    b = col->blue * 255;
    return g_strdup_printf ("#%02X%02X%02X", r, g, b);
}

static int n_desktops (void)
{
    int n, m;
    char *res;

    if (wm != WM_OPENBOX)
        res = get_string ("wlr-randr | grep -cv '^ '");
    else
        res = get_string ("xrandr -q | grep -cw connected");

    n = sscanf (res, "%d", &m);
    g_free (res);

    if (n == 1 && m >= 1) return m;
    return 1;
}

void check_directory (const char *path)
{
    char *dir = g_path_get_dirname (path);
    g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (dir);
}

const char *theme_name (int dark)
{
    if (dark == TEMP)
    {
        if (trix_theme)
            return is_dark () ? "tPiXonyx" : "tPiXtrix";
        else
            return is_dark () ? "tPiXnoir" : "tPiXflat";
    }
    else
    {
        if (trix_theme)
            return dark ? "PiXonyx" : "PiXtrix";
        else
            return dark ? "PiXnoir" : "PiXflat";
    }
}

static void update_greeter (void)
{
    if (g_file_test (GREETER_TMP, G_FILE_TEST_IS_REGULAR))
    {
        system (SUDO_PREFIX "cp " GREETER_TMP " /etc/lightdm/pi-greeter.conf");
        remove (GREETER_TMP);
    }
}

/*----------------------------------------------------------------------------*/
/* Message box                                                                */
/*----------------------------------------------------------------------------*/

void message (char *msg, gboolean ok)
{
    GtkWidget *wid;
    GtkBuilder *builder;

    textdomain (GETTEXT_PACKAGE);
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/pipanel.ui");

    msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    if (main_dlg) gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (main_dlg));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
    gtk_label_set_text (GTK_LABEL (wid), msg);

    if (ok)
    {
        wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_buttons");
        gtk_widget_show (wid);

        wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_ok");
        gtk_widget_show (wid);
        g_signal_connect (wid, "clicked", G_CALLBACK (ok_clicked), NULL);
        gtk_widget_grab_focus (wid);
    }

    gtk_widget_show (msg_dlg);

    g_object_unref (builder);
}

static gboolean ok_clicked (GtkButton *button, gpointer data)
{
    gtk_widget_destroy (msg_dlg);
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Initial configuration                                                      */
/*----------------------------------------------------------------------------*/

static void init_config (void)
{
    int i;
    char *buf;
    struct stat st;

    // check to see if new theme is installed
    if (stat ("/usr/share/themes/PiXtrix", &st) == 0) trix_theme = TRUE;

    // find the number of monitors
    ndesks = n_desktops ();
    if (ndesks > MAX_DESKTOPS) ndesks = MAX_DESKTOPS;
    if (wm == WM_OPENBOX && ndesks > MAX_X_DESKTOPS) ndesks = MAX_X_DESKTOPS;

    // load monitor names into list store
    mons = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
    for (i = 0; i < ndesks; i++)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), i);
#pragma GCC diagnostic pop
        gtk_list_store_insert_with_values (mons, NULL, i, 0, i, 1, buf, -1);
        g_free (buf);
    }
    sortmons = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (mons));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sortmons), 1, GTK_SORT_ASCENDING);

    // create default data structures
    create_defaults ();

    // load current state and controls
    load_desktop_tab (builder);
    load_taskbar_tab (builder);
    load_system_tab (builder);
    load_defaults_tab (builder);

    // create session file to be tracked
    init_session (theme_name (cur_conf.darkmode));

    // set up controls to match current state of data
    set_desktop_controls ();
    set_taskbar_controls ();
    set_system_controls ();
}

/*----------------------------------------------------------------------------*/
/* Plugin interface                                                           */
/*----------------------------------------------------------------------------*/

#ifdef PLUGIN_NAME

void init_plugin (void)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE")) wm = WM_WAYFIRE;
        else wm = WM_LABWC;
    }
    else wm = WM_OPENBOX;

    main_dlg = NULL;
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/pipanel.ui");

    init_config ();
}

int plugin_tabs (void)
{
    return 4;
}

const char *tab_name (int tab)
{
    switch (tab)
    {
        case 0 : return C_("tab", "Desktop");
        case 1 : return C_("tab", "Taskbar");
        case 2 : return C_("tab", "Theme");
        case 3 : return C_("tab", "Defaults");
        default : return _("No such tab");
    }
}

const char *icon_name (int tab)
{
    switch (tab)
    {
        case 0 : return "appset-desktop";
        case 1 : return "appset-taskbar";
        case 2 : return "preferences-desktop-theme";
        case 3 : return "applications-utilities";
        default : return NULL;
    }
}

const char *tab_id (int tab)
{
    switch (tab)
    {
        case 0 : return "desktop";
        case 1 : return "taskbar";
        default : return NULL;
    }
}

GtkWidget *get_tab (int tab)
{
    GtkWidget *window, *plugin;

    window = (GtkWidget *) gtk_builder_get_object (builder, "notebook1");
    switch (tab)
    {
        case 0 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox1");
            break;
        case 1 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox2");
            break;
        case 2 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox3");
            break;
        case 3 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox4");
            break;
        default :
            plugin = NULL;
    }

    gtk_container_remove (GTK_CONTAINER (window), plugin);

    return plugin;
}

gboolean reboot_needed (void)
{
    update_greeter ();
    return system_reboot ();
}

void free_plugin (void)
{
    g_object_unref (builder);
}

#else

/*----------------------------------------------------------------------------*/
/* Backup and restore (for cancel)                                            */
/*----------------------------------------------------------------------------*/

static void backup_file (char *filepath)
{
    // filepath must be relative to current user's home directory
    char *orig = g_build_filename (g_get_home_dir (), filepath, NULL);
    char *backup = g_build_filename (g_get_home_dir (), ".pp_backup", filepath, NULL);

    if (g_file_test (orig, G_FILE_TEST_IS_REGULAR))
    {
        check_directory (backup);
        vsystem ("cp %s %s", orig, backup);
    }
    g_free (backup);
    g_free (orig);
}

static void backup_config_files (void)
{
    char *path, *monname;
    int i;

    // delete any old backups and create a new backup directory
    path = g_build_filename (g_get_home_dir (), ".pp_backup", NULL);
    if (g_file_test (path, G_FILE_TEST_IS_DIR)) vsystem ("rm -rf %s", path);
    g_mkdir_with_parents (path, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (path);

    backup_file (".config/openbox/rpd-rc.xml");
    backup_file (".config/lxsession/rpd-x/desktop.conf");
    backup_file (".config/lxpanel-pi/panels/panel");
    backup_file (".config/pcmanfm/default/pcmanfm.conf");

    for (i = 0; i < ndesks; i++)
    {
        path = g_strdup_printf (".config/pcmanfm/default/desktop-items-%d.conf", i);
        backup_file (path);
        g_free (path);

        if (wm != WM_OPENBOX)
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            monname = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), i);
#pragma GCC diagnostic pop
            path = g_strdup_printf (".config/pcmanfm/default/desktop-items-%s.conf", monname);
            backup_file (path);
            g_free (path);
            g_free (monname);
        }
    }

    path = g_build_filename (".local/share/themes", theme_name (LIGHT), "gtk-3.0/gtk.css", NULL);
    backup_file (path);
    g_free (path);

    path = g_build_filename (".local/share/themes", theme_name (DARK), "gtk-3.0/gtk.css", NULL);
    backup_file (path);
    g_free (path);

    backup_file (".config/wf-panel-pi/wf-panel-pi.ini");
    backup_file (".config/libfm/libfm.conf");
    backup_file (".config/gtk-3.0/gtk.css");
    backup_file (".config/qt5ct/qt5ct.conf");
    backup_file (".config/xsettingsd/xsettingsd.conf");
    backup_file (".config/wayfire.ini");
    backup_file (".config/labwc/themerc-override");
    backup_file (".config/labwc/rc.xml");
    backup_file (".config/labwc/environment");
    backup_file (".gtkrc-2.0");

    // app-specific
    backup_file (".config/lxterminal/lxterminal.conf");
    backup_file (".config/libreoffice/4/user/registrymodifications.xcu");
    backup_file (".config/geany/geany.conf");
    backup_file (".config/galculator/galculator.conf");
}

static int restore_file (char *filepath)
{
    // filepath must be relative to current user's home directory
    char *orig = g_build_filename (g_get_home_dir (), filepath, NULL);
    char *backup = g_build_filename (g_get_home_dir (), ".pp_backup", filepath, NULL);
    int changed = 1;

    if (g_file_test (backup, G_FILE_TEST_IS_REGULAR))
    {
        if (vsystem ("diff %s %s > /dev/null 2>&1", backup, orig) == 0) changed = 0;
        else vsystem ("cp %s %s", backup, orig);
    }
    else if (g_file_test (orig, G_FILE_TEST_IS_REGULAR))
    {
        g_remove (orig);
    }
    else changed = 0;
    g_free (backup);
    g_free (orig);

    return changed;
}

static int restore_config_files (void)
{
    char *path, *monname;
    int i, changed = 0;

    restore_file (".config/openbox/rpd-rc.xml");
    if (restore_file (".config/lxsession/rpd-x/desktop.conf")) changed = 1;
    if (restore_file (".config/lxpanel-pi/panels/panel")) changed = 1;
    if (restore_file (".config/pcmanfm/default/pcmanfm.conf")) changed = 1;

    for (i = 0; i < ndesks; i++)
    {
        path = g_strdup_printf (".config/pcmanfm/default/desktop-items-%d.conf", i);
        if (restore_file (path)) changed = 1;
        g_free (path);

        if (wm != WM_OPENBOX)
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            monname = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), i);
#pragma GCC diagnostic pop
            path = g_strdup_printf (".config/pcmanfm/default/desktop-items-%s.conf", monname);
            if (restore_file (path)) changed = 1;
            g_free (path);
            g_free (monname);
        }
    }

    path = g_build_filename (".local/share/themes", theme_name (LIGHT), "gtk-3.0/gtk.css", NULL);
    if (restore_file (path)) changed = 1;
    g_free (path);

    path = g_build_filename (".local/share/themes", theme_name (DARK), "gtk-3.0/gtk.css", NULL);
    if (restore_file (path)) changed = 1;
    g_free (path);

    if (restore_file (".config/wf-panel-pi/wf-panel-pi.ini")) changed = 1;
    if (restore_file (".config/libfm/libfm.conf")) changed = 1;
    if (restore_file (".config/gtk-3.0/gtk.css")) changed = 1;
    if (restore_file (".config/qt5ct/qt5ct.conf")) changed = 1;
    if (restore_file (".config/xsettingsd/xsettingsd.conf")) changed = 1;
    if (restore_file (".config/wayfire.ini")) changed = 1;
    if (restore_file (".config/labwc/themerc-override")) changed = 1;
    if (restore_file (".config/labwc/rc.xml")) changed = 1;
    if (restore_file (".config/labwc/environment")) changed = 1;
    if (restore_file (".gtkrc-2.0")) changed = 1;

    // app-specific
    if (restore_file (".config/lxterminal/lxterminal.conf")) changed = 1;
    if (restore_file (".config/libreoffice/4/user/registrymodifications.xcu")) changed = 1;
    if (restore_file (".config/geany/geany.conf")) changed = 1;
    if (restore_file (".config/galculator/galculator.conf")) changed = 1;

    return changed;
}

static gpointer restore_thread (gpointer ptr)
{
    restore_gsettings ();
    if (restore_config_files ())
    {
        cur_conf.darkmode = orig_darkmode;
        set_theme (theme_name (TEMP));
        reload_session ();
        reload_panel ();
        reload_desktop ();
        reload_theme (TRUE);
    }
    else gtk_main_quit ();
    return NULL;
}

/*----------------------------------------------------------------------------*/
/* Main window button handlers                                                */
/*----------------------------------------------------------------------------*/

static gboolean ok_main (GtkButton *button, gpointer data)
{
    update_greeter ();
    gtk_main_quit ();
    return FALSE;
}

static gboolean cancel_main (GtkButton *button, gpointer data)
{
    if (orig_darkmode != cur_conf.darkmode)
    {
        if (!system ("pgrep geany > /dev/null"))
        {
            message (_("The theme for Geany cannot be changed while it is open.\nPlease close it and try again."), TRUE);
            return FALSE;
        }

        if (!system ("pgrep galculator > /dev/null"))
        {
            message (_("The theme for Calculator cannot be changed while it is open.\nPlease close it and try again."), TRUE);
            return FALSE;
        }
    }
    message (_("Restoring configuration - please wait..."), FALSE);
    g_thread_new (NULL, restore_thread, NULL);
    return FALSE;
}

static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    update_greeter ();
    gtk_main_quit ();
    return TRUE;
}

/*----------------------------------------------------------------------------*/
/* Main window                                                                */
/*----------------------------------------------------------------------------*/

static gboolean init_window (gpointer data)
{
    GtkWidget *wid;

    init_config ();

    // backup current configuration for cancel
    backup_config_files ();
    orig_darkmode = cur_conf.darkmode;

    // set the initial tab
    if (st_tab)
    {
        wid = (GtkWidget *) gtk_builder_get_object (builder, "notebook1");
        if (!g_strcmp0 (st_tab, "desktop")) gtk_notebook_set_current_page (GTK_NOTEBOOK (wid), 0);
        if (!g_strcmp0 (st_tab, "taskbar")) gtk_notebook_set_current_page (GTK_NOTEBOOK (wid), 1);
    }

    g_object_unref (builder);

    gtk_widget_show (main_dlg);
    gtk_widget_destroy (msg_dlg);

    return FALSE;
}

static gboolean draw (GtkWidget *wid, cairo_t *cr, gpointer data)
{
    g_signal_handler_disconnect (wid, draw_id);
    g_idle_add (init_window, NULL);
    return FALSE;
}

int main (int argc, char *argv[])
{
    GtkWidget *wid;

    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE")) wm = WM_WAYFIRE;
        else wm = WM_LABWC;
    }
    else wm = WM_OPENBOX;

    main_dlg = NULL;
    gtk_init (&argc, &argv);

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/pipanel.ui");

    main_dlg = (GtkWidget *) gtk_builder_get_object (builder, "main_window");
    g_signal_connect (main_dlg, "delete_event", G_CALLBACK (close_prog), NULL);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "button_ok");
    g_signal_connect (wid, "clicked", G_CALLBACK (ok_main), NULL);
    wid = (GtkWidget *) gtk_builder_get_object (builder, "button_cancel");
    g_signal_connect (wid, "clicked", G_CALLBACK (cancel_main), NULL);

    message (_("Loading configuration - please wait..."), FALSE);
    draw_id = g_signal_connect (msg_dlg, "draw", G_CALLBACK (draw), NULL);

    // read starting tab if there is one
    if (argc > 1) st_tab = g_strdup (argv[1]);
    else st_tab = NULL;

    gtk_main ();

    return 0;
}

#endif

/* End of file */
/*----------------------------------------------------------------------------*/
