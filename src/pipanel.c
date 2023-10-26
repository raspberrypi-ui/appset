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

#define MAX_ICON 68
#define MIN_ICON 16

#define DEFAULT_THEME "PiXflat"
#define DEFAULT_THEME_DARK "PiXflat-dark"
#define TEMP_THEME    "tPiXflat"

#define GREY    "#808080"

#define DEFAULT(x) cur_conf.x=def_med.x

#define XC(str) ((xmlChar *) str)

/* Global variables for window values */

typedef struct {
    const char *desktop_font;
    const char *desktop_picture[2];
    const char *desktop_mode[2];
    const char *terminal_font;
    const char *desktop_folder;
    GdkRGBA theme_colour[2];
    GdkRGBA themetext_colour[2];
    GdkRGBA desktop_colour[2];
    GdkRGBA desktoptext_colour[2];
    GdkRGBA bar_colour[2];
    GdkRGBA bartext_colour[2];
    int icon_size;
    int barpos;
    int show_docs[2];
    int show_trash[2];
    int show_mnts[2];
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
} DesktopConfig;

static DesktopConfig cur_conf, def_lg, def_med, def_sm;

/* Flag to indicate whether lxsession is version 4.9 or later, in which case no need to refresh manually */

static gboolean needs_refresh;

/* Flags to indicate window and settings manager in use */

static gboolean wayfire = FALSE;
static gboolean gsettings = FALSE;

/* Version of Libreoffice installed - affects toolbar icon setting */

static char lo_ver;

/* Handler IDs so they can be blocked when needed */

static gulong cid, iid, bpid, blid, dmid[2], tdid[2], ttid[2], tmid[2], dfid, cbid, draw_id, bdid;

/* Controls */
static GObject *hcol, *htcol, *font, *dcol[2], *dtcol[2], *dmod[2], *dpic[2], *bcol, *btcol, *rb1, *rb2, *rb3, *rb4, *rb5, *rb6;
static GObject *isz, *cb1[2], *cb2[2], *cb3[2], *cb4, *csz, *cmsg, *t1lab, *t2lab, *nb, *dfold;

/* Dialogs */
static GtkWidget *dlg, *msg_dlg;

/* Starting tab value read from command line */
static int st_tab;

static void backup_file (char *filepath);
static void backup_config_files (void);
static int restore_file (char *filepath);
static int restore_config_files (void);
static void reload_gsettings (void);
static void delete_file (char *filepath);
static void reset_to_defaults (void);
static void load_lxsession_settings (void);
static void load_pcman_settings (int desktop);
static void load_pcman_g_settings (void);
static void load_lxpanel_settings (void);
static void load_obconf_settings (void);
static void load_wfshell_settings (void);
static void load_gtk3_settings (void);
static void save_lxpanel_settings (void);
static void save_gtk3_settings (void);
static void save_lxsession_settings (void);
static void save_xsettings (void);
static void save_pcman_settings (int desktop);
static void save_pcman_g_settings (void);
static void save_libfm_settings (void);
static void save_wfshell_settings (void);
static void save_obconf_settings (void);
static void save_lxterm_settings (void);
static void save_libreoffice_settings (void);
static void save_qt_settings (void);
static void add_or_amend (const char *conffile, const char *block, const char *param, const char *repl);
static void save_scrollbar_settings (void);
static void set_controls (void);
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
static void on_bar_loc_set (GtkRadioButton* btn, gpointer ptr);
static void on_bar_pos_set (GtkRadioButton* btn, gpointer ptr);
static void on_toggle_docs (GtkCheckButton* btn, gpointer ptr);
static void on_toggle_trash (GtkCheckButton* btn, gpointer ptr);
static void on_toggle_mnts (GtkCheckButton* btn, gpointer ptr);
static void on_toggle_desktop (GtkCheckButton* btn, gpointer ptr);
static void on_darkmode_set (GtkRadioButton* btn, gpointer ptr);
static void on_cursor_size_set (GtkComboBox* btn, gpointer ptr);
static void on_set_defaults (GtkButton* btn, gpointer ptr);
static void set_tabs (int n_desk);
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

static void message (char *msg)
{
    GtkWidget *wid;
    GtkBuilder *builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/pipanel.ui");

    msg_dlg = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    if (dlg) gtk_window_set_transient_for (GTK_WINDOW (msg_dlg), GTK_WINDOW (dlg));

    wid = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
    gtk_label_set_text (GTK_LABEL (wid), msg);

    gtk_widget_show (msg_dlg);
    gtk_window_set_decorated (GTK_WINDOW (msg_dlg), FALSE);

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

/* Shell commands to reload data */

static void reload_lxpanel (void)
{
    if (wayfire) return;

    vsystem ("lxpanelctl refresh");
}

static void reload_openbox (void)
{
    if (wayfire) return;

    vsystem ("openbox --reconfigure");
}

static void reload_pcmanfm (void)
{
    vsystem ("pcmanfm --reconfigure");
}

static void reload_lxsession (void)
{
    if (wayfire) return;

    if (needs_refresh)
    {
        vsystem ("lxsession -r");
    }
}

static void reload_xsettings (void)
{
    if (!wayfire) return;

    vsystem ("pkill xsettingsd");
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

static char *pcmanfm_file (gboolean global, int desktop)
{
    return g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), desktop == 0 ? "desktop-items-0.conf" : "desktop-items-1.conf", NULL);
}

static char *pcmanfm_g_file (gboolean global)
{
    return g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), "pcmanfm.conf", NULL);
}

static char *libfm_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "libfm/libfm.conf", NULL);
}

static char *wfshell_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "wf-panel-pi.ini", NULL);
}

static void check_directory (char *path)
{
    char *dir = g_path_get_dirname (path);
    g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (dir);
}

// Functions to force reloading of GTK+3 theme

