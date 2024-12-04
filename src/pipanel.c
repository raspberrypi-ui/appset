/*
Copyright (c) 2018 Raspberry Pi (Trading) Ltd.
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
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <locale.h>
#include <ctype.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define MAX_ICON 68
#define MIN_ICON 16

#define DEFAULT_THEME "PiXflat"
#define DEFAULT_THEME_DARK "PiXnoir"
#define DEFAULT_THEME_L "PiXflat_l"
#define DEFAULT_THEME_DARK_L "PiXnoir_l"
#define TEMP_THEME    "tPiXflat"

#define GREY    "#808080"

#define MAX_DESKTOPS 9
#define MAX_X_DESKTOPS 2

#define LARGE_ICON_THRESHOLD 20

#define DEFAULT(x) cur_conf.x=def_med.x

#define XC(str) ((xmlChar *) str)

/* Global variables for window values */
typedef struct {
    const char *desktop_folder;
    const char *desktop_picture;
    const char *desktop_mode;
    GdkRGBA desktop_colour;
    GdkRGBA desktoptext_colour;
    int show_docs;
    int show_trash;
    int show_mnts;
} DesktopConfig;

typedef struct {
    DesktopConfig desktops[MAX_DESKTOPS];
    const char *desktop_font;
    const char *terminal_font;
    GdkRGBA theme_colour[2];
    GdkRGBA themetext_colour[2];
    GdkRGBA bar_colour[2];
    GdkRGBA bartext_colour[2];
    int icon_size;
    int barpos;
    int folder_size;
    int thumb_size;
    int pane_size;
    int sicon_size;
    int tb_icon_size;
    int lo_icon_size;
    int cursor_size;
    int task_width;
    int handle_width;
    int scrollbar_width;
    int monitor;
    int common_bg;
    int darkmode;
} Config;

static Config cur_conf, def_lg, def_med, def_sm;

/* Flag to indicate whether lxsession is version 4.9 or later, in which case no need to refresh manually */

static gboolean needs_refresh;

/* Flag to indicate window manager in use */

typedef enum {
    WM_OPENBOX,
    WM_WAYFIRE,
    WM_LABWC } wm_type;
static wm_type wm;

/* Original theme in use */
static int orig_darkmode;

/* Version of Libreoffice installed - affects toolbar icon setting */
static char lo_ver;

/* Handler IDs so they can be blocked when needed */
static gulong cid, iid, bpid, blid, dmid, tdid, ttid, tmid, dfid, cbid, bdid, cdid;

/* Controls */
static GObject *hcol, *htcol, *font, *dcol, *dtcol, *dmod, *dpic, *bcol, *btcol, *rb1, *rb2, *rb5, *rb6;
static GObject *isz, *cb1, *cb2, *cb3, *cb4, *csz, *cmsg, *nb, *dfold, *cbdesk, *cbbar;

/* Dialogs */
static GtkWidget *dlg, *msg_dlg;

/* Monitor list for combos */
static GtkListStore *mons;
static GtkTreeModel *sortmons;

/* Starting tab value read from command line */
static int st_tab;

/* Desktop number */
static int ndesks, desktop_n;

GtkBuilder *builder;

static void check_directory (const char *path);
static void backup_file (char *filepath);
static void backup_config_files (void);
static void reload_gsettings (void);
static void delete_file (char *filepath);
static void reset_to_defaults (void);
static void load_lxsession_settings (void);
static void load_pcman_settings (int desktop);
static void load_pcman_g_settings (void);
static void load_lxpanel_settings (void);
static void load_obconf_settings (void);
static void load_wfpanel_settings (void);
static void load_gtk3_settings (void);
static void save_lxpanel_settings (void);
static void save_gtk3_settings (void);
static void save_lxsession_settings (void);
static void save_xsettings (void);
static void save_pcman_settings (int desktop);
static void save_pcman_g_settings (void);
static void save_libfm_settings (void);
static void save_wayfire_settings (void);
static void save_wfpanel_settings (void);
static void save_obconf_settings (gboolean lw);
static void save_labwc_to_settings (void);
static void save_labwc_env_settings (void);
static void save_lxterm_settings (void);
static void save_libreoffice_settings (void);
static void save_qt_settings (void);
static void save_app_settings (void);
static void add_or_amend (const char *conffile, const char *block, const char *param, const char *repl);
static void set_controls (void);
static void set_desktop_controls (void);
static void defaults_lxpanel (void);
static void defaults_lxsession (void);
static void defaults_pcman (int desktop);
static void defaults_pcman_g (void);
static void defaults_gtk3 (void);
static void create_defaults (void);
static void on_menu_size_set (GtkComboBox* btn, gpointer ptr);
static void on_theme_colour_set (GtkColorChooser* btn, gpointer ptr);
static void on_themetext_colour_set (GtkColorChooser* btn, gpointer ptr);
static void on_bar_colour_set (GtkColorChooser* btn, gpointer ptr);
static void on_bartext_colour_set (GtkColorChooser* btn, gpointer ptr);
static void on_desktop_colour_set (GtkColorChooser* btn, gpointer ptr);
static void on_desktoptext_colour_set (GtkColorChooser* btn, gpointer ptr);
static void on_desktop_picture_set (GtkFileChooser* btn, gpointer ptr);
static void on_desktop_font_set (GtkFontChooser* btn, gpointer ptr);
static void on_desktop_mode_set (GtkComboBox* btn, gpointer ptr);
static void on_desktop_folder_set (GtkFileChooser* btn, gpointer ptr);
static void on_desktop_changed (GtkComboBox* btn, gpointer ptr);
static void on_bar_loc_set (GtkComboBox* btn, gpointer ptr);
static void on_bar_pos_set (GtkRadioButton* btn, gpointer ptr);
static void on_toggle_docs (GtkCheckButton* btn, gpointer ptr);
static void on_toggle_trash (GtkCheckButton* btn, gpointer ptr);
static void on_toggle_mnts (GtkCheckButton* btn, gpointer ptr);
static void on_toggle_desktop (GtkCheckButton* btn, gpointer ptr);
static void on_darkmode_set (GtkRadioButton* btn, gpointer ptr);
static void on_cursor_size_set (GtkComboBox* btn, gpointer ptr);
static void on_set_defaults (GtkButton* btn, gpointer ptr);
static int n_desktops (void);

/* Utilities */

static int vsystem (const char *fmt, ...)
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

static char *get_string (char *cmd)
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

static gboolean read_version (char *package, int *maj, int *min, int *sub)
{
    char *cmd, *res;
    int val;

    cmd = g_strdup_printf ("dpkg -s %s 2> /dev/null | grep Version | rev | cut -d : -f 1 | rev", package);
    res = get_string (cmd);
    val = sscanf (res, "%d.%d.%d", maj, min, sub);
    g_free (cmd);
    g_free (res);
    if (val == 3) return TRUE;
    else return FALSE;
}

static gboolean ok_clicked (GtkButton *button, gpointer data)
{
    gtk_widget_destroy (msg_dlg);
    return FALSE;
}

static void message_ok (char *msg)
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

/* Create a labelled-by relationship between a widget and a label */

static void atk_label (GtkWidget *widget, GtkLabel *label)
{
    AtkObject *atk_widget, *atk_label;
    AtkRelationSet *relation_set;
    AtkRelation *relation;
    AtkObject *targets[1];

    atk_widget = gtk_widget_get_accessible (widget);
    atk_label = gtk_widget_get_accessible (GTK_WIDGET (label));
    relation_set = atk_object_ref_relation_set (atk_widget);
    targets[0] = atk_label;
    relation = atk_relation_new (targets, 1, ATK_RELATION_LABELLED_BY);
    atk_relation_set_add (relation_set, relation);
    g_object_unref (G_OBJECT (relation));
}

char *rgba_to_gdk_color_string (GdkRGBA *col)
{
    int r, g, b;
    r = col->red * 255;
    g = col->green * 255;
    b = col->blue * 255;
    return g_strdup_printf ("#%02X%02X%02X", r, g, b);
}

static void set_config_param (const char *file, const char *section, const char *tag, const char *value)
{
    char *str;
    GKeyFile *kf;
    gsize len;

    check_directory (file);

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_string (kf, section, tag, value);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
}

/* Shell commands to reload data */

static void reload_lxpanel (void)
{
    if (wm != WM_OPENBOX) return;

    vsystem ("lxpanelctl refresh");
}

static void reload_labwc (void)
{
    if (wm == WM_LABWC) vsystem ("labwc --reconfigure");
}

static void reload_openbox (void)
{
    reload_labwc ();
    if (wm == WM_OPENBOX) vsystem ("openbox --reconfigure");
}

static void reload_pcmanfm (void)
{
    vsystem ("pcmanfm --reconfigure");
}

static void reload_lxsession (void)
{
    if (wm != WM_OPENBOX) return;

    if (needs_refresh)
    {
        vsystem ("lxsession -r");
    }
}

static void reload_xsettings (void)
{
    if (wm == WM_OPENBOX) return;

    vsystem ("pgrep xsettingsd > /dev/null && killall -HUP xsettingsd");
}

static const char *session (void)
{
    const char *session_name =  g_getenv ("DESKTOP_SESSION");
    if (!session_name) return "LXDE-pi";
    if (!strncmp (session_name, "LXDE-pi", 7)) return "LXDE-pi";
    else return session_name;
}

static char *openbox_file (void)
{
    const char *session_name = session ();
    char *lc_sess = g_ascii_strdown (session_name, -1);
    char *fname = g_strconcat (lc_sess, "-rc.xml", NULL);
    char *path = g_build_filename (g_get_user_config_dir (), "openbox", fname, NULL);
    g_free (lc_sess);
    g_free (fname);
    return path;
}

static char *labwc_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "labwc", "rc.xml", NULL);
}

static char *lxsession_file (gboolean global)
{
    return g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "lxsession", session (), "desktop.conf", NULL);
}

static char *xsettings_file (gboolean global)
{
    return g_build_filename (global ? "/etc" : g_get_user_config_dir (), "xsettingsd/xsettingsd.conf", NULL);
}

static char *lxpanel_file (gboolean global)
{
    return g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "lxpanel", session (), "panels/panel", NULL);
}

static char *pcmanfm_file (gboolean global, int desktop, gboolean write)
{
    char *fname, *buf;
    if (desktop < 0 || desktop > MAX_DESKTOPS) return NULL;
    if (cur_conf.common_bg)
    {
        fname = g_strdup_printf ("desktop-items-0.conf");
        buf = g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), fname, NULL);
        g_free (fname);
        return buf;
    }

    if (wm != WM_OPENBOX)
    {
        buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), desktop);
        fname = g_strdup_printf ("desktop-items-%s.conf", buf);
        g_free (buf);
        buf = g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), fname, NULL);
        g_free (fname);
        if (write || access (buf, F_OK) == 0) return buf;
        else g_free (buf);
        if (global && desktop > 1) desktop = 1; // only 2 numbered global desktop files
    }

    fname = g_strdup_printf ("desktop-items-%u.conf", desktop);
    buf = g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), fname, NULL);
    g_free (fname);
    return buf;
}

static char *pcmanfm_g_file (gboolean global)
{
    return g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), "pcmanfm.conf", NULL);
}

static char *libfm_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "libfm/libfm.conf", NULL);
}

static char *wayfire_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);
}

static char *wfpanel_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "wf-panel-pi.ini", NULL);
}

static void check_directory (const char *path)
{
    char *dir = g_path_get_dirname (path);
    g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (dir);
}

// Functions to force reloading of GTK+3 theme

static void init_lxsession (const char *theme)
{
    char *user_config_file;

    /* Creates a default lxsession data file with the theme in it - the
     * system checks this for changes and reloads the theme if a change is detected */
    if (wm == WM_OPENBOX)
    {
        user_config_file = lxsession_file (FALSE);
        if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
        {
            check_directory (user_config_file);
            vsystem ("echo '[GTK]\nsNet/ThemeName=%s' >> %s", theme, user_config_file);
        }
    }
    else
    {
        vsystem ("gsettings set org.gnome.desktop.interface gtk-theme %s", theme);

        user_config_file = xsettings_file (FALSE);
        if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
        {
            check_directory (user_config_file);
            vsystem ("cp /etc/xsettingsd/xsettingsd.conf %s", user_config_file);
        }
    }
    g_free (user_config_file);
}

