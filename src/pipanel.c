/*============================================================================
Copyright (c) 2014-2025 Raspberry Pi Holdings Ltd.
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

/* Current configuration */
Config cur_conf;

/* Flag to indicate window manager in use */
wm_type wm;

/* Original theme in use */
static int orig_darkmode;

/* Original cursor size */
int orig_csize;

/* Dialogs */
static GtkWidget *dlg, *msg_dlg;

/* Monitor list for combos */
static GtkListStore *mons;
GtkTreeModel *sortmons;

/* Number of desktops */
int ndesks;

#ifdef PLUGIN_NAME
GtkBuilder *builder;
#else
static gulong draw_id;
static int st_tab;          /* Starting tab value read from command line */
#endif

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static int n_desktops (void);
static void message (char *msg);
static gboolean ok_clicked (GtkButton *button, gpointer data);
static void backup_file (char *filepath);
static void backup_config_files (void);
static int restore_file (char *filepath);
static int restore_config_files (void);
static gpointer restore_thread (gpointer ptr);
static gboolean cancel_main (GtkButton *button, gpointer data);
static gboolean ok_main (GtkButton *button, gpointer data);
static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data);
static gboolean init_config (gpointer data);

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

char *rgba_to_gdk_color_string (GdkRGBA *col)
{
    int r, g, b;
    r = col->red * 255;
    g = col->green * 255;
    b = col->blue * 255;
    return g_strdup_printf ("#%02X%02X%02X", r, g, b);
}

const char *session (void)
{
    const char *session_name =  g_getenv ("DESKTOP_SESSION");
    if (!session_name) return "LXDE-pi";
    if (!strncmp (session_name, "LXDE-pi", 7)) return "LXDE-pi";
    else return session_name;
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

/*----------------------------------------------------------------------------*/
/* Message box                                                                */
/*----------------------------------------------------------------------------*/

static void message (char *msg)
{
    GtkWidget *wid;
    GtkBuilder *builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/pipanel.ui");

    msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    if (dlg) gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (dlg));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
    gtk_label_set_text (GTK_LABEL (wid), msg);

    gtk_widget_show (msg_dlg);

    g_object_unref (builder);
}

static gboolean ok_clicked (GtkButton *button, gpointer data)
{
    gtk_widget_destroy (msg_dlg);
    return FALSE;
}