static void init_lxsession (const char *theme)
{
    /* Creates a default lxsession data file with the theme in it - the
     * system checks this for changes and reloads the theme if a change is detected */
    if (!gsettings)
    {
        char *user_config_file = lxsession_file (FALSE);
        if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
        {
            check_directory (user_config_file);
            vsystem ("echo '[GTK]\nsNet/ThemeName=%s' >> %s", theme, user_config_file);
        }
        g_free (user_config_file);
    }
    else vsystem ("gsettings set org.gnome.desktop.interface gtk-theme %s", theme);
}

static void set_theme (const char *theme)
{
    /* Sets the theme in the lxsession data file, which triggers a theme change */
    if (!gsettings)
    {
        char *user_config_file = lxsession_file (FALSE);
        vsystem ("sed -i s#sNet/ThemeName=.*#sNet/ThemeName=%s#g %s", theme, user_config_file);
        g_free (user_config_file);
    }
    else vsystem ("gsettings set org.gnome.desktop.interface gtk-theme %s", theme);
}

static gboolean is_dark (void)
{
    int res;

    if (!gsettings)
    {
        char *user_config_file = lxsession_file (FALSE);
        res = vsystem ("grep sNet/ThemeName %s | grep -q dark", user_config_file);
        g_free (user_config_file);
    }
    else res = system ("gsettings get org.gnome.desktop.interface gtk-theme | grep -q dark");

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

static void reload_theme (gboolean quit)
{
    g_idle_add (restore_theme, (gpointer *) quit);
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
    char *path, *lc_sess, *fname;

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

    path = g_build_filename (".config/pcmanfm", session_name, "desktop-items-0.conf", NULL);
    backup_file (path);
    g_free (path);

    path = g_build_filename (".config/pcmanfm", session_name, "desktop-items-1.conf", NULL);
    backup_file (path);
    g_free (path);

    path = g_build_filename (".config/pcmanfm", session_name, "pcmanfm.conf", NULL);
    backup_file (path);
    g_free (path);

    backup_file (".config/wf-panel-pi.ini");
    backup_file (".config/libfm/libfm.conf");
    backup_file (".config/gtk-3.0/gtk.css");
    backup_file (".config/gtk-3.0/settings.ini");
    backup_file (".local/share/themes/PiXflat/gtk-3.0/gtk.css");
    backup_file (".local/share/themes/PiXflat-dark/gtk-3.0/gtk.css");
    backup_file (".config/qt5ct/qt5ct.conf");
    backup_file (".config/xsettingsd/xsettingsd.conf");
    backup_file (".gtkrc-2.0");

    // app-specific
    backup_file (".config/lxterminal/lxterminal.conf");
    backup_file (".config/libreoffice/4/user/registrymodifications.xcu");
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
    char *path, *lc_sess, *fname;
    int changed = 0;

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

    path = g_build_filename (".config/pcmanfm", session_name, "desktop-items-0.conf", NULL);
    if (restore_file (path)) changed = 1;
    g_free (path);

    path = g_build_filename (".config/pcmanfm", session_name, "desktop-items-1.conf", NULL);
    if (restore_file (path)) changed = 1;
    g_free (path);

    path = g_build_filename (".config/pcmanfm", session_name, "pcmanfm.conf", NULL);
    if (restore_file (path)) changed = 1;
    g_free (path);

    if (restore_file (".config/wf-panel-pi.ini")) changed = 1;
    if (restore_file (".config/libfm/libfm.conf")) changed = 1;
    if (restore_file (".config/gtk-3.0/gtk.css")) changed = 1;
    if (restore_file (".config/gtk-3.0/settings.ini")) changed = 1;
    if (restore_file (".local/share/themes/PiXflat/gtk-3.0/gtk.css")) changed = 1;
    if (restore_file (".local/share/themes/PiXflat-dark/gtk-3.0/gtk.css")) changed = 1;
    if (restore_file (".config/qt5ct/qt5ct.conf")) changed = 1;
    if (restore_file (".config/xsettingsd/xsettingsd.conf")) changed = 1;
    if (restore_file (".gtkrc-2.0")) changed = 1;

    // app-specific
    if (restore_file (".config/lxterminal/lxterminal.conf")) changed = 1;
    if (restore_file (".config/libreoffice/4/user/registrymodifications.xcu")) changed = 1;

    return changed;
}

static void reload_gsettings (void)
{
    if (gsettings)
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
    char *path, *lc_sess, *fname;

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

    path = g_build_filename (".config/pcmanfm", session_name, "desktop-items-0.conf", NULL);
    delete_file (path);
    g_free (path);

    path = g_build_filename (".config/pcmanfm", session_name, "desktop-items-1.conf", NULL);
    delete_file (path);
    g_free (path);

    path = g_build_filename (".config/pcmanfm", session_name, "pcmanfm.conf", NULL);
    delete_file (path);
    g_free (path);

    delete_file (".config/libfm/libfm.conf");
    delete_file (".config/gtk-3.0/gtk.css");
    delete_file (".config/gtk-3.0/settings.ini");
    delete_file (".local/share/themes/PiXflat/gtk-3.0/gtk.css");
    delete_file (".local/share/themes/PiXflat-dark/gtk-3.0/gtk.css");
    delete_file (".config/qt5ct/qt5ct.conf");
    delete_file (".config/xsettingsd/xsettingsd.conf");
    delete_file (".gtkrc-2.0");

    reload_gsettings ();
    init_lxsession (TEMP_THEME);
}


/* Functions to load required values from user config files */

static void load_lxsession_settings (void)
{
    char *user_config_file, *ret, *cptr, *nptr;
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
    user_config_file = pcmanfm_file (FALSE, desktop);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_bg", &err);
        if (err == NULL)
        {
            if (!gdk_rgba_parse (&cur_conf.desktop_colour[desktop], ret))
                DEFAULT (desktop_colour[desktop]);
        }
        else DEFAULT (desktop_colour[desktop]);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_fg", &err);
        if (err == NULL)
        {
            if (!gdk_rgba_parse (&cur_conf.desktoptext_colour[desktop], ret))
                DEFAULT (desktoptext_colour[desktop]);
        }
        else DEFAULT (desktoptext_colour[desktop]);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper", &err);
        if (err == NULL && ret) cur_conf.desktop_picture[desktop] = g_strdup (ret);
        else DEFAULT (desktop_picture[desktop]);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper_mode", &err);
        if (err == NULL && ret) cur_conf.desktop_mode[desktop] = g_strdup (ret);
        else DEFAULT (desktop_mode[desktop]);
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_documents", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.show_docs[desktop] = val;
        else DEFAULT (show_docs[desktop]);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_trash", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.show_trash[desktop] = val;
        else DEFAULT (show_trash[desktop]);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_mounts", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.show_mnts[desktop] = val;
        else DEFAULT (show_mnts[desktop]);

        if (desktop == 1)
        {
            err = NULL;
            ret = g_key_file_get_string (kf, "*", "folder", &err);
            if (err == NULL && ret) cur_conf.desktop_folder = g_strdup (ret);
            else DEFAULT (desktop_folder);
            g_free (ret);
        }
    }
    else
    {
        DEFAULT (desktop_colour[desktop]);
        DEFAULT (desktoptext_colour[desktop]);
        DEFAULT (desktop_picture[desktop]);
        DEFAULT (desktop_mode[desktop]);
        DEFAULT (show_docs[desktop]);
        DEFAULT (show_trash[desktop]);
        DEFAULT (show_mnts[desktop]);
        if (desktop == 1) DEFAULT (desktop_folder);
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

static void load_wfshell_settings (void)
{
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from file to a key file
    user_config_file = wfshell_file ();
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
        val = g_key_file_get_integer (kf, "panel", "monitor", &err);
        if (err == NULL) cur_conf.monitor = val;
        else DEFAULT (monitor);
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
    char *user_config_file, *cmdbuf, *res;
    const char *sys_config_file[2] = { "/usr/share/themes/PiXflat/gtk-3.0/gtk-colours.css", "/usr/share/themes/PiXflat-dark/gtk-3.0/gtk-colours.css" };
    GKeyFile *kf;
    GError *err;
    gint val;

    cur_conf.darkmode = is_dark ();

    for (val = 0; val < 2; val++)
    {
        user_config_file = g_build_filename (g_get_user_data_dir (), val ? "themes/PiXflat-dark/gtk-3.0/gtk.css" : "themes/PiXflat/gtk-3.0/gtk.css", NULL);

        cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\stheme_selected_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", user_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&cur_conf.theme_colour[val], res))
        {
            g_free (res);
            cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\stheme_selected_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", sys_config_file[val]);
            res = get_string (cmdbuf);
            g_free (cmdbuf);
            if (!res[0] || !gdk_rgba_parse (&cur_conf.theme_colour[val], res)) DEFAULT (theme_colour[val]);
        }
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\stheme_selected_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", user_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&cur_conf.themetext_colour[val], res))
        {
            g_free (res);
            cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\stheme_selected_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", sys_config_file[val]);
            res = get_string (cmdbuf);
            g_free (cmdbuf);
            if (!res[0] || !gdk_rgba_parse (&cur_conf.themetext_colour[val], res)) DEFAULT (themetext_colour[val]);
        }
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\sbar_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", user_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&cur_conf.bar_colour[val], res))
        {
            g_free (res);
            cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\sbar_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", sys_config_file[val]);
            res = get_string (cmdbuf);
            g_free (cmdbuf);
            if (!res[0] || !gdk_rgba_parse (&cur_conf.bar_colour[val], res)) DEFAULT (bar_colour[val]);
        }
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\sbar_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", user_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&cur_conf.bartext_colour[val], res))
        {
            g_free (res);
            cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\sbar_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", sys_config_file[val]);
            res = get_string (cmdbuf);
            g_free (cmdbuf);
            if (!res[0] || !gdk_rgba_parse (&cur_conf.bartext_colour[val], res)) DEFAULT (bartext_colour[val]);
        }
        g_free (res);

        g_free (user_config_file);
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
    char *user_config_file, *cstrb, *cstrf, *cstrbb, *cstrbf, *link1, *link2, *str;
    GKeyFile *kf;
    gsize len;
    int dark;

    // delete old file used to store general overrides
    user_config_file = g_build_filename (g_get_user_config_dir (), "gtk-3.0/gtk.css", NULL);
    vsystem ("if grep -q -s define-color %s ; then rm %s ; fi", user_config_file, user_config_file);
    g_free (user_config_file);

    // create a temp theme to switch to
    link1 = g_build_filename (g_get_user_data_dir (), "themes/tPiXflat", NULL);
    if (!g_file_test (link1, G_FILE_TEST_IS_DIR))
    {
        link2 = g_build_filename (g_get_user_data_dir (), "themes/PiXflat", NULL);
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
        user_config_file = g_build_filename (g_get_user_data_dir (), dark ? "themes/PiXflat-dark/gtk-3.0/gtk.css" : "themes/PiXflat/gtk-3.0/gtk.css", NULL);
        check_directory (user_config_file);

        if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
        {
            vsystem ("echo '@import url(\"/usr/share/themes/PiXflat%s/gtk-3.0/gtk.css\");' >> %s", dark ? "-dark" : "", user_config_file);
            vsystem ("echo '@define-color theme_selected_bg_color %s;' >> %s", cstrb, user_config_file);
            vsystem ("echo '@define-color theme_selected_fg_color %s;' >> %s", cstrf, user_config_file);
            vsystem ("echo '@define-color bar_bg_color %s;' >> %s", cstrbb, user_config_file);
            vsystem ("echo '@define-color bar_fg_color %s;' >> %s", cstrbf, user_config_file);

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
        g_free (user_config_file);
    }
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

    if (gsettings)
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

    int tbi = GTK_ICON_SIZE_LARGE_TOOLBAR;
    if (cur_conf.tb_icon_size == 16) tbi = GTK_ICON_SIZE_SMALL_TOOLBAR;
    if (cur_conf.tb_icon_size == 48) tbi = GTK_ICON_SIZE_DIALOG;

    // use sed to write
    vsystem ("sed -i s/'FontName.*'/'FontName \"%s\"'/g %s", cur_conf.desktop_font, user_config_file);
    vsystem ("sed -i s/'ToolbarIconSize.*'/'ToolbarIconSize %d'/g %s", tbi, user_config_file);
    vsystem ("sed -i s/'CursorThemeSize.*'/'CursorThemeSize %d'/g %s", cur_conf.cursor_size, user_config_file);
    vsystem ("sed -i s/gtk-large-toolbar=[0-9]+,[0-9]+/gtk-large-toolbar=%d,%d/g %s", cur_conf.tb_icon_size, cur_conf.tb_icon_size, user_config_file);

    g_free (user_config_file);
}

static void save_pcman_settings (int desktop)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = pcmanfm_file (FALSE, desktop);
    check_directory (user_config_file);

    // process pcmanfm config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    str = rgba_to_gdk_color_string (&cur_conf.desktop_colour[desktop]);
    g_key_file_set_string (kf, "*", "desktop_bg", str);
    g_key_file_set_string (kf, "*", "desktop_shadow", str);
    g_free (str);

    str = rgba_to_gdk_color_string (&cur_conf.desktoptext_colour[desktop]);
    g_key_file_set_string (kf, "*", "desktop_fg", str);
    g_free (str);

    g_key_file_set_string (kf, "*", "desktop_font", cur_conf.desktop_font);
    g_key_file_set_string (kf, "*", "wallpaper", cur_conf.desktop_picture[desktop]);
    g_key_file_set_string (kf, "*", "wallpaper_mode", cur_conf.desktop_mode[desktop]);
    g_key_file_set_integer (kf, "*", "show_documents", cur_conf.show_docs[desktop]);
    g_key_file_set_integer (kf, "*", "show_trash", cur_conf.show_trash[desktop]);
    g_key_file_set_integer (kf, "*", "show_mounts", cur_conf.show_mnts[desktop]);

    if (desktop == 1) g_key_file_set_string (kf, "*", "folder", cur_conf.desktop_folder);

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
    check_directory (user_config_file);

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

static void save_wfshell_settings (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = wfshell_file ();
    check_directory (user_config_file);

    // process pcmanfm config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_string (kf, "panel", "position", cur_conf.barpos ? "bottom" : "top");
    g_key_file_set_integer (kf, "panel", "icon_size", cur_conf.icon_size - 4);
    g_key_file_set_integer (kf, "panel", "window-list_max_width", cur_conf.task_width);
    g_key_file_set_integer (kf, "panel", "monitor", cur_conf.monitor);

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

static void save_obconf_settings (void)
{
    char *user_config_file, *font, *cptr;
    int count, size;
    const gchar *weight = NULL, *style = NULL;
    char buf[10];

    xmlDocPtr xDoc;
    xmlNodePtr root, cur_node, node;
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;

    user_config_file = openbox_file ();
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
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']", xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        root = xmlNewNode (NULL, (xmlChar *) "openbox_config");
        xmlDocSetRootElement (xDoc, root);
    }
    else root = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval)) xmlNewChild (root, NULL, (xmlChar *) "theme", NULL);
    xmlXPathFreeObject (xpathObj);

    // update relevant nodes with new values
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='font']", xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);
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

    sprintf (buf, "%d", cur_conf.handle_width);
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='invHandleWidth']", xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNewChild (cur_node, NULL, XC ("invHandleWidth"), XC (buf));
    }
    else
    {
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNodeSetContent (cur_node, XC (buf));
    }

    cptr = rgba_to_gdk_color_string (&cur_conf.theme_colour[cur_conf.darkmode]);
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titleColor']", xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);
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
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='textColor']", xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNewChild (cur_node, NULL, XC ("textColor"), XC (cptr));
    }
    else
    {
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNodeSetContent (cur_node, XC (cptr));
    }
    g_free (cptr);

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