static void set_theme (const char *theme)
{
    char *user_config_file;

    /* Sets the theme in the lxsession data file, which triggers a theme change */
    if (wm == WM_OPENBOX)
    {
        user_config_file = lxsession_file (FALSE);
        vsystem ("sed -i s#sNet/ThemeName=.*#sNet/ThemeName=%s#g %s", theme, user_config_file);
        g_free (user_config_file);
    }
    else
    {
        vsystem ("gsettings set org.gnome.desktop.interface gtk-theme %s", theme);

        user_config_file = xsettings_file (FALSE);
        vsystem ("sed -i s#'Net/ThemeName.*'#'Net/ThemeName \"%s\"'#g %s", theme, user_config_file);
        g_free (user_config_file);
        reload_xsettings ();
    }
}

static int is_dark (void)
{
    int res;

    char *config_file = g_build_filename ("/usr/share/themes", DEFAULT_THEME_DARK, "gtk-3.0/gtk.css", NULL);
    if (access (config_file, F_OK)) return -1;
    g_free (config_file);

    if (wm == WM_OPENBOX)
    {
        char *user_config_file = lxsession_file (FALSE);
        res = vsystem ("grep sNet/ThemeName %s | grep -q %s", user_config_file, DEFAULT_THEME_DARK);
        g_free (user_config_file);
    }
    else res = vsystem ("gsettings get org.gnome.desktop.interface gtk-theme | grep -q %s", DEFAULT_THEME_DARK);

    if (!res) return 1;
    else return 0;
}

static gboolean restore_theme (gpointer data)
{
    /* Resets the theme to the default, causing it to take effect */
    set_theme (cur_conf.darkmode ? DEFAULT_THEME_DARK : DEFAULT_THEME);
    if (data) gtk_main_quit ();
    return FALSE;
}

static void reload_theme (long int quit)
{
    g_idle_add (restore_theme, (gpointer) quit);
}

// File handling for backing up, restoring and resetting config

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
            monname = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), i);
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

static void reload_gsettings (void)
{
    if (wm != WM_OPENBOX)
    {
        load_lxsession_settings ();
        vsystem ("gsettings set org.gnome.desktop.interface font-name \"%s\"", cur_conf.desktop_font);
        vsystem ("gsettings set org.gnome.desktop.interface cursor-size %d", cur_conf.cursor_size);
        switch (cur_conf.tb_icon_size)
        {
            case 16:    vsystem ("gsettings set org.gnome.desktop.interface toolbar-icons-size small");
                        break;
            case 48:    vsystem ("gsettings set org.gnome.desktop.interface toolbar-icons-size large");
                        break;
            default:    vsystem ("gsettings set org.gnome.desktop.interface toolbar-icons-size medium");
                        break;
        }
    }
}

static void delete_file (char *filepath)
{
    char *orig = g_build_filename (g_get_home_dir (), filepath, NULL);

    if (g_file_test (orig, G_FILE_TEST_IS_REGULAR))
    {
        g_remove (orig);
    }
    g_free (orig);
}

static void reset_to_defaults (void)
{
    const char *session_name = session ();
    char *path, *lc_sess, *fname, *monname;
    int i;

    lc_sess = g_ascii_strdown (session_name, -1);
    fname = g_strconcat (lc_sess, "-rc.xml", NULL);
    path = g_build_filename (".config/openbox", fname, NULL);
    delete_file (path);
    g_free (path);
    g_free (fname);
    g_free (lc_sess);

    path = g_build_filename (".config/lxsession", session_name, "desktop.conf", NULL);
    delete_file (path);
    g_free (path);

    path = g_build_filename (".config/lxpanel", session_name, "panels/panel", NULL);
    delete_file (path);
    g_free (path);

    for (i = 0; i < ndesks; i++)
    {
        fname = g_strdup_printf ("desktop-items-%d.conf", i);
        path = g_build_filename (".config/pcmanfm", session_name, fname, NULL);
        delete_file (path);
        g_free (path);
        g_free (fname);

        if (wm != WM_OPENBOX)
        {
            monname = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), i);
            fname = g_strdup_printf ("desktop-items-%s.conf", monname);
            path = g_build_filename (".config/pcmanfm", session_name, fname, NULL);
            delete_file (path);
            g_free (path);
            g_free (fname);
            g_free (monname);
        }
    }

    path = g_build_filename (".config/pcmanfm", session_name, "pcmanfm.conf", NULL);
    delete_file (path);
    g_free (path);

    path = g_build_filename (".local/share/themes", DEFAULT_THEME, "gtk-3.0/gtk.css", NULL);
    delete_file (path);
    g_free (path);

    path = g_build_filename (".local/share/themes", DEFAULT_THEME_DARK, "gtk-3.0/gtk.css", NULL);
    delete_file (path);
    g_free (path);

    delete_file (".config/libfm/libfm.conf");
    delete_file (".config/gtk-3.0/gtk.css");
    delete_file (".config/qt5ct/qt5ct.conf");
    delete_file (".config/xsettingsd/xsettingsd.conf");
    delete_file (".config/labwc/themerc-override");
    delete_file (".gtkrc-2.0");

    reload_gsettings ();
    init_lxsession (TEMP_THEME);
}


/* Functions to load required values from user config files */

static void load_lxsession_settings (void)
{
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;
    int val;

    user_config_file = lxsession_file (FALSE);

    // read in data from file to a key file structure
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        g_key_file_free (kf);
        g_free (user_config_file);
        DEFAULT (desktop_font);
        DEFAULT (tb_icon_size);
        DEFAULT (cursor_size);
        return;
    }

    // get data from the key file
    err = NULL;
    ret = g_key_file_get_string (kf, "GTK", "sGtk/FontName", &err);
    if (err == NULL) cur_conf.desktop_font = g_strdup (ret);
    else DEFAULT (desktop_font);
    g_free (ret);

    err = NULL;
    ret = g_key_file_get_string (kf, "GTK", "sGtk/IconSizes", &err);
    DEFAULT (tb_icon_size);
    if (err == NULL)
    {
        if (sscanf (ret, "gtk-large-toolbar=%d,", &val) == 1)
        {
            if (val >= 8 && val <= 256) cur_conf.tb_icon_size = val;
        }
    }
    g_free (ret);

    err = NULL;
    val = g_key_file_get_integer (kf, "GTK", "iGtk/CursorThemeSize", &err);
    if (err == NULL && val >= 24 && val <= 48) cur_conf.cursor_size = val;
    else DEFAULT (cursor_size);

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_pcman_settings (int desktop)
{
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from file to a key file
    user_config_file = pcmanfm_file (FALSE, desktop, FALSE);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_bg", &err);
        if (err == NULL)
        {
            if (!gdk_rgba_parse (&cur_conf.desktops[desktop].desktop_colour, ret))
                DEFAULT (desktops[desktop].desktop_colour);
        }
        else DEFAULT (desktops[desktop].desktop_colour);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_fg", &err);
        if (err == NULL)
        {
            if (!gdk_rgba_parse (&cur_conf.desktops[desktop].desktoptext_colour, ret))
                DEFAULT (desktops[desktop].desktoptext_colour);
        }
        else DEFAULT (desktops[desktop].desktoptext_colour);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper", &err);
        if (err == NULL && ret) cur_conf.desktops[desktop].desktop_picture = g_strdup (ret);
        else DEFAULT (desktops[desktop].desktop_picture);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper_mode", &err);
        if (err == NULL && ret) cur_conf.desktops[desktop].desktop_mode = g_strdup (ret);
        else DEFAULT (desktops[desktop].desktop_mode);
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_documents", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.desktops[desktop].show_docs = val;
        else DEFAULT (desktops[desktop].show_docs);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_trash", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.desktops[desktop].show_trash = val;
        else DEFAULT (desktops[desktop].show_trash);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_mounts", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.desktops[desktop].show_mnts = val;
        else DEFAULT (desktops[desktop].show_mnts);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "folder", &err);
        if (err == NULL && ret) cur_conf.desktops[desktop].desktop_folder = g_strdup (ret);
        else DEFAULT (desktops[desktop].desktop_folder);
        g_free (ret);
    }
    else
    {
        DEFAULT (desktops[desktop].desktop_colour);
        DEFAULT (desktops[desktop].desktoptext_colour);
        DEFAULT (desktops[desktop].desktop_picture);
        DEFAULT (desktops[desktop].desktop_mode);
        DEFAULT (desktops[desktop].show_docs);
        DEFAULT (desktops[desktop].show_trash);
        DEFAULT (desktops[desktop].show_mnts);
        DEFAULT (desktops[desktop].desktop_folder);
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_pcman_g_settings (void)
{
    char *user_config_file;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from file to a key file
    user_config_file = pcmanfm_g_file (FALSE);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "common_bg", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.common_bg = val;
        else DEFAULT (common_bg);
    }
    else
    {
        DEFAULT (common_bg);
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_lxpanel_settings (void)
{
    char *user_config_file, *cmdbuf, *res;
    int val;

    user_config_file = lxpanel_file (FALSE);
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        DEFAULT (barpos);
        DEFAULT (icon_size);
        DEFAULT (task_width);
        DEFAULT (monitor);
        g_free (user_config_file);
        return;
    }

    if (!vsystem ("grep -q edge=bottom %s", user_config_file)) cur_conf.barpos = 1;
    else DEFAULT (barpos);

    if (!vsystem ("grep -q monitor=1 %s", user_config_file)) cur_conf.monitor = 1;
    else DEFAULT (monitor);

    cmdbuf = g_strdup_printf ("grep -Po '(?<=iconsize=)[0-9]+' %s", user_config_file);
    res = get_string (cmdbuf);
    if (res[0] && sscanf (res, "%d", &val) == 1) cur_conf.icon_size = val;
    else DEFAULT (icon_size);
    g_free (res);
    g_free (cmdbuf);

    cmdbuf = g_strdup_printf ("grep -Po '(?<=MaxTaskWidth=)[0-9]+' %s", user_config_file);
    res = get_string (cmdbuf);
    if (res[0] && sscanf (res, "%d", &val) == 1) cur_conf.task_width = val;
    else DEFAULT (task_width);
    g_free (res);
    g_free (cmdbuf);

    g_free (user_config_file);
}

static void load_obconf_settings (void)
{
    char *user_config_file;
    int val;

    DEFAULT (handle_width);

    user_config_file = openbox_file ();
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        g_free (user_config_file);
        return;
    }

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    xmlDocPtr xDoc = xmlParseFile (user_config_file);
    if (xDoc == NULL)
    {
        g_free (user_config_file);
        return;
    }

    xmlXPathContextPtr xpathCtx = xmlXPathNewContext (xDoc);
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='invHandleWidth']", xpathCtx);
    xmlNode *node = xpathObj->nodesetval->nodeTab[0];
    if (node)
    {
         if (sscanf ((const char *) xmlNodeGetContent (node), "%d", &val) == 1 && val > 0) cur_conf.handle_width = val;
    }

    // cleanup XML
    xmlXPathFreeObject (xpathObj);
    xmlXPathFreeContext (xpathCtx);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_config_file);
}

