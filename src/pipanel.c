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

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void update_greeter (void);
static int n_desktops (void);
static gboolean ok_clicked (GtkButton *button, gpointer data);
static void init_config (void);

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

char *rgba_alpha_to_gdk_color_string (GdkRGBA *col)
{
    int r, g, b;
    r = col->red * 255;
    g = col->green * 255;
    b = col->blue * 255;
    return g_strdup_printf ("rgba(%d,%d,%d,%.2f)", r, g, b, col->alpha);
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
    load_theme_tab (builder);
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

void init_plugin (GtkWidget *)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    if (getenv ("WAYLAND_DISPLAY")) wm = WM_LABWC;
    else wm = WM_OPENBOX;

    main_dlg = NULL;
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/pipanel.ui");

    init_config ();
}

int plugin_tabs (void)
{
    if (wm != WM_OPENBOX) return 5;
    else return 4;
}

const char *tab_name (int tab)
{
    switch (tab)
    {
        case 0 : return C_("tab", "Desktop");
        case 1 : return C_("tab", "Taskbar");
        case 2 : return C_("tab", "Theme");
        case 3 : return C_("tab", "Defaults");
        case 4 : return C_("tab", "Dock");
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
        case 4 : return "appset-dock";
        default : return NULL;
    }
}

const char *tab_id (int tab)
{
    switch (tab)
    {
        case 0 : return "desktop";
        case 1 : return "taskbar";
        case 4 : return "dock";
        default : return NULL;
    }
}

GtkWidget *get_tab (int tab)
{
    GtkWidget *window, *plugin;

    switch (tab)
    {
        case 0 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox1");
            window = (GtkWidget *) gtk_builder_get_object (builder, "desktop_window");
            break;
        case 1 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox2");
            window = (GtkWidget *) gtk_builder_get_object (builder, "taskbar_window");
            break;
        case 2 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox3");
            window = (GtkWidget *) gtk_builder_get_object (builder, "theme_window");
            break;
        case 3 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox4");
            window = (GtkWidget *) gtk_builder_get_object (builder, "defaults_window");
            break;
        case 4 :
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox5");
            window = (GtkWidget *) gtk_builder_get_object (builder, "dock_window");
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

/* End of file */
/*----------------------------------------------------------------------------*/