static void save_scrollbar_settings (void)
{
    char *conffile, *repl, *block;

    // GTK2 override file
    conffile = g_build_filename (g_get_home_dir (), ".gtkrc-2.0", NULL);

    // check if the scrollbar button entry is in the file - if not, add it...
    repl = g_strdup_printf ("GtkRange::slider-width = %d", cur_conf.scrollbar_width);
    add_or_amend (conffile, "style \"scrollbar\"", "GtkRange::slider-width\\s*=\\s*[0-9]*", repl);
    g_free (repl);

    repl = g_strdup_printf ("GtkRange::stepper-size = %d", cur_conf.scrollbar_width);
    add_or_amend (conffile, "style \"scrollbar\"", "GtkRange::stepper-size\\s*=\\s*[0-9]*", repl);
    g_free (repl);

    g_free (conffile);

    // GTK3 override file
    conffile = g_build_filename (g_get_user_data_dir (), cur_conf.darkmode ? "themes/PiXflat-dark/gtk-3.0/gtk.css" : "themes/PiXflat/gtk-3.0/gtk.css", NULL);
    check_directory (conffile);

    // check if the scrollbar button entry is in the file - if not, add it...
    repl = g_strdup_printf ("min-width: %dpx;", cur_conf.scrollbar_width - 6);
    add_or_amend (conffile, "scrollbar button", "min-width:\\s*[0-9]*px;", repl);
    g_free (repl);

    repl = g_strdup_printf ("min-height: %dpx;", cur_conf.scrollbar_width - 6);
    add_or_amend (conffile, "scrollbar button", "min-height:\\s*[0-9]*px;", repl);
    g_free (repl);

    // check if the scrollbar slider entry is in the file - if not, add it...
    repl = g_strdup_printf ("min-width: %dpx;", cur_conf.scrollbar_width - 6);
    add_or_amend (conffile, "scrollbar slider", "min-width:\\s*[0-9]*px;", repl);
    g_free (repl);

    repl = g_strdup_printf ("min-height: %dpx;", cur_conf.scrollbar_width - 6);
    add_or_amend (conffile, "scrollbar slider", "min-height:\\s*[0-9]*px;", repl);
    g_free (repl);

    // process active scrollbar button icons
    int i;
    const char *dl[4] = { "d", "u", "r", "l" };
    for (i = 0; i < 4; i++)
    {
        block = g_strdup_printf ("scrollbar.%s button.%s", i < 2 ? "vertical" : "horizontal", i % 2 ? "up" : "down");
        repl = g_strdup_printf ("-gtk-icon-source: -gtk-icontheme(\"%sscroll_%s\");", cur_conf.scrollbar_width >= 18 ? "l" : "", dl[i]);

        add_or_amend (conffile, block, "-gtk-icon-source:.*;", repl);
        g_free (repl);
        g_free (block);
    }

    for (i = 0; i < 4; i++)
    {
        block = g_strdup_printf ("scrollbar.%s button:disabled.%s", i < 2 ? "vertical" : "horizontal", i % 2 ? "up" : "down");
        repl = g_strdup_printf ("-gtk-icon-source: -gtk-icontheme(\"%sscroll_%s_d\");", cur_conf.scrollbar_width >= 18 ? "l" : "", dl[i]);

        add_or_amend (conffile, block, "-gtk-icon-source:.*;", repl);
        g_free (repl);
        g_free (block);
    }

    g_free (conffile);
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
    if (wayfire)
    {
        save_wfshell_settings ();
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
    save_obconf_settings ();
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
    save_obconf_settings ();
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
    save_gtk3_settings ();
    reload_lxsession ();
    reload_xsettings ();
    reload_pcmanfm ();
    reload_theme (FALSE);
}

static void on_desktop_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    int desk = (int) ptr;
    gtk_color_chooser_get_rgba (btn, &cur_conf.desktop_colour[desk]);
    save_pcman_settings (desk);
    reload_pcmanfm ();
}