static void load_wfpanel_settings (void)
{
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from file to a key file
    user_config_file = wfpanel_file ();
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "panel", "position", &err);
        if (err == NULL && ret && !strcmp (ret, "bottom")) cur_conf.barpos = 1;
        else DEFAULT (barpos);
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "panel", "icon_size", &err);
        if (err == NULL && val >= 16 && val <= 48) cur_conf.icon_size = val + 4;
        else DEFAULT (icon_size);

        err = NULL;
        val = g_key_file_get_integer (kf, "panel", "window-list_max_width", &err);
        if (err == NULL) cur_conf.task_width = val;
        else DEFAULT (task_width);

        err = NULL;
        ret = g_key_file_get_string (kf, "panel", "monitor", &err);
        DEFAULT (monitor);
        if (err == NULL && ret)
        {
            for (val = 0; val < ndesks; val++)
            {
                char *buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), val);
                if (!g_strcmp0 (buf, ret)) cur_conf.monitor = val;
                g_free (buf);
            }
        }
    }
    else
    {
        DEFAULT (barpos);
        DEFAULT (icon_size);
        DEFAULT (task_width);
        DEFAULT (monitor);
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_gtk3_settings (void)
{
    char *user_config_file, *sys_config_file, *cmdbuf, *res;
    int dark;

    cur_conf.darkmode = (is_dark () == 1) ? TRUE : FALSE;
    orig_darkmode = cur_conf.darkmode;

    cur_conf.scrollbar_width = 13;
    user_config_file = g_build_filename (g_get_user_data_dir (), "themes", cur_conf.darkmode ? DEFAULT_THEME_DARK : DEFAULT_THEME, "gtk-3.0/gtk.css", NULL);
    if (!vsystem ("grep -q \"min-width: 17px\" %s 2> /dev/null", user_config_file)) cur_conf.scrollbar_width = 17;
    g_free (user_config_file);

    for (dark = 0; dark < 2; dark++)
    {
        sys_config_file = g_build_filename ("/usr/share/themes", dark ? DEFAULT_THEME_DARK : DEFAULT_THEME, "gtk-3.0/!(*-dark).css", NULL);
        user_config_file = g_build_filename (g_get_user_data_dir (), "themes", dark ? DEFAULT_THEME_DARK : DEFAULT_THEME, "gtk-3.0/*.css", NULL);

        cmdbuf = g_strdup_printf ("grep -hPo '(?<=@define-color\\stheme_selected_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", user_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&cur_conf.theme_colour[dark], res))
        {
            g_free (res);
            cmdbuf = g_strdup_printf ("bash -O extglob -c \"grep -hPo '(?<=@define-color\\stheme_selected_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null\"", sys_config_file);
            res = get_string (cmdbuf);
            g_free (cmdbuf);
            if (!res[0] || !gdk_rgba_parse (&cur_conf.theme_colour[dark], res)) DEFAULT (theme_colour[dark]);
        }
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -hPo '(?<=@define-color\\stheme_selected_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", user_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&cur_conf.themetext_colour[dark], res))
        {
            g_free (res);
            cmdbuf = g_strdup_printf ("bash -O extglob -c \"grep -hPo '(?<=@define-color\\stheme_selected_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null\"", sys_config_file);
            res = get_string (cmdbuf);
            g_free (cmdbuf);
            if (!res[0] || !gdk_rgba_parse (&cur_conf.themetext_colour[dark], res)) DEFAULT (themetext_colour[dark]);
        }
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -hPo '(?<=@define-color\\sbar_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", user_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&cur_conf.bar_colour[dark], res))
        {
            g_free (res);
            cmdbuf = g_strdup_printf ("bash -O extglob -c \"grep -hPo '(?<=@define-color\\sbar_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null\"", sys_config_file);
            res = get_string (cmdbuf);
            g_free (cmdbuf);
            if (!res[0] || !gdk_rgba_parse (&cur_conf.bar_colour[dark], res)) DEFAULT (bar_colour[dark]);
        }
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -hPo '(?<=@define-color\\sbar_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", user_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&cur_conf.bartext_colour[dark], res))
        {
            g_free (res);
            cmdbuf = g_strdup_printf ("bash -O extglob -c \"grep -hPo '(?<=@define-color\\sbar_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null\"", sys_config_file);
            res = get_string (cmdbuf);
            g_free (cmdbuf);
            if (!res[0] || !gdk_rgba_parse (&cur_conf.bartext_colour[dark], res)) DEFAULT (bartext_colour[dark]);
        }
        g_free (res);

        g_free (user_config_file);
        g_free (sys_config_file);
    }
}


/* Functions to save settings back to relevant files */

static void save_lxpanel_settings (void)
{
    char *user_config_file;

    // sanity check
    if (cur_conf.icon_size > MAX_ICON || cur_conf.icon_size < MIN_ICON) return;

    user_config_file = lxpanel_file (FALSE);
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        // need a local copy to take the changes
        check_directory (user_config_file);
        vsystem ("cp /etc/xdg/lxpanel/%s/panels/panel %s", session (), user_config_file);
    }

    // use sed to write
    vsystem ("sed -i s/iconsize=.*/iconsize=%d/g %s", cur_conf.icon_size, user_config_file);
    vsystem ("sed -i s/height=.*/height=%d/g %s", cur_conf.icon_size, user_config_file);
    vsystem ("sed -i s/edge=.*/edge=%s/g %s", cur_conf.barpos ? "bottom" : "top", user_config_file);
    vsystem ("sed -i s/MaxTaskWidth=.*/MaxTaskWidth=%d/g %s", cur_conf.task_width, user_config_file);
    vsystem ("sed -i s/monitor=.*/monitor=%d/g %s", cur_conf.monitor, user_config_file);

    g_free (user_config_file);
}

static void save_gtk3_settings (void)
{
    char *user_config_file, *cstrb, *cstrf, *cstrbb, *cstrbf, *link1, *link2, *repl;
    int dark;

    // delete old file used to store general overrides
    user_config_file = g_build_filename (g_get_user_config_dir (), "gtk-3.0/gtk.css", NULL);
    vsystem ("if grep -q -s define-color %s ; then rm %s ; fi", user_config_file, user_config_file);
    g_free (user_config_file);

    // create a temp theme to switch to
    link1 = g_build_filename (g_get_user_data_dir (), "themes", TEMP_THEME, NULL);
    if (!g_file_test (link1, G_FILE_TEST_IS_DIR))
    {
        link2 = g_build_filename (g_get_user_data_dir (), "themes", DEFAULT_THEME, NULL);
        symlink (link2, link1);
        g_free (link2);
    }
    g_free (link1);

    for (dark = 0; dark < 2; dark++)
    {
        cstrb = rgba_to_gdk_color_string (&cur_conf.theme_colour[dark]);
        cstrf = rgba_to_gdk_color_string (&cur_conf.themetext_colour[dark]);
        cstrbb = rgba_to_gdk_color_string (&cur_conf.bar_colour[dark]);
        cstrbf = rgba_to_gdk_color_string (&cur_conf.bartext_colour[dark]);

        // construct the file path
        user_config_file = g_build_filename (g_get_user_data_dir (), "themes", dark ? DEFAULT_THEME_DARK : DEFAULT_THEME, "gtk-3.0/gtk.css", NULL);
        check_directory (user_config_file);

        if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
        {
            vsystem ("echo '@import url(\"/usr/share/themes/%s/gtk-3.0/gtk.css\");' >> %s", dark ? DEFAULT_THEME_DARK : DEFAULT_THEME, user_config_file);
            vsystem ("echo '@define-color theme_selected_bg_color %s;' >> %s", cstrb, user_config_file);
            vsystem ("echo '@define-color theme_selected_fg_color %s;' >> %s", cstrf, user_config_file);
            vsystem ("echo '@define-color bar_bg_color %s;' >> %s", cstrbb, user_config_file);
            vsystem ("echo '@define-color bar_fg_color %s;' >> %s", cstrbf, user_config_file);
            vsystem ("echo '\nscrollbar button {\n\tmin-width: %dpx;\n\tmin-height: %dpx;\n}' >> %s", cur_conf.scrollbar_width, cur_conf.scrollbar_width, user_config_file);
            vsystem ("echo '\nscrollbar slider {\n\tmin-width: %dpx;\n\tmin-height: %dpx;\n}' >> %s", cur_conf.scrollbar_width - 6, cur_conf.scrollbar_width - 6, user_config_file);

            g_free (cstrf);
            g_free (cstrb);
            g_free (cstrbf);
            g_free (cstrbb);
            g_free (user_config_file);
            return;
        }

        // amend entries already in file, or add if not present
        if (vsystem ("grep -q theme_selected_bg_color %s\n", user_config_file))
            vsystem ("echo '@define-color theme_selected_bg_color %s;' >> %s", cstrb, user_config_file);
        else
            vsystem ("sed -i s/'theme_selected_bg_color #......'/'theme_selected_bg_color %s'/g %s", cstrb, user_config_file);

        if (vsystem ("grep -q theme_selected_fg_color %s\n", user_config_file))
            vsystem ("echo '@define-color theme_selected_fg_color %s;' >> %s", cstrf, user_config_file);
        else
            vsystem ("sed -i s/'theme_selected_fg_color #......'/'theme_selected_fg_color %s'/g %s", cstrf, user_config_file);

        if (vsystem ("grep -q bar_bg_color %s\n", user_config_file))
            vsystem ("echo '@define-color bar_bg_color %s;' >> %s", cstrbb, user_config_file);
        else
            vsystem ("sed -i s/'bar_bg_color #......'/'bar_bg_color %s'/g %s", cstrbb, user_config_file);

        if (vsystem ("grep -q bar_fg_color %s\n", user_config_file))
            vsystem ("echo '@define-color bar_fg_color %s;' >> %s", cstrbf, user_config_file);
        else
            vsystem ("sed -i s/'bar_fg_color #......'/'bar_fg_color %s'/g %s", cstrbf, user_config_file);

        g_free (cstrf);
        g_free (cstrb);
        g_free (cstrbf);
        g_free (cstrbb);

        // check if the scrollbar button entry is in the file - if not, add it...
        repl = g_strdup_printf ("min-width: %dpx;", cur_conf.scrollbar_width);
        add_or_amend (user_config_file, "scrollbar button", "min-width:\\s*[0-9]*px;", repl);
        g_free (repl);

        repl = g_strdup_printf ("min-height: %dpx;", cur_conf.scrollbar_width);
        add_or_amend (user_config_file, "scrollbar button", "min-height:\\s*[0-9]*px;", repl);
        g_free (repl);

        // check if the scrollbar slider entry is in the file - if not, add it...
        repl = g_strdup_printf ("min-width: %dpx;", cur_conf.scrollbar_width - 6);
        add_or_amend (user_config_file, "scrollbar slider", "min-width:\\s*[0-9]*px;", repl);
        g_free (repl);

        repl = g_strdup_printf ("min-height: %dpx;", cur_conf.scrollbar_width - 6);
        add_or_amend (user_config_file, "scrollbar slider", "min-height:\\s*[0-9]*px;", repl);
        g_free (repl);

        g_free (user_config_file);
    }

    // GTK2 override file
    user_config_file = g_build_filename (g_get_home_dir (), ".gtkrc-2.0", NULL);

    // check if the scrollbar button entry is in the file - if not, add it...
    repl = g_strdup_printf ("GtkRange::slider-width = %d", cur_conf.scrollbar_width);
    add_or_amend (user_config_file, "style \"scrollbar\"", "GtkRange::slider-width\\s*=\\s*[0-9]*", repl);
    g_free (repl);

    repl = g_strdup_printf ("GtkRange::stepper-size = %d", cur_conf.scrollbar_width);
    add_or_amend (user_config_file, "style \"scrollbar\"", "GtkRange::stepper-size\\s*=\\s*[0-9]*", repl);
    g_free (repl);

    g_free (user_config_file);
}