void message_ok (char *msg)
{
    GtkWidget *wid;
    GtkBuilder *builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/pipanel.ui");

    msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    if (dlg) gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (dlg));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
    gtk_label_set_text (GTK_LABEL (wid), msg);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_buttons");
    gtk_widget_show (wid);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_ok");
    gtk_widget_show (wid);
    g_signal_connect (wid, "clicked", G_CALLBACK (ok_clicked), NULL);
    gtk_widget_grab_focus (wid);

    gtk_widget_show (msg_dlg);

    g_object_unref (builder);
}

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
    const char *session_name = session ();
    char *path, *lc_sess, *fname, *monname;
    int i;

    // delete any old backups and create a new backup directory
    path = g_build_filename (g_get_home_dir (), ".pp_backup", NULL);
    if (g_file_test (path, G_FILE_TEST_IS_DIR)) vsystem ("rm -rf %s", path);
    g_mkdir_with_parents (path, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (path);

    lc_sess = g_ascii_strdown (session_name, -1);
    fname = g_strconcat (lc_sess, "-rc.xml", NULL);
    path = g_build_filename (".config/openbox", fname, NULL);
    backup_file (path);
    g_free (path);
    g_free (fname);
    g_free (lc_sess);

    path = g_build_filename (".config/lxsession", session_name, "desktop.conf", NULL);
    backup_file (path);
    g_free (path);

    path = g_build_filename (".config/lxpanel", session_name, "panels/panel", NULL);
    backup_file (path);
    g_free (path);

    for (i = 0; i < ndesks; i++)
    {
        fname = g_strdup_printf ("desktop-items-%d.conf", i);
        path = g_build_filename (".config/pcmanfm", session_name, fname, NULL);
        backup_file (path);
        g_free (path);
        g_free (fname);

        if (wm != WM_OPENBOX)
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            monname = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), i);
#pragma GCC diagnostic pop
            fname = g_strdup_printf ("desktop-items-%s.conf", monname);
            path = g_build_filename (".config/pcmanfm", session_name, fname, NULL);
            backup_file (path);
            g_free (path);
            g_free (fname);
            g_free (monname);
        }
    }

    path = g_build_filename (".config/pcmanfm", session_name, "pcmanfm.conf", NULL);
    backup_file (path);
    g_free (path);

    path = g_build_filename (".local/share/themes", DEFAULT_THEME, "gtk-3.0/gtk.css", NULL);
    backup_file (path);
    g_free (path);

    path = g_build_filename (".local/share/themes", DEFAULT_THEME_DARK, "gtk-3.0/gtk.css", NULL);
    backup_file (path);
    g_free (path);

    backup_file (".config/wf-panel-pi.ini");
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
    const char *session_name = session ();
    char *path, *lc_sess, *fname, *monname;
    int i, changed = 0;

    lc_sess = g_ascii_strdown (session_name, -1);
    fname = g_strconcat (lc_sess, "-rc.xml", NULL);
    path = g_build_filename (".config/openbox", fname, NULL);
    restore_file (path);
    g_free (path);
    g_free (fname);
    g_free (lc_sess);

    path = g_build_filename (".config/lxsession", session_name, "desktop.conf", NULL);
    if (restore_file (path)) changed = 1;
    g_free (path);

    path = g_build_filename (".config/lxpanel", session_name, "panels/panel", NULL);
    if (restore_file (path)) changed = 1;
    g_free (path);

    for (i = 0; i < ndesks; i++)
    {
        fname = g_strdup_printf ("desktop-items-%d.conf", i);
        path = g_build_filename (".config/pcmanfm", session_name, fname, NULL);
        if (restore_file (path)) changed = 1;
        g_free (path);
        g_free (fname);

        if (wm != WM_OPENBOX)
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            monname = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), i);
#pragma GCC diagnostic pop
            fname = g_strdup_printf ("desktop-items-%s.conf", monname);
            path = g_build_filename (".config/pcmanfm", session_name, fname, NULL);
            if (restore_file (path)) changed = 1;
            g_free (path);
            g_free (fname);
            g_free (monname);
        }
    }

    path = g_build_filename (".config/pcmanfm", session_name, "pcmanfm.conf", NULL);
    if (restore_file (path)) changed = 1;
    g_free (path);

    path = g_build_filename (".local/share/themes", DEFAULT_THEME, "gtk-3.0/gtk.css", NULL);
    if (restore_file (path)) changed = 1;
    g_free (path);

    path = g_build_filename (".local/share/themes", DEFAULT_THEME_DARK, "gtk-3.0/gtk.css", NULL);
    if (restore_file (path)) changed = 1;
    g_free (path);

    if (restore_file (".config/wf-panel-pi.ini")) changed = 1;
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
    if (restore_config_files ())
    {
        cur_conf.darkmode = orig_darkmode;
        set_theme (TEMP_THEME);
        reload_session ();
        reload_gsettings ();
        reload_panel ();
        reload_desktop ();
        reload_theme (TRUE);
    }
    else gtk_main_quit ();
    return NULL;
}

/*----------------------------------------------------------------------------*/
/* Control handlers                                                           */
/*----------------------------------------------------------------------------*/

static gboolean cancel_main (GtkButton *button, gpointer data)
{
    if (orig_darkmode != cur_conf.darkmode)
    {
        if (!system ("pgrep geany > /dev/null"))
        {
            message_ok (_("The theme for Geany cannot be changed while it is open.\nPlease close it and try again."));
            return FALSE;
        }

        if (!system ("pgrep galculator > /dev/null"))
        {
            message_ok (_("The theme for Calculator cannot be changed while it is open.\nPlease close it and try again."));
            return FALSE;
        }
    }
    message (_("Restoring configuration - please wait..."));
    g_thread_new (NULL, restore_thread, NULL);
    return FALSE;
}