static void on_desktoptext_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    int desk = (int) ptr;
    gtk_color_chooser_get_rgba (btn, &cur_conf.desktoptext_colour[desk]);
    save_pcman_settings (desk);
    reload_pcmanfm ();
}

static void on_desktop_picture_set (GtkFileChooser* btn, gpointer ptr)
{
    int desk = (int) ptr;
    char *picture = gtk_file_chooser_get_filename (btn);
    if (picture) cur_conf.desktop_picture[desk] = picture;
    save_pcman_settings (desk);
    reload_pcmanfm ();
}

static void on_desktop_font_set (GtkFontChooser* btn, gpointer ptr)
{
    const char *font = gtk_font_chooser_get_font (btn);
    if (font) cur_conf.desktop_font = font;

    save_lxsession_settings ();
    save_xsettings ();
    save_pcman_settings (0);
    save_pcman_settings (1);
    save_obconf_settings ();
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
    int desk = (int) ptr;
    gint val = gtk_combo_box_get_active (btn);
    switch (val)
    {
        case 0 :    cur_conf.desktop_mode[desk] = "color";
                    break;
        case 1 :    cur_conf.desktop_mode[desk] = "center";
                    break;
        case 2 :    cur_conf.desktop_mode[desk] = "fit";
                    break;
        case 3 :    cur_conf.desktop_mode[desk] = "crop";
                    break;
        case 4 :    cur_conf.desktop_mode[desk] = "stretch";
                    break;
        case 5 :    cur_conf.desktop_mode[desk] = "tile";
                    break;
    }

    if (!strcmp (cur_conf.desktop_mode[desk], "color")) gtk_widget_set_sensitive (GTK_WIDGET (dpic[desk]), FALSE);
    else gtk_widget_set_sensitive (GTK_WIDGET (dpic[desk]), TRUE);
    save_pcman_settings (desk);
    reload_pcmanfm ();
}