static void save_lxsession_settings (void)
{
    char *user_config_file, *str, *ostr, *ctheme, *cthemet, *cbar, *cbart;
    GKeyFile *kf;
    gsize len;
    GError *err;

    user_config_file = lxsession_file (FALSE);
    check_directory (user_config_file);

    // read in data from file to a key file
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_string (kf, "GTK", "sNet/ThemeName", TEMP_THEME);

    // update changed values in the key file
    ctheme = rgba_to_gdk_color_string (&cur_conf.theme_colour[cur_conf.darkmode]);
    cthemet = rgba_to_gdk_color_string (&cur_conf.themetext_colour[cur_conf.darkmode]);
    cbar = rgba_to_gdk_color_string (&cur_conf.bar_colour[cur_conf.darkmode]);
    cbart = rgba_to_gdk_color_string (&cur_conf.bartext_colour[cur_conf.darkmode]);

    str = g_strdup_printf ("selected_bg_color:%s\nselected_fg_color:%s\nbar_bg_color:%s\nbar_fg_color:%s\n",
        ctheme, cthemet, cbar, cbart);
    g_key_file_set_string (kf, "GTK", "sGtk/ColorScheme", str);
    g_free (ctheme);
    g_free (cthemet);
    g_free (cbar);
    g_free (cbart);
    g_free (str);

    g_key_file_set_string (kf, "GTK", "sGtk/FontName", cur_conf.desktop_font);
    int tbi = GTK_ICON_SIZE_LARGE_TOOLBAR;
    if (cur_conf.tb_icon_size == 16) tbi = GTK_ICON_SIZE_SMALL_TOOLBAR;
    if (cur_conf.tb_icon_size == 48) tbi = GTK_ICON_SIZE_DIALOG;
    g_key_file_set_integer (kf, "GTK", "iGtk/ToolbarIconSize", tbi);

    err = NULL;
    str = g_key_file_get_string (kf, "GTK", "sGtk/IconSizes", &err);
    if (err == NULL && str)
    {
        if (strstr (str, "gtk-large-toolbar"))
        {
            gchar **str_arr = g_strsplit (str, ":", -1);
            int index = 0;
            while (str_arr[index])
            {
                if (strstr (str_arr[index], "gtk-large-toolbar"))
                {
                    g_free (str_arr[index]);
                    str_arr[index] = g_strdup_printf ("gtk-large-toolbar=%d,%d", cur_conf.tb_icon_size, cur_conf.tb_icon_size);
                }
                index++;
            }
            ostr = g_strjoinv (":", str_arr);
            g_strfreev (str_arr);
        }
        else
        {
            // append this element to existing string
            ostr = g_strdup_printf ("%s:gtk-large-toolbar=%d,%d", str, cur_conf.tb_icon_size, cur_conf.tb_icon_size);
        }
    }
    else
    {
        // new string with just this element
        ostr = g_strdup_printf ("gtk-large-toolbar=%d,%d", cur_conf.tb_icon_size, cur_conf.tb_icon_size);
    }
    g_key_file_set_string (kf, "GTK", "sGtk/IconSizes", ostr);
    g_free (ostr);
    g_free (str);

    g_key_file_set_integer (kf, "GTK", "iGtk/CursorThemeSize", cur_conf.cursor_size);

    // write the modified key file out
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);

    if (wm != WM_OPENBOX)
    {
        vsystem ("gsettings set org.gnome.desktop.interface font-name \"%s\"", cur_conf.desktop_font);
        vsystem ("gsettings set org.gnome.desktop.interface cursor-size %d", cur_conf.cursor_size);
        switch (cur_conf.tb_icon_size)
        {
            case 16:    vsystem ("gsettings set org.gnome.desktop.interface toolbar-icons-size small");
                        break;
            case 48:    vsystem ("gsettings set org.gnome.desktop.interface toolbar-icons-size large");
                        break;
            default:    vsystem ("gsettings set org.gnome.desktop.interface toolbar-icons-size medium");
                        break;
        }
        set_theme (TEMP_THEME);
    }
}

static void save_xsettings (void)
{
    char *user_config_file, *str, *ctheme, *cthemet, *cbar, *cbart;

    user_config_file = xsettings_file (FALSE);
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        // need a local copy to take the changes
        check_directory (user_config_file);
        vsystem ("cp /etc/xsettingsd/xsettingsd.conf %s", user_config_file);
    }

    ctheme = rgba_to_gdk_color_string (&cur_conf.theme_colour[cur_conf.darkmode]);
    cthemet = rgba_to_gdk_color_string (&cur_conf.themetext_colour[cur_conf.darkmode]);
    cbar = rgba_to_gdk_color_string (&cur_conf.bar_colour[cur_conf.darkmode]);
    cbart = rgba_to_gdk_color_string (&cur_conf.bartext_colour[cur_conf.darkmode]);

    str = g_strdup_printf ("selected_bg_color:%s\\\\nselected_fg_color:%s\\\\nbar_bg_color:%s\\\\nbar_fg_color:%s\\\\n",
        ctheme, cthemet, cbar, cbart);

    int tbi = GTK_ICON_SIZE_LARGE_TOOLBAR;
    if (cur_conf.tb_icon_size == 16) tbi = GTK_ICON_SIZE_SMALL_TOOLBAR;
    if (cur_conf.tb_icon_size == 48) tbi = GTK_ICON_SIZE_DIALOG;

    // use sed to write
    vsystem ("sed -i s/'ColorScheme.*'/'ColorScheme \"%s\"'/g %s", str, user_config_file);
    vsystem ("sed -i s/'FontName.*'/'FontName \"%s\"'/g %s", cur_conf.desktop_font, user_config_file);
    vsystem ("sed -i s/'ToolbarIconSize.*'/'ToolbarIconSize %d'/g %s", tbi, user_config_file);
    vsystem ("sed -i s/'CursorThemeSize.*'/'CursorThemeSize %d'/g %s", cur_conf.cursor_size, user_config_file);
    vsystem ("sed -i s/gtk-large-toolbar=[0-9]+,[0-9]+/gtk-large-toolbar=%d,%d/g %s", cur_conf.tb_icon_size, cur_conf.tb_icon_size, user_config_file);

    g_free (ctheme);
    g_free (cthemet);
    g_free (cbar);
    g_free (cbart);
    g_free (str);
    g_free (user_config_file);
}

static void save_pcman_settings (int desktop)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = pcmanfm_file (FALSE, desktop, TRUE);
    check_directory (user_config_file);

    // process pcmanfm config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    str = rgba_to_gdk_color_string (&cur_conf.desktops[desktop].desktop_colour);
    g_key_file_set_string (kf, "*", "desktop_bg", str);
    g_key_file_set_string (kf, "*", "desktop_shadow", str);
    g_free (str);

    str = rgba_to_gdk_color_string (&cur_conf.desktops[desktop].desktoptext_colour);
    g_key_file_set_string (kf, "*", "desktop_fg", str);
    g_free (str);

    g_key_file_set_string (kf, "*", "desktop_font", cur_conf.desktop_font);
    g_key_file_set_string (kf, "*", "wallpaper", cur_conf.desktops[desktop].desktop_picture);
    g_key_file_set_string (kf, "*", "wallpaper_mode", cur_conf.desktops[desktop].desktop_mode);
    g_key_file_set_integer (kf, "*", "show_documents", cur_conf.desktops[desktop].show_docs);
    g_key_file_set_integer (kf, "*", "show_trash", cur_conf.desktops[desktop].show_trash);
    g_key_file_set_integer (kf, "*", "show_mounts", cur_conf.desktops[desktop].show_mnts);
    g_key_file_set_string (kf, "*", "folder", cur_conf.desktops[desktop].desktop_folder);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_pcman_g_settings (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = pcmanfm_g_file (FALSE);
    check_directory (user_config_file);

    // process pcmanfm config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_integer (kf, "ui", "common_bg", cur_conf.common_bg);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_libfm_settings (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    // process libfm config data
    user_config_file = libfm_file ();
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        check_directory (user_config_file);
        vsystem ("cp /etc/xdg/libfm/libfm.conf %s", user_config_file);
    }

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_integer (kf, "ui", "big_icon_size", cur_conf.folder_size);
    g_key_file_set_integer (kf, "ui", "thumbnail_size", cur_conf.thumb_size);
    g_key_file_set_integer (kf, "ui", "pane_icon_size", cur_conf.pane_size);
    g_key_file_set_integer (kf, "ui", "small_icon_size", cur_conf.sicon_size);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_wfpanel_settings (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = wfpanel_file ();
    check_directory (user_config_file);

    // process wfpanel config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_string (kf, "panel", "position", cur_conf.barpos ? "bottom" : "top");
    g_key_file_set_integer (kf, "panel", "icon_size", cur_conf.icon_size - 4);
    g_key_file_set_integer (kf, "panel", "window-list_max_width", cur_conf.task_width);

    char *buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), cur_conf.monitor);
    g_key_file_set_string (kf, "panel", "monitor", buf);
    g_free (buf);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_wayfire_settings (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = wayfire_file ();
    check_directory (user_config_file);

    // process wayfire config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_integer (kf, "input", "cursor_size", cur_conf.cursor_size);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_lxterm_settings (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxterminal/lxterminal.conf", NULL);
    check_directory (user_config_file);

    // read in data from file to a key file
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    // update changed values in the key file
    g_key_file_set_string (kf, "general", "fontname", cur_conf.terminal_font);

    // write the modified key file out
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_obconf_settings (gboolean lw)
{
    char *user_config_file, *font, *cptr;
    int count, size;
    const gchar *weight = NULL, *style = NULL;
    char buf[10];

    xmlDocPtr xDoc;
    xmlNodePtr root, cur_node, node;
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;

    if (lw) user_config_file = labwc_file ();
    else user_config_file = openbox_file ();
    check_directory (user_config_file);

    // set the font description variables for XML from the font name
    PangoFontDescription *pfd = pango_font_description_from_string (cur_conf.desktop_font);
    size = pango_font_description_get_size (pfd) / (pango_font_description_get_size_is_absolute (pfd) ? 1 : PANGO_SCALE);
    PangoWeight pweight = pango_font_description_get_weight (pfd);
    PangoStyle pstyle = pango_font_description_get_style (pfd);

    if (pweight == PANGO_WEIGHT_BOLD)
    {
        weight = "Bold";
        pango_font_description_unset_fields (pfd, PANGO_FONT_MASK_WEIGHT);
    }
    else weight = "Normal";

    if (pstyle == PANGO_STYLE_ITALIC)
    {
        style = "Italic";
        pango_font_description_unset_fields (pfd, PANGO_FONT_MASK_STYLE);
    }
    else style = "Normal";

    // by this point, Bold and Italic flags will be missing from the font description, so just remove the size...
    font = g_strdup (pango_font_description_to_string (pfd));
    cptr = font + strlen (font) - 1;
    while (*cptr >= '0' && *cptr <= '9') cptr--;
    *cptr = 0;

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    if (g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        xDoc = xmlParseFile (user_config_file);
        if (!xDoc) xDoc = xmlNewDoc ((xmlChar *) "1.0");
    }
    else xDoc = xmlNewDoc ((xmlChar *) "1.0");
    xpathCtx = xmlXPathNewContext (xDoc);

    // check that the config and theme nodes exist in the document - create them if not
    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        root = xmlNewNode (NULL, XC ("openbox_config"));
        xmlDocSetRootElement (xDoc, root);
        xmlNewNs (root, XC ("http://openbox.org/3.4/rc"), NULL);
        xmlXPathRegisterNs (xpathCtx, XC ("openbox_config"), XC ("http://openbox.org/3.4/rc"));
    }
    else root = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval)) xmlNewChild (root, NULL, XC ("theme"), NULL);
    xmlXPathFreeObject (xpathObj);

    // update relevant nodes with new values
    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='font']"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']"), xpathCtx);
        for (count = 0; count < 2; count ++)
        {
            cur_node = xmlNewChild (xpathObj->nodesetval->nodeTab[0], NULL, XC ("font"), NULL);

            xmlSetProp (cur_node, XC ("place"), count == 0 ? XC ("ActiveWindow") : XC ("InactiveWindow"));
            sprintf (buf, "%d", size);
            xmlNewChild (cur_node, NULL, XC ("name"), XC (font));
            xmlNewChild (cur_node, NULL, XC ("size"), XC (buf));
            xmlNewChild (cur_node, NULL, XC ("weight"), XC (weight));
            xmlNewChild (cur_node, NULL, XC ("slant"), XC (style));
        }
    }
    else
    {
        for (count = 0; count < xpathObj->nodesetval->nodeNr; count++)
        {
            node = xpathObj->nodesetval->nodeTab[count];
            cur_node = NULL;
            for (cur_node = node->children; cur_node; cur_node = cur_node->next)
            {
                if (cur_node->type == XML_ELEMENT_NODE)
                {
                    sprintf (buf, "%d", size);
                    if (!xmlStrcmp (cur_node->name, XC ("name"))) xmlNodeSetContent (cur_node, XC (font));
                    if (!xmlStrcmp (cur_node->name, XC ("size"))) xmlNodeSetContent (cur_node, XC (buf));
                    if (!xmlStrcmp (cur_node->name, XC ("weight"))) xmlNodeSetContent (cur_node, XC (weight));
                    if (!xmlStrcmp (cur_node->name, XC ("slant")))  xmlNodeSetContent (cur_node, XC (style));
                }
            }
        }
    }
    xmlXPathFreeObject (xpathObj);

    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='name']"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']"), xpathCtx);
        cur_node = xpathObj->nodesetval->nodeTab[0];
        if (cur_conf.scrollbar_width >= 17)
            xmlNewChild (cur_node, NULL, XC ("name"), cur_conf.darkmode ? XC (DEFAULT_THEME_DARK_L) : XC (DEFAULT_THEME_L));
        else
            xmlNewChild (cur_node, NULL, XC ("name"), cur_conf.darkmode ? XC (DEFAULT_THEME_DARK) : XC (DEFAULT_THEME));
    }
    else
    {
        cur_node = xpathObj->nodesetval->nodeTab[0];
        if (cur_conf.scrollbar_width >= 17)
            xmlNodeSetContent (cur_node, cur_conf.darkmode ? XC (DEFAULT_THEME_DARK_L) : XC (DEFAULT_THEME_L));
        else
            xmlNodeSetContent (cur_node, cur_conf.darkmode ? XC (DEFAULT_THEME_DARK) : XC (DEFAULT_THEME));
    }

    if (!lw)
    {
        sprintf (buf, "%d", cur_conf.handle_width);
        xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='invHandleWidth']"), xpathCtx);
        if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        {
            xmlXPathFreeObject (xpathObj);
            xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']"), xpathCtx);
            cur_node = xpathObj->nodesetval->nodeTab[0];
            xmlNewChild (cur_node, NULL, XC ("invHandleWidth"), XC (buf));
        }
        else
        {
            cur_node = xpathObj->nodesetval->nodeTab[0];
            xmlNodeSetContent (cur_node, XC (buf));
        }
    }

    if (!lw)
    {
        cptr = rgba_to_gdk_color_string (&cur_conf.theme_colour[cur_conf.darkmode]);
        xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titleColor']"), xpathCtx);
        if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        {
            xmlXPathFreeObject (xpathObj);
            xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']"), xpathCtx);
            cur_node = xpathObj->nodesetval->nodeTab[0];
            xmlNewChild (cur_node, NULL, XC ("titleColor"), XC (cptr));
        }
        else
        {
            cur_node = xpathObj->nodesetval->nodeTab[0];
            xmlNodeSetContent (cur_node, XC (cptr));
        }
        g_free (cptr);

        cptr = rgba_to_gdk_color_string (&cur_conf.themetext_colour[cur_conf.darkmode]);
        xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='textColor']"), xpathCtx);
        if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        {
            xmlXPathFreeObject (xpathObj);
            xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']"), xpathCtx);
            cur_node = xpathObj->nodesetval->nodeTab[0];
            xmlNewChild (cur_node, NULL, XC ("textColor"), XC (cptr));
        }
        else
        {
            cur_node = xpathObj->nodesetval->nodeTab[0];
            xmlNodeSetContent (cur_node, XC (cptr));
        }
        g_free (cptr);
    }

    // cleanup XML
    xmlXPathFreeObject (xpathObj);
    xmlXPathFreeContext (xpathCtx);
    xmlSaveFile (user_config_file, xDoc);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    pango_font_description_free (pfd);
    g_free (font);
    g_free (user_config_file);
}