static gboolean ok_main (GtkButton *button, gpointer data)
{
    gtk_main_quit ();
    return FALSE;
}

static gboolean close_prog (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_main_quit ();
    return TRUE;
}

/*----------------------------------------------------------------------------*/
/* Main window creation                                                       */
/*----------------------------------------------------------------------------*/

static gboolean init_config (gpointer data)
{
#ifndef PLUGIN_NAME
    GtkBuilder *builder;
#endif
    GtkWidget *wid;
    int i;
    char *buf;

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

    // build the UI
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/pipanel.ui");

    dlg = (GtkWidget *) gtk_builder_get_object (builder, "main_window");
    g_signal_connect (dlg, "delete_event", G_CALLBACK (close_prog), NULL);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "button_ok");
    g_signal_connect (wid, "clicked", G_CALLBACK (ok_main), NULL);
    wid = (GtkWidget *) gtk_builder_get_object (builder, "button_cancel");
    g_signal_connect (wid, "clicked", G_CALLBACK (cancel_main), NULL);

    // create default data structures
    create_defaults ();

    // load current state and controls
    load_desktop_tab (builder);
    load_taskbar_tab (builder);
    load_system_tab (builder);
    load_defaults_tab (builder);

    // create session file to be tracked
    init_lxsession (cur_conf.darkmode ? DEFAULT_THEME_DARK : DEFAULT_THEME);

    // backup current configuration for cancel
    backup_config_files ();
    orig_csize = cur_conf.cursor_size;
    orig_darkmode = cur_conf.darkmode;

    // set up controls to match current state of data
    set_desktop_controls ();
    set_taskbar_controls ();
    set_system_controls ();

#ifndef PLUGIN_NAME
    // set the initial tab
    if (st_tab < 0)
    {
        wid = (GtkWidget *) gtk_builder_get_object (builder, "notebook1");
        gtk_notebook_set_current_page (GTK_NOTEBOOK (wid), 4 + st_tab);
    }

    g_object_unref (builder);

    gtk_widget_show (dlg);
    gtk_widget_destroy (msg_dlg);
#endif

    return FALSE;
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

    init_config (NULL);
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
    }

    gtk_container_remove (GTK_CONTAINER (window), plugin);

    return plugin;
}

gboolean reboot_needed (void)
{
    if (wm == WM_OPENBOX && cur_conf.cursor_size != orig_csize) return TRUE;
    return FALSE;
}

void free_plugin (void)
{
    g_object_unref (builder);
}

#else

static gboolean event (GtkWidget *wid, GdkEventWindowState *ev, gpointer data)
{
    if (ev->type == GDK_WINDOW_STATE)
    {
        if (ev->changed_mask == GDK_WINDOW_STATE_FOCUSED
            && ev->new_window_state & GDK_WINDOW_STATE_FOCUSED)
                g_idle_add (init_config, NULL);
    }
    return FALSE;
}

static gboolean draw (GtkWidget *wid, cairo_t *cr, gpointer data)
{
    g_signal_handler_disconnect (wid, draw_id);
    g_idle_add (init_config, NULL);
    return FALSE;
}

int main (int argc, char *argv[])
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    // read starting tab if there is one
    if (argc > 1)
    {
        if (sscanf (argv[1], "%d", &st_tab) != 1) st_tab = 0;
    }

    // GTK setup
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE")) wm = WM_WAYFIRE;
        else wm = WM_LABWC;
    }
    else wm = WM_OPENBOX;

    message (_("Loading configuration - please wait..."));
    if (wm != WM_OPENBOX) g_signal_connect (msg_dlg, "event", G_CALLBACK (event), NULL);
    else draw_id = g_signal_connect (msg_dlg, "draw", G_CALLBACK (draw), NULL);

    gtk_main ();

    gtk_widget_destroy (dlg);

    return 0;
}

#endif

/* End of file */
/*----------------------------------------------------------------------------*/