static void on_desktop_folder_set (GtkFileChooser* btn, gpointer ptr)
{
    char *folder = gtk_file_chooser_get_filename (btn);
    if (folder)
    {
        if (g_strcmp0 (cur_conf.desktop_folder, folder))
        {
            cur_conf.desktop_folder = folder;
            save_pcman_settings (1);
            reload_pcmanfm ();
        }
    }
}

static void on_bar_pos_set (GtkRadioButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) cur_conf.barpos = 0;
    else cur_conf.barpos = 1;
    if (wayfire)
    {
        save_wfshell_settings ();
        reload_pcmanfm ();
    }
    else
    {
        save_lxpanel_settings ();
        reload_lxpanel ();
    }
}

static void on_bar_loc_set (GtkRadioButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) cur_conf.monitor = 0;
    else cur_conf.monitor = 1;
    if (wayfire)
    {
        save_wfshell_settings ();
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
    int desk = (int) ptr;
    cur_conf.show_docs[desk] = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings (desk);
    reload_pcmanfm ();
}

static void on_toggle_trash (GtkCheckButton* btn, gpointer ptr)
{
    int desk = (int) ptr;
    cur_conf.show_trash[desk] = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings (desk);
    reload_pcmanfm ();
}

static void on_toggle_mnts (GtkCheckButton* btn, gpointer ptr)
{
    int desk = (int) ptr;
    cur_conf.show_mnts[desk] = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings (desk);
    reload_pcmanfm ();
}

static void on_darkmode_set (GtkRadioButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) cur_conf.darkmode = 0;
    else cur_conf.darkmode = 1;
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (hcol), &cur_conf.theme_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (htcol), &cur_conf.themetext_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (bcol), &cur_conf.bar_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (btcol), &cur_conf.bartext_colour[cur_conf.darkmode]);
    save_gtk3_settings ();
    reload_theme (FALSE);
}