static void save_labwc_to_settings (void)
{
    char *user_config_file, *cstrb, *cstrf;

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "labwc", "themerc-override", NULL);
    check_directory (user_config_file);

    cstrb = rgba_to_gdk_color_string (&cur_conf.theme_colour[cur_conf.darkmode]);
    cstrf = rgba_to_gdk_color_string (&cur_conf.themetext_colour[cur_conf.darkmode]);

    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("echo 'window.active.title.bg.color: %s' >> %s", cstrb, user_config_file);
        vsystem ("echo 'window.active.label.text.color: %s' >> %s", cstrf, user_config_file);

        g_free (cstrf);
        g_free (cstrb);
        g_free (user_config_file);
        return;
    }

    // amend entries already in file, or add if not present
    if (vsystem ("grep -q window.active.title.bg.color %s\n", user_config_file))
        vsystem ("echo 'window.active.title.bg.color: %s' >> %s", cstrb, user_config_file);
    else
        vsystem ("sed -i s/'window.active.title.bg.color.*'/'window.active.title.bg.color: %s'/g %s", cstrb, user_config_file);

    if (vsystem ("grep -q window.active.label.text.color %s\n", user_config_file))
        vsystem ("echo 'window.active.label.text.color: %s' >> %s", cstrf, user_config_file);
    else
        vsystem ("sed -i s/'window.active.label.text.color.*'/'window.active.label.text.color: %s'/g %s", cstrf, user_config_file);

    g_free (cstrf);
    g_free (cstrb);
    g_free (user_config_file);
}

static void save_labwc_env_settings (void)
{
    char *user_config_file;

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "labwc", "environment", NULL);
    check_directory (user_config_file);

    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("echo 'XCURSOR_SIZE=%d' >> %s", cur_conf.cursor_size, user_config_file);

        g_free (user_config_file);
        return;
    }

    // amend entries already in file, or add if not present
    if (vsystem ("grep -q XCURSOR_SIZE %s\n", user_config_file))
        vsystem ("echo 'XCURSOR_SIZE=%d' >> %s", cur_conf.cursor_size, user_config_file);
    else
        vsystem ("sed -i s/'XCURSOR_SIZE.*'/'XCURSOR_SIZE=%d'/g %s", cur_conf.cursor_size, user_config_file);

    g_free (user_config_file);
}

static void save_libreoffice_settings (void)
{
    char *user_config_file;
    char buf[2];
    gboolean found = FALSE;

    xmlDocPtr xDoc;
    xmlNodePtr rootnode, itemnode, propnode, valnode;
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;

    sprintf (buf, "%d", cur_conf.lo_icon_size);

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "libreoffice/4/user/registrymodifications.xcu", NULL);
    check_directory (user_config_file);

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    if (g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        xDoc = xmlParseFile (user_config_file);
        if (!xDoc) xDoc = xmlNewDoc ((xmlChar *) "1.0");
    }
    else xDoc = xmlNewDoc ((xmlChar *) "1.0");
    xpathCtx = xmlXPathNewContext (xDoc);

    // check that the oor:items node exists in the document - create it if not
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[name()='oor:items']", xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        rootnode = xmlNewNode (NULL, (xmlChar *) XC ("oor:items"));
        xmlSetProp (rootnode, XC ("xmlns:oor"), XC ("http://openoffice.org/2001/registry"));
        xmlSetProp (rootnode, XC ("xmlns:xs"), XC ("http://www.w3.org/2001/XMLSchema"));
        xmlSetProp (rootnode, XC ("xmlns:xsi"), XC ("http://www.w3.org/2001/XMLSchema-instance"));
        xmlDocSetRootElement (xDoc, rootnode);
    }
    else rootnode = xpathObj->nodesetval->nodeTab[0];

    for (itemnode = rootnode->children; itemnode; itemnode = itemnode->next)
    {
        if (itemnode->type == XML_ELEMENT_NODE && !xmlStrcmp (itemnode->name, XC ("item")) && !xmlStrcmp (xmlGetProp (itemnode, XC ("path")), XC ("/org.openoffice.Office.Common/Misc")))
        {
            xmlNode *propnode = itemnode->children;
            if (propnode->type == XML_ELEMENT_NODE && !xmlStrcmp (propnode->name, XC ("prop")) && !xmlStrcmp (xmlGetProp (propnode, XC ("name")), XC ("SymbolSet")))
            {
                xmlNode *valnode = propnode->children;
                if (valnode->type == XML_ELEMENT_NODE && !xmlStrcmp (valnode->name, XC ("value")))
                    xmlNodeSetContent (valnode, XC (buf));
                found = TRUE;
                break;
            }
        }
    }

    // if node not found, add it with desired value
    if (!found)
    {
        itemnode = xmlNewNode (NULL, XC ("item"));
        xmlSetProp (itemnode, XC ("oor:path"), XC ("/org.openoffice.Office.Common/Misc"));
        propnode = xmlNewNode (NULL, XC ("prop"));
        xmlSetProp (propnode, XC ("oor:name"), XC ("SymbolSet"));
        xmlSetProp (propnode, XC ("oor:op"), XC ("fuse"));
        xmlAddChild (itemnode, propnode);
        valnode = xmlNewNode (NULL, XC ("value"));
        xmlNodeSetContent (valnode, XC (buf));
        xmlAddChild (propnode, valnode);
        xmlAddChild (rootnode, itemnode);
    }

    // cleanup XML
    xmlXPathFreeObject (xpathObj);
    xmlXPathFreeContext (xpathCtx);
    xmlSaveFile (user_config_file, xDoc);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_config_file);
}

static void save_qt_settings (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;
    char buffer[100], tbuf[400], *c;
    const char *font;
    int size, weight, style, nlen, count, index, oind;
    double sval;

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "qt5ct/qt5ct.conf", NULL);
    check_directory (user_config_file);

    // read in data from file to a key file
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    // create the Qt font representation
    PangoFontDescription *pfd = pango_font_description_from_string (cur_conf.desktop_font);
    font = pango_font_description_get_family (pfd);
    size = pango_font_description_get_size (pfd) / (pango_font_description_get_size_is_absolute (pfd) ? 1 : PANGO_SCALE);
    PangoWeight pweight = pango_font_description_get_weight (pfd);
    PangoStyle pstyle = pango_font_description_get_style (pfd);

    switch (pweight)
    {
        case PANGO_WEIGHT_THIN :        weight = 0;
                                        break;
        case PANGO_WEIGHT_ULTRALIGHT :  weight = 12;
                                        break;
        case PANGO_WEIGHT_LIGHT :       weight = 25;
                                        break;
        case PANGO_WEIGHT_MEDIUM :      weight = 57;
                                        break;
        case PANGO_WEIGHT_SEMIBOLD :    weight = 63;
                                        break;
        case PANGO_WEIGHT_BOLD :        weight = 75;
                                        break;
        case PANGO_WEIGHT_ULTRABOLD :   weight = 81;
                                        break;
        case PANGO_WEIGHT_HEAVY :
        case PANGO_WEIGHT_ULTRAHEAVY :  weight = 87;
                                        break;
        default :                       weight = 50;
                                        break;
    }

    switch (pstyle)
    {
        case PANGO_STYLE_ITALIC :   style = 1;
                                    break;
        case PANGO_STYLE_OBLIQUE :  style = 2;
                                    break;
        default :                   style = 0;
                                    break;
    }

    memset (buffer, 0, sizeof (buffer));

    // header
    buffer[3] = '@';

    // font family
    nlen = strlen (font);
    buffer[7] = nlen * 2;
    index = 8;
    for (count = 0; count < nlen; count++)
    {
        buffer[index++] = 0;
        buffer[index++] = font[count];
    }

    // font size - need to reverse bytes :(
    sval = size;
    c = ((char *) &sval) + 7;
    for (count = 0; count < sizeof (double); count++)
        buffer[index++] = *c--;

    // padding
    sprintf (buffer + index, "\xff\xff\xff\xff\x5\x1");
    index += 7;

    // weight and style
    buffer[index++] = weight;
    buffer[index++] = style + 16;

    // convert to text
    sprintf (tbuf, "\"@Variant(");
    oind = 10;
    for (count = 0; count < index; count++)
    {
        switch (buffer[count])
        {
            case 0 :    tbuf[oind++] = '\\';
                        tbuf[oind++] = '0';
                        break;

            case 0x22 :
            case 0x27 :
            case 0x3F :
            case 0x30 :
            case 0x31 :
            case 0x32 :
            case 0x33 :
            case 0x34 :
            case 0x35 :
            case 0x36 :
            case 0x37 :
            case 0x38 :
            case 0x39 :
            case 0x41 :
            case 0x42 :
            case 0x43 :
            case 0x44 :
            case 0x45 :
            case 0x46 :
            case 0x5C :
            case 0x61 :
            case 0x62 :
            case 0x63 :
            case 0x64 :
            case 0x65 :
            case 0x66 :
                        tbuf[oind++] = '\\';
                        tbuf[oind++] = 'x';
                        sprintf (tbuf + oind, "%02x", buffer[count]);
                        oind += 2;
                        break;

            default :   if (buffer[count] >= 32 && buffer[count] <= 126)
                            tbuf[oind++] = buffer[count];
                        else
                        {
                            tbuf[oind++] = '\\';
                            tbuf[oind++] = 'x';
                            sprintf (tbuf + oind, buffer[count] > 9 ? "%02x" : "%x", buffer[count]);
                            oind += 1 + (buffer[count] > 9);
                        }
                        break;
        }
    }
    tbuf[oind++] = ')';
    tbuf[oind++] = '"';
    tbuf[oind] = 0;

    // update changed values in the key file
    g_key_file_set_value (kf, "Fonts", "fixed", tbuf);
    g_key_file_set_value (kf, "Fonts", "general", tbuf);

    // write the modified key file out
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    pango_font_description_free (pfd);
    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_app_settings (void)
{
    char *config_file;

    // geany colour theme
    config_file = g_build_filename (g_get_user_config_dir (), "geany/geany.conf", NULL);
    set_config_param (config_file, "geany", "color_scheme", cur_conf.darkmode ? "pixnoir.conf" : "");
    g_free (config_file);

    // galculator display colours
    config_file = g_build_filename (g_get_user_config_dir (), "galculator/galculator.conf", NULL);
    set_config_param (config_file, "general", "display_bkg_color", cur_conf.darkmode ? "rgb(94,92,100)" : "#ffffff");
    set_config_param (config_file, "general", "display_result_color", cur_conf.darkmode ? "rgb(246,245,244)" : "black");
    set_config_param (config_file, "general", "display_stack_color", cur_conf.darkmode ? "rgb(246,245,244)" : "black");
    g_free (config_file);
}