static void set_tabs (int n_desk)
{
    char *buf;

    if (n_desk > 1)
    {
        gtk_widget_show (gtk_notebook_get_nth_page (GTK_NOTEBOOK (nb), 1));
        buf = g_strdup_printf (_("Desktop 1"));
        gtk_label_set_text (GTK_LABEL (t1lab), buf);
        gtk_label_set_justify (GTK_LABEL (t1lab), GTK_JUSTIFY_CENTER);
        g_free (buf);
        buf = g_strdup_printf (_("Desktop 2"));
        gtk_label_set_text (GTK_LABEL (t2lab), buf);
        gtk_label_set_justify (GTK_LABEL (t2lab), GTK_JUSTIFY_CENTER);
        g_free (buf);
    }
    else
    {
        gtk_widget_hide (gtk_notebook_get_nth_page (GTK_NOTEBOOK (nb), 1));
        gtk_label_set_text (GTK_LABEL (t1lab), _("Desktop"));
    }
}

static void on_toggle_desktop (GtkCheckButton* btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    {
        cur_conf.common_bg = 1;
        set_tabs (1);
    }
    else
    {
        cur_conf.common_bg = 0;
        set_tabs (2);
    }
    save_pcman_g_settings ();
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
    reload_lxsession ();
    reload_xsettings ();
    reload_theme (FALSE);
}


static void set_controls (void)
{
    int i;

    // block widget handlers
    g_signal_handler_block (isz, iid);
    g_signal_handler_block (csz, cid);
    g_signal_handler_block (rb1, bpid);
    g_signal_handler_block (rb3, blid);
    g_signal_handler_block (rb5, bdid);
    g_signal_handler_block (dfold, dfid);
    g_signal_handler_block (cb4, cbid);
    for (i = 0; i < 2; i++)
    {
        g_signal_handler_block (dmod[i], dmid[i]);
        g_signal_handler_block (cb1[i], tdid[i]);
        g_signal_handler_block (cb2[i], ttid[i]);
        g_signal_handler_block (cb3[i], tmid[i]);
    }

    gtk_font_chooser_set_font (GTK_FONT_CHOOSER (font), cur_conf.desktop_font);
    for (i = 0; i < 2; i++)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (dpic[i]), TRUE);
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dpic[i]), cur_conf.desktop_picture[i]);
        if (!strcmp (cur_conf.desktop_mode[i], "center")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod[i]), 1);
        else if (!strcmp (cur_conf.desktop_mode[i], "fit")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod[i]), 2);
        else if (!strcmp (cur_conf.desktop_mode[i], "crop")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod[i]), 3);
        else if (!strcmp (cur_conf.desktop_mode[i], "stretch")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod[i]), 4);
        else if (!strcmp (cur_conf.desktop_mode[i], "tile")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod[i]), 5);
        else
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX (dmod[i]), 0);
            gtk_widget_set_sensitive (GTK_WIDGET (dpic[i]), FALSE);
        }
        gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dcol[i]), &cur_conf.desktop_colour[i]);
        gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dtcol[i]), &cur_conf.desktoptext_colour[i]);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb1[i]), cur_conf.show_docs[i]);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb2[i]), cur_conf.show_trash[i]);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb3[i]), cur_conf.show_mnts[i]);
    }
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

    if (cur_conf.monitor) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb4), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb3), TRUE);

    if (cur_conf.darkmode) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb6), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb5), TRUE);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb4), cur_conf.common_bg);
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dfold), cur_conf.desktop_folder);

    // unblock widget handlers
    g_signal_handler_unblock (isz, iid);
    g_signal_handler_unblock (csz, cid);
    g_signal_handler_unblock (rb1, bpid);
    g_signal_handler_unblock (rb3, blid);
    g_signal_handler_unblock (rb5, bdid);
    g_signal_handler_unblock (dfold, dfid);
    g_signal_handler_unblock (cb4, cbid);
    for (i = 0; i < 2; i++)
    {
        g_signal_handler_unblock (dmod[i], dmid[i]);
        g_signal_handler_unblock (cb1[i], tdid[i]);
        g_signal_handler_unblock (cb2[i], ttid[i]);
        g_signal_handler_unblock (cb3[i], tmid[i]);
    }
}

static void on_set_defaults (GtkButton* btn, gpointer ptr)
{
    // clear all the config files
    reset_to_defaults ();

    // set config structure to a default
    switch ((int) ptr)
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
    if (ptr != 2)
    {
        save_lxsession_settings ();
        save_xsettings ();
        save_pcman_g_settings ();
        save_pcman_settings (0);
        save_pcman_settings (1);
        save_libfm_settings ();
        save_obconf_settings ();
        save_gtk3_settings ();
        if (!wayfire) save_lxpanel_settings ();
        save_qt_settings ();
        save_scrollbar_settings ();
    }

    if (wayfire) save_wfshell_settings ();

    // save application-specific config - we don't delete these files first...
    save_lxterm_settings ();
    save_libreoffice_settings ();

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
    char *user_config_file, *ret, *cptr, *nptr;
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
    user_config_file = pcmanfm_file (TRUE, desktop);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_bg", &err);
        if (err == NULL)
        {
            if (!gdk_rgba_parse (&def_med.desktop_colour[desktop], ret))
                gdk_rgba_parse (&def_med.desktop_colour[desktop], GREY);
        }
        else gdk_rgba_parse (&def_med.desktop_colour[desktop], GREY);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_fg", &err);
        if (err == NULL)
        {
            if (!gdk_rgba_parse (&def_med.desktoptext_colour[desktop], ret))
                gdk_rgba_parse (&def_med.desktoptext_colour[desktop], GREY);
        }
        else gdk_rgba_parse (&def_med.desktoptext_colour[desktop], GREY);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper", &err);
        if (err == NULL && ret) def_med.desktop_picture[desktop] = g_strdup (ret);
        else def_med.desktop_picture[desktop] = "";
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper_mode", &err);
        if (err == NULL && ret) def_med.desktop_mode[desktop] = g_strdup (ret);
        else def_med.desktop_mode[desktop] = "color";
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_documents", &err);
        if (err == NULL && val >= 0 && val <= 1) def_med.show_docs[desktop] = val;
        else def_med.show_docs[desktop] = 0;

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_trash", &err);
        if (err == NULL && val >= 0 && val <= 1) def_med.show_trash[desktop] = val;
        else def_med.show_trash[desktop] = 0;

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_mounts", &err);
        if (err == NULL && val >= 0 && val <= 1) def_med.show_mnts[desktop] = val;
        else def_med.show_mnts[desktop] = 0;

        if (desktop == 1)
        {
            err = NULL;
            ret = g_key_file_get_string (kf, "*", "folder", &err);
            if (err == NULL && ret) def_med.desktop_folder = g_strdup (ret);
            else def_med.desktop_folder = g_build_filename (g_get_home_dir (), "Desktop", NULL);
            g_free (ret);
        }
    }
    else
    {
        def_med.desktop_picture[desktop] = "";
        def_med.desktop_mode[desktop] = "color";
        gdk_rgba_parse (&def_med.desktop_colour[desktop], GREY);
        gdk_rgba_parse (&def_med.desktoptext_colour[desktop], GREY);
        def_med.show_docs[desktop] = 0;
        def_med.show_trash[desktop] = 0;
        def_med.show_mnts[desktop] = 0;
        if (desktop == 1) def_med.desktop_folder = g_build_filename (g_get_home_dir (), "Desktop", NULL);
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
    char *cmdbuf, *res;
    const char *sys_config_file[2] = { "/usr/share/themes/PiXflat/gtk-3.0/gtk-colours.css", "/usr/share/themes/PiXflat-dark/gtk-3.0/gtk-colours.css" };
    GKeyFile *kf;
    GError *err;
    gint val;

    def_med.darkmode = 0;

    for (val = 0; val < 2; val++)
    {
        cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\stheme_selected_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", sys_config_file[val]);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.theme_colour[val], res)) gdk_rgba_parse (&def_med.theme_colour[val], GREY);

        cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\stheme_selected_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", sys_config_file[val]);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.themetext_colour[val], res)) gdk_rgba_parse (&def_med.themetext_colour[val], GREY);

        cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\sbar_bg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", sys_config_file[val]);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.bar_colour[val], res)) gdk_rgba_parse (&def_med.bar_colour[val], GREY);

        cmdbuf = g_strdup_printf ("grep -Po '(?<=@define-color\\sbar_fg_color\\s)#[0-9A-Fa-f]{6}' %s 2> /dev/null", sys_config_file[val]);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.bartext_colour[val], res)) gdk_rgba_parse (&def_med.bartext_colour[val], GREY);
    }
}

static void create_defaults (void)
{
    // defaults for controls

    // /etc/xdg/lxpanel/LXDE-pi/panels/panel
    defaults_lxpanel ();

    // /etc/xdg/lxsession/LXDE-pi/desktop.conf
    defaults_lxsession ();

    // /etc/xdg/pcmanfm/LXDE-pi/desktop-items-0.conf
    defaults_pcman (0);

    // /etc/xdg/pcmanfm/LXDE-pi/desktop-items-1.conf
    defaults_pcman (1);

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
    def_lg.scrollbar_width = 18;

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

int get_common_bg (gboolean global)
{
    GKeyFile *kf;
    GError *err;
    gint val;

    char *fname = g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), "pcmanfm.conf", NULL);

    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, fname, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "common_bg", &err);
        if (err == NULL && (val == 0 || val == 1))
        {
            g_key_file_free (kf);
            g_free (fname);
            return val;
        }
    }

    g_key_file_free (kf);
    g_free (fname);
    return -1;
}

static int n_desktops (void)
{
    int i, n, m;
    char *res;

    if (wayfire)
        res = get_string ("wlr-randr | grep -cv '^ '");
    else
        res = get_string ("xrandr -q | grep -cw connected");

    n = sscanf (res, "%d", &m);
    g_free (res);

    if (n == 1 && m >= 2) return 2;
    return 1;
}

static gpointer restore_thread (gpointer ptr)
{
    if (restore_config_files ())
    {
        set_theme (TEMP_THEME);
        reload_lxsession ();
        reload_xsettings ();
        reload_gsettings ();
        reload_lxpanel ();
        reload_openbox ();
        reload_pcmanfm ();
        reload_theme (TRUE);
    }
    else gtk_main_quit ();
    return NULL;
}