static void add_or_amend (const char *conffile, const char *block, const char *param, const char *repl)
{
    // grep - use tr to convert file to single line then search for -
    // start of first line of block - block
    // followed by any whitespace and a { - \s*{
    // followed by any number (including zero) of non { characters - [^{]*
    // followed by the parameter string - param
    // followed by any number (including zero) of non } characters - [^}]*
    // followed by a } to close the block - }

    // sed - use a range to restrict changes - syntax is '/range_start/,/range_end/ s/find_string/replace_string/'
    // range_start is block start string
    // range_end is }
    // in add case, replace } with tab, replace string, newline and replacement }

    // process block string to add grep whitespace characters
    gchar **tokens = g_strsplit (block, " ", 0);
    gchar *block_ws = g_strjoinv ("\\s*", tokens);
    g_strfreev (tokens);

    // check the file and block exist - add an empty block if not
    if (!g_file_test (conffile, G_FILE_TEST_IS_REGULAR) || vsystem ("cat %s | tr -d '\\n' | grep -q '%s\\s*{.*}'", conffile, block_ws))
    {
        vsystem ("echo '\n%s\n{\n}' >> %s", block, conffile);
    }

    // check if the block contains the entry
    if (vsystem ("cat %s | tr -d '\\n' | grep -q -P '%s\\s*{[^{]*%s[^}]*}'", conffile, block_ws, param))
    {
        // entry does not exist - add it
        vsystem ("sed -i '/%s/,/}/ s/}/\t%s\\n}/' %s", block_ws, repl, conffile);
    }
    else
    {
        // entry exists - amend it
        vsystem ("sed -i '/%s/,/}/ s/%s/%s/' %s", block_ws, param, repl, conffile);
    }
    g_free (block_ws);
}

/* Dialog box "changed" signal handlers */

static void on_menu_size_set (GtkComboBox* btn, gpointer ptr)
{
    gint val = gtk_combo_box_get_active (btn);
    switch (val)
    {
        case 0 :    cur_conf.icon_size = 52;
                    break;
        case 1 :    cur_conf.icon_size = 36;
                    break;
        case 2 :    cur_conf.icon_size = 28;
                    break;
        case 3 :    cur_conf.icon_size = 20;
                    break;
    }
    if (wm != WM_OPENBOX)
    {
        save_wfpanel_settings ();
        reload_pcmanfm ();
    }
    else
    {
        save_lxpanel_settings ();
        reload_lxpanel ();
    }
}

static void on_theme_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.theme_colour[cur_conf.darkmode]);
    set_theme (TEMP_THEME);
    save_lxsession_settings ();
    save_xsettings ();
    save_obconf_settings (FALSE);
    if (wm == WM_LABWC) save_labwc_to_settings ();
    save_gtk3_settings ();
    reload_lxsession ();
    reload_xsettings ();
    reload_openbox ();
    reload_pcmanfm ();
    reload_theme (FALSE);
}

static void on_themetext_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.themetext_colour[cur_conf.darkmode]);
    set_theme (TEMP_THEME);
    save_lxsession_settings ();
    save_xsettings ();
    save_obconf_settings (FALSE);
    if (wm == WM_LABWC) save_labwc_to_settings ();
    save_gtk3_settings ();
    reload_lxsession ();
    reload_xsettings ();
    reload_openbox ();
    reload_pcmanfm ();
    reload_theme (FALSE);
}

static void on_bar_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.bar_colour[cur_conf.darkmode]);
    set_theme (TEMP_THEME);
    save_lxsession_settings ();
    save_xsettings ();
    save_gtk3_settings ();
    reload_lxsession ();
    reload_xsettings ();
    reload_pcmanfm ();
    reload_theme (FALSE);
}

static void on_bartext_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.bartext_colour[cur_conf.darkmode]);
    set_theme (TEMP_THEME);
    save_lxsession_settings ();
    save_xsettings ();
    save_gtk3_settings ();
    reload_lxsession ();
    reload_xsettings ();
    reload_pcmanfm ();
    reload_theme (FALSE);
}

static void on_desktop_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.desktops[desktop_n].desktop_colour);
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_desktoptext_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.desktops[desktop_n].desktoptext_colour);
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_desktop_picture_set (GtkFileChooser* btn, gpointer ptr)
{
    char *picture = gtk_file_chooser_get_filename (btn);
    if (picture) cur_conf.desktops[desktop_n].desktop_picture = picture;
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_desktop_font_set (GtkFontChooser* btn, gpointer ptr)
{
    int i;
    PangoFontDescription *font_desc = gtk_font_chooser_get_font_desc (btn);
    const char *font = gtk_font_chooser_get_font (btn);
    if (font)
    {
        cur_conf.desktop_font = font;

        int font_height = pango_font_description_get_size (font_desc);
        if (!pango_font_description_get_size_is_absolute (font_desc))
        {
            font_height *= 4;
            font_height /= 3;
        }
        font_height /= PANGO_SCALE;

        cur_conf.scrollbar_width = font_height >= LARGE_ICON_THRESHOLD ? 17 : 13;
    }

    save_lxsession_settings ();
    save_xsettings ();
    for (i = 0; i < ndesks; i++)
        save_pcman_settings (i);
    save_obconf_settings (FALSE);
    if (wm == WM_LABWC) save_obconf_settings (TRUE);
    save_gtk3_settings ();
    save_qt_settings ();

    reload_lxsession ();
    reload_xsettings ();
    reload_lxpanel ();
    reload_openbox ();
    reload_pcmanfm ();
    reload_theme (FALSE);
}

static void on_desktop_mode_set (GtkComboBox* btn, gpointer ptr)
{
    gint val = gtk_combo_box_get_active (btn);
    switch (val)
    {
        case 0 :    cur_conf.desktops[desktop_n].desktop_mode = "color";
                    break;
        case 1 :    cur_conf.desktops[desktop_n].desktop_mode = "center";
                    break;
        case 2 :    cur_conf.desktops[desktop_n].desktop_mode = "fit";
                    break;
        case 3 :    cur_conf.desktops[desktop_n].desktop_mode = "crop";
                    break;
        case 4 :    cur_conf.desktops[desktop_n].desktop_mode = "stretch";
                    break;
        case 5 :    cur_conf.desktops[desktop_n].desktop_mode = "tile";
                    break;
    }

    if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "color")) gtk_widget_set_sensitive (GTK_WIDGET (dpic), FALSE);
    else gtk_widget_set_sensitive (GTK_WIDGET (dpic), TRUE);
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_desktop_folder_set (GtkFileChooser* btn, gpointer ptr)
{
    char *folder = gtk_file_chooser_get_filename (btn);
    if (folder)
    {
        if (g_strcmp0 (cur_conf.desktops[desktop_n].desktop_folder, folder))
        {
            cur_conf.desktops[desktop_n].desktop_folder = folder;
            save_pcman_settings (desktop_n);
            reload_pcmanfm ();
        }
    }
}

static void on_desktop_changed (GtkComboBox* cb, gpointer ptr)
{
    GtkTreeIter iter;

    gtk_combo_box_get_active_iter (cb, &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &desktop_n, -1);

    set_desktop_controls ();
}

static void on_bar_pos_set (GtkRadioButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) cur_conf.barpos = 0;
    else cur_conf.barpos = 1;
    if (wm != WM_OPENBOX)
    {
        save_wfpanel_settings ();
        reload_pcmanfm ();
    }
    else
    {
        save_lxpanel_settings ();
        reload_lxpanel ();
    }
}

static void on_bar_loc_set (GtkComboBox* cb, gpointer ptr)
{
    GtkTreeIter iter;

    gtk_combo_box_get_active_iter (cb, &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &cur_conf.monitor, -1);
    if (wm != WM_OPENBOX)
    {
        save_wfpanel_settings ();
        reload_pcmanfm ();
    }
    else
    {
        save_lxpanel_settings ();
        reload_lxpanel ();
    }
}

static void on_toggle_docs (GtkCheckButton* btn, gpointer ptr)
{
    cur_conf.desktops[desktop_n].show_docs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_toggle_trash (GtkCheckButton* btn, gpointer ptr)
{
    cur_conf.desktops[desktop_n].show_trash = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_toggle_mnts (GtkCheckButton* btn, gpointer ptr)
{
    cur_conf.desktops[desktop_n].show_mnts = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_darkmode_set (GtkRadioButton* btn, gpointer ptr)
{
    if (!system ("pgrep geany > /dev/null"))
    {
        g_signal_handler_block (rb5, bdid);
        if (cur_conf.darkmode) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb6), TRUE);
        else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb5), TRUE);
        message_ok (_("The theme for Geany cannot be changed while it is open.\nPlease close it and try again."));
        g_signal_handler_unblock (rb5, bdid);
        return;
    }

    if (!system ("pgrep galculator > /dev/null"))
    {
        g_signal_handler_block (rb5, bdid);
        if (cur_conf.darkmode) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb6), TRUE);
        else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb5), TRUE);
        message_ok (_("The theme for Calculator cannot be changed while it is open.\nPlease close it and try again."));
        g_signal_handler_unblock (rb5, bdid);
        return;
    }

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) cur_conf.darkmode = 0;
    else cur_conf.darkmode = 1;
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (hcol), &cur_conf.theme_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (htcol), &cur_conf.themetext_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (bcol), &cur_conf.bar_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (btcol), &cur_conf.bartext_colour[cur_conf.darkmode]);
    save_lxsession_settings ();
    save_xsettings ();
    save_obconf_settings (FALSE);
    if (wm == WM_LABWC)
    {
        save_obconf_settings (TRUE);
        save_labwc_to_settings ();
    }
    save_gtk3_settings ();
    save_app_settings ();
    reload_lxsession ();
    reload_xsettings ();
    reload_openbox ();
    reload_theme (FALSE);
}

static void on_toggle_desktop (GtkCheckButton* btn, gpointer ptr)
{
    int i;

    desktop_n = 0;
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    {
        cur_conf.common_bg = 1;
        g_signal_handler_block (cbdesk, cdid);
        gtk_combo_box_set_active (GTK_COMBO_BOX (cbdesk), -1);
        g_signal_handler_unblock (cbdesk, cdid);
        gtk_widget_set_sensitive (GTK_WIDGET (cbdesk), FALSE);
    }
    else
    {
        cur_conf.common_bg = 0;
        gtk_combo_box_set_active (GTK_COMBO_BOX (cbdesk), 0);
        gtk_widget_set_sensitive (GTK_WIDGET (cbdesk), TRUE);
        for (i = 0; i < ndesks; i++) load_pcman_settings (i);
    }
    set_desktop_controls ();
    save_pcman_g_settings ();
    save_pcman_settings (0);
    reload_pcmanfm ();
}

static void on_cursor_size_set (GtkComboBox* btn, gpointer ptr)
{
    gint val = gtk_combo_box_get_active (btn);
    switch (val)
    {
        case 0 :    cur_conf.cursor_size = 48;
                    break;
        case 1 :    cur_conf.cursor_size = 36;
                    break;
        case 2 :    cur_conf.cursor_size = 24;
                    break;
    }
    save_lxsession_settings ();
    save_xsettings ();
    if (wm == WM_LABWC) save_labwc_env_settings ();
    if (wm == WM_WAYFIRE) save_wayfire_settings ();
    reload_lxsession ();
    reload_xsettings ();
    reload_labwc ();
    reload_theme (FALSE);
}

static void set_desktop_controls (void)
{
    g_signal_handler_block (dmod, dmid);
    g_signal_handler_block (cb1, tdid);
    g_signal_handler_block (cb2, ttid);
    g_signal_handler_block (cb3, tmid);
    g_signal_handler_block (dfold, dfid);

    gtk_widget_set_sensitive (GTK_WIDGET (dpic), TRUE);
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dpic), cur_conf.desktops[desktop_n].desktop_picture);
    if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "center")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 1);
    else if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "fit")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 2);
    else if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "crop")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 3);
    else if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "stretch")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 4);
    else if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "tile")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 5);
    else
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 0);
        gtk_widget_set_sensitive (GTK_WIDGET (dpic), FALSE);
    }
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dcol), &cur_conf.desktops[desktop_n].desktop_colour);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dtcol), &cur_conf.desktops[desktop_n].desktoptext_colour);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb1), cur_conf.desktops[desktop_n].show_docs);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb2), cur_conf.desktops[desktop_n].show_trash);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb3), cur_conf.desktops[desktop_n].show_mnts);
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dfold), cur_conf.desktops[desktop_n].desktop_folder);

    g_signal_handler_unblock (dmod, dmid);
    g_signal_handler_unblock (cb1, tdid);
    g_signal_handler_unblock (cb2, ttid);
    g_signal_handler_unblock (cb3, tmid);
    g_signal_handler_unblock (dfold, dfid);
}

static void set_controls (void)
{
    GtkTreeIter iter;
    int val;

    // block widget handlers
    g_signal_handler_block (isz, iid);
    g_signal_handler_block (csz, cid);
    g_signal_handler_block (rb1, bpid);
    g_signal_handler_block (cbbar, blid);
    g_signal_handler_block (rb5, bdid);
    g_signal_handler_block (cb4, cbid);
    g_signal_handler_block (cbdesk, cdid);

    gtk_font_chooser_set_font (GTK_FONT_CHOOSER (font), cur_conf.desktop_font);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (hcol), &cur_conf.theme_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (htcol), &cur_conf.themetext_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (bcol), &cur_conf.bar_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (btcol), &cur_conf.bartext_colour[cur_conf.darkmode]);

    if (cur_conf.icon_size <= 20) gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 3);
    else if (cur_conf.icon_size <= 28) gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 2);
    else if (cur_conf.icon_size <= 36) gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 1);
    else gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 0);

    if (cur_conf.cursor_size >= 48) gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 0);
    else if (cur_conf.cursor_size >= 36) gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 1);
    else gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 2);

    if (cur_conf.barpos) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb2), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb1), TRUE);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb4), cur_conf.common_bg);
    if (ndesks > 1)
    {
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sortmons), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &val, -1);
        while (val != cur_conf.monitor)
        {
            gtk_tree_model_iter_next (GTK_TREE_MODEL (sortmons), &iter);
            gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &val, -1);
        }
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cbbar), &iter);

        if (cur_conf.common_bg)
        {
            desktop_n = 0;
            gtk_combo_box_set_active (GTK_COMBO_BOX (cbdesk), -1);
            gtk_widget_set_sensitive (GTK_WIDGET (cbdesk), FALSE);
        }
        else
        {
            gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sortmons), &iter);
            gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &val, -1);
            while (val != desktop_n)
            {
                gtk_tree_model_iter_next (GTK_TREE_MODEL (sortmons), &iter);
                gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &val, -1);
            }
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cbdesk), &iter);
            gtk_widget_set_sensitive (GTK_WIDGET (cbdesk), TRUE);
        }
    }

    if (cur_conf.darkmode) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb6), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb5), TRUE);

    // unblock widget handlers
    g_signal_handler_unblock (isz, iid);
    g_signal_handler_unblock (csz, cid);
    g_signal_handler_unblock (rb1, bpid);
    g_signal_handler_unblock (cbbar, blid);
    g_signal_handler_unblock (rb5, bdid);
    g_signal_handler_unblock (cb4, cbid);
    g_signal_handler_unblock (cbdesk, cdid);

    set_desktop_controls ();
}

static void on_set_defaults (GtkButton* btn, gpointer ptr)
{
    int i;
    if (cur_conf.darkmode == 1)
    {
        if (!system ("pgrep geany > /dev/null"))
        {
            message_ok (_("The theme for Geany cannot be changed while it is open.\nPlease close it and try again."));
            return;
        }

        if (!system ("pgrep galculator > /dev/null"))
        {
            message_ok (_("The theme for Calculator cannot be changed while it is open.\nPlease close it and try again."));
            return;
        }
    }

    // clear all the config files
    reset_to_defaults ();

    // set config structure to a default
    switch ((long int) ptr)
    {
        case 3 :    cur_conf = def_lg;
                    break;
        case 1 :    cur_conf = def_sm;
                    break;
        default :   cur_conf = def_med;
    }

    // reset the GUI controls to match the variables
    set_controls ();

    // save changes to files if not using medium (the global default)
    if ((long int) ptr != 2)
    {
        save_lxsession_settings ();
        save_xsettings ();
        save_pcman_g_settings ();
        for (i = 0; i < ndesks; i++)
            save_pcman_settings (i);
        save_libfm_settings ();
        save_obconf_settings (FALSE);
        if (wm == WM_LABWC) save_labwc_to_settings ();
        save_gtk3_settings ();
        if (wm == WM_OPENBOX) save_lxpanel_settings ();
        save_qt_settings ();
    }

    if (wm != WM_OPENBOX) save_wfpanel_settings ();
    if (wm == WM_LABWC)
    {
        save_obconf_settings (TRUE);
        save_labwc_env_settings ();
    }
    if (wm == WM_WAYFIRE) save_wayfire_settings ();

    // save application-specific config - we don't delete these files first...
    save_lxterm_settings ();
    save_libreoffice_settings ();
    save_app_settings ();

    // reload everything to reflect the current state
    reload_lxsession ();
    reload_xsettings ();
    reload_lxpanel ();
    reload_openbox ();
    reload_pcmanfm ();
    reload_theme (FALSE);
}

static void defaults_lxpanel (void)
{
    char *user_config_file, *cmdbuf, *res;
    int val;

    user_config_file = lxpanel_file (TRUE);
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        def_med.barpos = 0;
        def_med.icon_size = 36;
        def_med.monitor = 0;
        g_free (user_config_file);
        return;
    }

    if (!vsystem ("grep -q edge=bottom %s", user_config_file)) def_med.barpos = 1;
    else def_med.barpos = 0;

    if (!vsystem ("grep -q monitor=1 %s", user_config_file)) def_med.monitor = 1;
    else def_med.monitor = 0;

    cmdbuf = g_strdup_printf ("grep -Po '(?<=iconsize=)[0-9]+' %s", user_config_file);
    res = get_string (cmdbuf);
    if (res[0] && sscanf (res, "%d", &val) == 1) def_med.icon_size = val;
    else def_med.icon_size = 36;
    g_free (res);
    g_free (cmdbuf);

    g_free (user_config_file);
}

static void defaults_lxsession ()
{
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;
    int val;

    // read in data from system default file to a key file structure
    user_config_file = lxsession_file (TRUE);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "GTK", "sGtk/FontName", &err);
        if (err == NULL) def_med.desktop_font = g_strdup (ret);
        else def_med.desktop_font = "";
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "GTK", "iGtk/CursorThemeSize", &err);
        if (err == NULL && val >= 24 && val <= 48) def_med.cursor_size = val;
        else def_med.cursor_size = 0;
    }
    else
    {
        def_med.desktop_font = "";
        def_med.cursor_size = 0;
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void defaults_pcman (int desktop)
{
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from system default file to a key file structure
    user_config_file = pcmanfm_file (TRUE, desktop, FALSE);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_bg", &err);
        if (err == NULL)
        {
            if (!gdk_rgba_parse (&def_med.desktops[desktop].desktop_colour, ret))
                gdk_rgba_parse (&def_med.desktops[desktop].desktop_colour, GREY);
        }
        else gdk_rgba_parse (&def_med.desktops[desktop].desktop_colour, GREY);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_fg", &err);
        if (err == NULL)
        {
            if (!gdk_rgba_parse (&def_med.desktops[desktop].desktoptext_colour, ret))
                gdk_rgba_parse (&def_med.desktops[desktop].desktoptext_colour, GREY);
        }
        else gdk_rgba_parse (&def_med.desktops[desktop].desktoptext_colour, GREY);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper", &err);
        if (err == NULL && ret) def_med.desktops[desktop].desktop_picture = g_strdup (ret);
        else def_med.desktops[desktop].desktop_picture = "";
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper_mode", &err);
        if (err == NULL && ret) def_med.desktops[desktop].desktop_mode = g_strdup (ret);
        else def_med.desktops[desktop].desktop_mode = "color";
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_documents", &err);
        if (err == NULL && val >= 0 && val <= 1) def_med.desktops[desktop].show_docs = val;
        else def_med.desktops[desktop].show_docs = 0;

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_trash", &err);
        if (err == NULL && val >= 0 && val <= 1) def_med.desktops[desktop].show_trash = val;
        else def_med.desktops[desktop].show_trash = 0;

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_mounts", &err);
        if (err == NULL && val >= 0 && val <= 1) def_med.desktops[desktop].show_mnts = val;
        else def_med.desktops[desktop].show_mnts = 0;

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "folder", &err);
        if (err == NULL && ret) def_med.desktops[desktop].desktop_folder = g_strdup (ret);
        else def_med.desktops[desktop].desktop_folder = g_build_filename (g_get_home_dir (), "Desktop", NULL);
        g_free (ret);
    }
    else
    {
        def_med.desktops[desktop].desktop_picture = "";
        def_med.desktops[desktop].desktop_mode = "color";
        gdk_rgba_parse (&def_med.desktops[desktop].desktop_colour, GREY);
        gdk_rgba_parse (&def_med.desktops[desktop].desktoptext_colour, GREY);
        def_med.desktops[desktop].show_docs = 0;
        def_med.desktops[desktop].show_trash = 0;
        def_med.desktops[desktop].show_mnts = 0;
        def_med.desktops[desktop].desktop_folder = g_build_filename (g_get_home_dir (), "Desktop", NULL);
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void defaults_pcman_g (void)
{
    char *user_config_file;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from system default file to a key file structure
    user_config_file = pcmanfm_g_file (TRUE);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "common_bg", &err);
        if (err == NULL && val >= 0 && val <= 1) def_med.common_bg = val;
        else def_med.common_bg = 0;
    }
    else
    {
        def_med.common_bg = 0;
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void defaults_gtk3 (void)
{
    char *sys_config_file, *cmdbuf, *res;
    int dark;

    def_med.darkmode = 0;

    for (dark = 0; dark < 2; dark++)
    {
        sys_config_file = g_build_filename ("/usr/share/themes", dark ? DEFAULT_THEME_DARK : DEFAULT_THEME, "gtk-3.0/!(*-dark).css", NULL);

        cmdbuf = g_strdup_printf ("bash -O extglob -c \"grep -hPo '(?<=@define-color\\stheme_selected_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null\"", sys_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.theme_colour[dark], res)) gdk_rgba_parse (&def_med.theme_colour[dark], GREY);

        cmdbuf = g_strdup_printf ("bash -O extglob -c \"grep -hPo '(?<=@define-color\\stheme_selected_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null\"", sys_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.themetext_colour[dark], res)) gdk_rgba_parse (&def_med.themetext_colour[dark], GREY);

        cmdbuf = g_strdup_printf ("bash -O extglob -c \"grep -hPo '(?<=@define-color\\sbar_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null\"", sys_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.bar_colour[dark], res)) gdk_rgba_parse (&def_med.bar_colour[dark], GREY);

        cmdbuf = g_strdup_printf ("bash -O extglob -c \"grep -hPo '(?<=@define-color\\sbar_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null\"", sys_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.bartext_colour[dark], res)) gdk_rgba_parse (&def_med.bartext_colour[dark], GREY);

        g_free (sys_config_file);
    }
}