static gboolean cancel_main (GtkButton *button, gpointer data)
{
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


/* The dialog... */

static gboolean init_config (gpointer data)
{
    GtkBuilder *builder;
    GObject *item;
    GtkWidget *wid;
    GtkLabel *lbl;
    GList *children, *child;
    int maj, min, sub, i;

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

    // check xsettings
    const char *type = g_getenv ("XDG_SESSION_TYPE");
    if (type && !strcmp (type, "wayland")) gsettings = TRUE;

    // load data from config files
    create_defaults ();

    load_lxsession_settings ();
    load_pcman_g_settings ();
    load_pcman_settings (0);
    load_pcman_settings (1);
    if (wayfire) load_wfshell_settings ();
    else load_lxpanel_settings ();
    load_obconf_settings ();
    load_gtk3_settings ();

    init_lxsession (cur_conf.darkmode ? DEFAULT_THEME_DARK : DEFAULT_THEME);
    backup_config_files ();

    // build the UI
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/pipanel.ui");
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "main_window");
    g_signal_connect (dlg, "delete_event", G_CALLBACK (close_prog), NULL);

    wid = (GtkWidget *) gtk_builder_get_object (builder, "button_ok");
    g_signal_connect (wid, "clicked", G_CALLBACK (ok_main), NULL);
    wid = (GtkWidget *) gtk_builder_get_object (builder, "button_cancel");
    g_signal_connect (wid, "clicked", G_CALLBACK (cancel_main), NULL);

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
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), 3);

    item = gtk_builder_get_object (builder, "defs_med");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), 2);

    item = gtk_builder_get_object (builder, "defs_sml");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), 1);

    rb1 = gtk_builder_get_object (builder, "radiobutton1");
    rb2 = gtk_builder_get_object (builder, "radiobutton2");
    bpid = g_signal_connect (rb1, "toggled", G_CALLBACK (on_bar_pos_set), NULL);

    rb3 = gtk_builder_get_object (builder, "radiobutton3");
    rb4 = gtk_builder_get_object (builder, "radiobutton4");
    blid = g_signal_connect (rb3, "toggled", G_CALLBACK (on_bar_loc_set), NULL);

    isz = gtk_builder_get_object (builder, "comboboxtext2");
    iid = g_signal_connect (isz, "changed", G_CALLBACK (on_menu_size_set), NULL);

    csz = gtk_builder_get_object (builder, "comboboxtext3");
    cid = g_signal_connect (csz, "changed", G_CALLBACK (on_cursor_size_set), NULL);

    rb5 = gtk_builder_get_object (builder, "radiobutton5");
    rb6 = gtk_builder_get_object (builder, "radiobutton6");
    bdid = g_signal_connect (rb5, "toggled", G_CALLBACK (on_darkmode_set), NULL);

    for (i = 0; i < 2; i++)
    {
        dcol[i] = gtk_builder_get_object (builder, i ? "colorbutton2_" : "colorbutton2");
        g_signal_connect (dcol[i], "color-set", G_CALLBACK (on_desktop_colour_set), (void *) i);

        dtcol[i] = gtk_builder_get_object (builder, i ? "colorbutton6_" : "colorbutton6");
        g_signal_connect (dtcol[i], "color-set", G_CALLBACK (on_desktoptext_colour_set), (void *) i);

        dpic[i] = gtk_builder_get_object (builder, i ? "filechooserbutton1_" : "filechooserbutton1");
        g_signal_connect (dpic[i], "file-set", G_CALLBACK (on_desktop_picture_set), (void *) i);

        // add accessibility label to button child of file chooser
        lbl = GTK_LABEL (gtk_builder_get_object (builder, i ? "label12_" : "label12"));
        children = gtk_container_get_children (GTK_CONTAINER (dpic[i]));
        child = children;
        do
        {
            wid = GTK_WIDGET (child->data);
            if (GTK_IS_BUTTON (wid))
            {
                atk_label (wid, lbl);
                gtk_widget_set_tooltip_text (wid, gtk_widget_get_tooltip_text (GTK_WIDGET (dpic[i])));
            }
        } while ((child = g_list_next (child)) != NULL);
        g_list_free (children);

        dmod[i] = gtk_builder_get_object (builder, i ? "comboboxtext1_" : "comboboxtext1");
        dmid[i] = g_signal_connect (dmod[i], "changed", G_CALLBACK (on_desktop_mode_set), (void *) i);

        cb1[i] = gtk_builder_get_object (builder, i ? "checkbutton1_" : "checkbutton1");
        tdid[i] = g_signal_connect (cb1[i], "toggled", G_CALLBACK (on_toggle_docs), (void *) i);

        cb2[i] = gtk_builder_get_object (builder, i ? "checkbutton2_" : "checkbutton2");
        ttid[i] = g_signal_connect (cb2[i], "toggled", G_CALLBACK (on_toggle_trash), (void *) i);

        cb3[i] = gtk_builder_get_object (builder, i ? "checkbutton3_" : "checkbutton3");
        tmid[i] = g_signal_connect (cb3[i], "toggled", G_CALLBACK (on_toggle_mnts), (void *) i);
    }

    cb4 = gtk_builder_get_object (builder, "checkbutton4");
    cbid = g_signal_connect (cb4, "toggled", G_CALLBACK (on_toggle_desktop), NULL);

    dfold = gtk_builder_get_object (builder, "filechooserbutton4");
    dfid = g_signal_connect (dfold, "selection-changed", G_CALLBACK (on_desktop_folder_set), NULL);

    // add accessibility label to combo box child of file chooser (yes, I know the previous one attached to a button...)
    lbl = GTK_LABEL (gtk_builder_get_object (builder, "label16_"));
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

    t1lab = gtk_builder_get_object (builder, "tablabel1");
    t2lab = gtk_builder_get_object (builder, "tablabel1_");
    nb = gtk_builder_get_object (builder, "notebook1");

    i = n_desktops ();
    if (i > 1)
    {
        gtk_widget_show (GTK_WIDGET (cb4));
        gtk_widget_show_all (GTK_WIDGET (gtk_builder_get_object (builder, "hbox25")));
        gtk_button_set_label (GTK_BUTTON (rb3), _("Desktop 1"));
        gtk_button_set_label (GTK_BUTTON (rb4), _("Desktop 2"));
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (cb4));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox25")));
    }

    if (i > 1 && cur_conf.common_bg == 0)
    {
        set_tabs (2);
        if (st_tab == 1) gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), 1);
    }
    else set_tabs (1);

    if (st_tab < 0)
    {
        switch (st_tab)
        {
            case -1 :   gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), 4);
                        break;
            case -2 :   gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), 3);
                        break;
            case -3 :   gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), 2);
                        break;
        }
    }

    g_object_unref (builder);

    set_controls ();

    gtk_widget_show (dlg);
    gtk_widget_destroy (msg_dlg);

    return FALSE;
}

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
#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    // read starting tab if there is one
    if (argc > 1)
    {
        if (sscanf (argv[1], "%d", &st_tab) != 1) st_tab = 0;
    }

    // GTK setup
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    if (getenv ("WAYFIRE_CONFIG_FILE")) wayfire = TRUE;

    message (_("Loading configuration - please wait..."));
    if (wayfire) g_signal_connect (msg_dlg, "event", G_CALLBACK (event), NULL);
    else draw_id = g_signal_connect (msg_dlg, "draw", G_CALLBACK (draw), NULL);

    gtk_main ();

    gtk_widget_destroy (dlg);

    return 0;
}