static void create_defaults (void)
{
    int i;
    // defaults for controls

    // /etc/xdg/lxpanel/LXDE-pi/panels/panel
    defaults_lxpanel ();

    // /etc/xdg/lxsession/LXDE-pi/desktop.conf
    defaults_lxsession ();

    // /etc/xdg/pcmanfm/LXDE-pi/desktop-items-n.conf
    for (i = 0; i < ndesks; i++)
        defaults_pcman (i);

    // /etc/xdg/pcmanfm/LXDE-pi/pcmanfm.conf
    defaults_pcman_g ();

    // GTK 3 theme defaults
    defaults_gtk3 ();

    // defaults with no dedicated controls - set on defaults buttons only,
    // so the values set in these are only used in the large and small cases
    // medium values provided for reference only...
    def_med.terminal_font = "Monospace 10";
    def_med.folder_size = 48;
    def_med.thumb_size = 128;
    def_med.pane_size = 24;
    def_med.sicon_size = 24;
    def_med.tb_icon_size = 24;
    def_med.lo_icon_size = 1;
    def_med.task_width = 200;
    def_med.handle_width = 10;
    def_med.scrollbar_width = 13;

    def_lg = def_sm = def_med;

    def_lg.desktop_font = "PibotoLt 16";
    def_lg.icon_size = 52;
    def_lg.cursor_size = 36;

    def_lg.terminal_font = "Monospace 15";
    def_lg.folder_size = 80;
    def_lg.thumb_size = 160;
    def_lg.pane_size = 32;
    def_lg.sicon_size = 32;
    def_lg.tb_icon_size = 48;
    def_lg.lo_icon_size = (lo_ver >= 6 ? 3 : 1);
    def_lg.task_width = 300;
    def_lg.handle_width = 20;
    def_lg.scrollbar_width = 17;

    def_sm.desktop_font = "PibotoLt 8";
    def_sm.icon_size = 20;
    def_sm.cursor_size = 24;

    def_sm.terminal_font = "Monospace 8";
    def_sm.folder_size = 32;
    def_sm.thumb_size = 64;
    def_sm.pane_size = 16;
    def_sm.sicon_size = 16;
    def_sm.tb_icon_size = 16;
    def_sm.lo_icon_size = 0;
    def_sm.task_width = 150;
    def_sm.handle_width = 10;
    def_sm.scrollbar_width = 13;
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

/* The dialog... */

static gboolean init_config (gpointer data)
{
    GObject *item;
    GtkWidget *wid;
    GtkLabel *lbl;
    GList *children, *child;
    int maj, min, sub, i;
    gboolean has_dark;
    char *buf;

    ndesks = n_desktops ();
    if (ndesks > MAX_DESKTOPS) ndesks = MAX_DESKTOPS;
    if (wm == WM_OPENBOX && ndesks > MAX_X_DESKTOPS) ndesks = MAX_X_DESKTOPS;
    desktop_n = 0;

    // check to see if lxsession will auto-refresh - version 0.4.9 or later
    if (read_version ("lxsession", &maj, &min, &sub))
    {
        if (min >= 5) needs_refresh = 0;
        else if (min == 4 && sub == 9) needs_refresh = 0;
        else needs_refresh = 1;
    }
    else needs_refresh = 1;

    // get libreoffice version
    if (read_version ("libreoffice", &maj, &min, &sub)) lo_ver = maj;
    else lo_ver = 5;

    // is there a dark theme available?
    has_dark = (is_dark () == -1) ? FALSE : TRUE;

    // load data from config files
    create_defaults ();

    load_lxsession_settings ();
    load_pcman_g_settings ();
    for (i = 0; i < ndesks; i++)
        load_pcman_settings (i);
    if (wm != WM_OPENBOX) load_wfpanel_settings ();
    else load_lxpanel_settings ();
    load_obconf_settings ();
    load_gtk3_settings ();

    init_lxsession (cur_conf.darkmode ? DEFAULT_THEME_DARK : DEFAULT_THEME);
    backup_config_files ();

    // build the UI
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/pipanel.ui");

    font = gtk_builder_get_object (builder, "fontbutton1");
    g_signal_connect (font, "font-set", G_CALLBACK (on_desktop_font_set), NULL);

    hcol = gtk_builder_get_object (builder, "colorbutton1");
    g_signal_connect (hcol, "color-set", G_CALLBACK (on_theme_colour_set), NULL);

    bcol = gtk_builder_get_object (builder, "colorbutton3");
    g_signal_connect (bcol, "color-set", G_CALLBACK (on_bar_colour_set), NULL);

    btcol = gtk_builder_get_object (builder, "colorbutton4");
    g_signal_connect (btcol, "color-set", G_CALLBACK (on_bartext_colour_set), NULL);

    htcol = gtk_builder_get_object (builder, "colorbutton5");
    g_signal_connect (htcol, "color-set", G_CALLBACK (on_themetext_colour_set), NULL);

    item = gtk_builder_get_object (builder, "defs_lg");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), (void *) 3);

    item = gtk_builder_get_object (builder, "defs_med");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), (void *) 2);

    item = gtk_builder_get_object (builder, "defs_sml");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), (void *) 1);

    rb1 = gtk_builder_get_object (builder, "radiobutton1");
    rb2 = gtk_builder_get_object (builder, "radiobutton2");
    bpid = g_signal_connect (rb1, "toggled", G_CALLBACK (on_bar_pos_set), NULL);

    cbbar = gtk_builder_get_object (builder, "cb_barmon");
    blid = g_signal_connect (cbbar, "changed", G_CALLBACK (on_bar_loc_set), NULL);

    isz = gtk_builder_get_object (builder, "comboboxtext2");
    iid = g_signal_connect (isz, "changed", G_CALLBACK (on_menu_size_set), NULL);

    csz = gtk_builder_get_object (builder, "comboboxtext3");
    cid = g_signal_connect (csz, "changed", G_CALLBACK (on_cursor_size_set), NULL);

    if (has_dark)
    {
        rb5 = gtk_builder_get_object (builder, "radiobutton5");
        rb6 = gtk_builder_get_object (builder, "radiobutton6");
        bdid = g_signal_connect (rb5, "toggled", G_CALLBACK (on_darkmode_set), NULL);
    }
    else gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox35")));

    dcol = gtk_builder_get_object (builder, "colorbutton2");
    g_signal_connect (dcol, "color-set", G_CALLBACK (on_desktop_colour_set), NULL);

    dtcol = gtk_builder_get_object (builder, "colorbutton6");
    g_signal_connect (dtcol, "color-set", G_CALLBACK (on_desktoptext_colour_set), NULL);

    dpic = gtk_builder_get_object (builder, "filechooserbutton1");
    g_signal_connect (dpic, "file-set", G_CALLBACK (on_desktop_picture_set), NULL);

    // add accessibility label to button child of file chooser
    lbl = GTK_LABEL (gtk_builder_get_object (builder, "label12"));
    children = gtk_container_get_children (GTK_CONTAINER (dpic));
    child = children;
    do
    {
        wid = GTK_WIDGET (child->data);
        if (GTK_IS_BUTTON (wid))
        {
            atk_label (wid, lbl);
            gtk_widget_set_tooltip_text (wid, gtk_widget_get_tooltip_text (GTK_WIDGET (dpic)));
        }
    } while ((child = g_list_next (child)) != NULL);
    g_list_free (children);

    dmod = gtk_builder_get_object (builder, "comboboxtext1");
    dmid = g_signal_connect (dmod, "changed", G_CALLBACK (on_desktop_mode_set), NULL);

    cb1 = gtk_builder_get_object (builder, "checkbutton1");
    tdid = g_signal_connect (cb1, "toggled", G_CALLBACK (on_toggle_docs), NULL);

    cb2 = gtk_builder_get_object (builder, "checkbutton2");
    ttid = g_signal_connect (cb2, "toggled", G_CALLBACK (on_toggle_trash), NULL);

    cb3 = gtk_builder_get_object (builder, "checkbutton3");
    tmid = g_signal_connect (cb3, "toggled", G_CALLBACK (on_toggle_mnts), NULL);

    dfold = gtk_builder_get_object (builder, "filechooserbutton4");
    dfid = g_signal_connect (dfold, "selection-changed", G_CALLBACK (on_desktop_folder_set), NULL);

    cb4 = gtk_builder_get_object (builder, "checkbutton4");
    cbid = g_signal_connect (cb4, "toggled", G_CALLBACK (on_toggle_desktop), NULL);

    cbdesk = gtk_builder_get_object (builder, "cb_desktop");
    cdid = g_signal_connect (cbdesk, "changed", G_CALLBACK (on_desktop_changed), NULL);

    // add accessibility label to combo box child of file chooser (yes, I know the previous one attached to a button...)
    lbl = GTK_LABEL (gtk_builder_get_object (builder, "label16"));
    children = gtk_container_get_children (GTK_CONTAINER (dfold));
    child = children;
    do
    {
        wid = GTK_WIDGET (child->data);
        if (GTK_IS_COMBO_BOX (wid))
        {
            atk_label (wid, lbl);
            gtk_widget_set_tooltip_text (wid, gtk_widget_get_tooltip_text (GTK_WIDGET (dfold)));
        }
    } while ((child = g_list_next (child)) != NULL);
    g_list_free (children);

    cmsg = gtk_builder_get_object (builder, "label35");

    nb = gtk_builder_get_object (builder, "notebook1");

    mons = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
    for (i = 0; i < ndesks; i++)
    {
        buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), i);
        gtk_list_store_insert_with_values (mons, NULL, i, 0, i, 1, buf, -1);
        g_free (buf);
    }
    sortmons = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (mons));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sortmons), 1, GTK_SORT_ASCENDING);

    if (ndesks > 1)
    {
        GtkCellRenderer *rend = gtk_cell_renderer_text_new ();

        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbdesk), rend, FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (cbdesk), rend, "text", 1);
        gtk_combo_box_set_model (GTK_COMBO_BOX (cbdesk), GTK_TREE_MODEL (sortmons));

        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbbar), rend, FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (cbbar), rend, "text", 1);
        gtk_combo_box_set_model (GTK_COMBO_BOX (cbbar), GTK_TREE_MODEL (sortmons));

        gtk_widget_show (GTK_WIDGET (cb4));
        gtk_widget_show_all (GTK_WIDGET (gtk_builder_get_object (builder, "hbox25")));
        if (cur_conf.common_bg == 0 && st_tab == 1) desktop_n = 1;
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox10")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox25")));
    }

    if (st_tab < 0)
    {
        switch (st_tab)
        {
            case -1 :   gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), 3);
                        break;
            case -2 :   gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), 2);
                        break;
            case -3 :   gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), 1);
                        break;
        }
    }

    set_controls ();

    return FALSE;
}

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

    switch (tab)
    {
        case 0 :
            window = (GtkWidget *) gtk_builder_get_object (builder, "window1");
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox1");
            break;
        case 1 :
            window = (GtkWidget *) gtk_builder_get_object (builder, "window2");
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox2");
            break;
        case 2 :
            window = (GtkWidget *) gtk_builder_get_object (builder, "window3");
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox3");
            break;
        case 3 :
            window = (GtkWidget *) gtk_builder_get_object (builder, "window4");
            plugin = (GtkWidget *) gtk_builder_get_object (builder, "vbox4");
            break;
    }

    gtk_container_remove (GTK_CONTAINER (window), plugin);

    return plugin;
}

void free_plugin (void)
{
    g_object_unref (builder);
}
