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

#define DEFAULT_SES "LXDE-pi"

#define DEFAULT(x) cur_conf.x=def_med.x

/* Global variables for window values */

typedef struct {
    const char *desktop_font;
    const char *desktop_picture;
    const char *desktop_mode;
    const char *terminal_font;
    GdkColor theme_colour;
    GdkColor themetext_colour;
    GdkColor desktop_colour;
    GdkColor desktoptext_colour;
    GdkColor bar_colour;
    GdkColor bartext_colour;
    int icon_size;
    int barpos;
    int show_docs;
    int show_trash;
    int show_mnts;
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
} DesktopConfig;

static DesktopConfig cur_conf, def_lg, def_med, def_sm;

static const char *orig_desktop_font;
static const char *orig_desktop_picture;
static const char *orig_desktop_mode;
static const char *orig_lxsession_theme;
static const char *orig_openbox_theme;
static GdkColor orig_desktop_colour;
static GdkColor orig_desktoptext_colour;
static int orig_cursor_size;

/* Flag to indicate whether lxsession is version 4.9 or later, in which case no need to refresh manually */

static gboolean needs_refresh;

/* Version of Libreoffice installed - affects toolbar icon setting */

static char lo_ver;

/* Handler IDs so they can be blocked when needed */

static gulong cid, iid, bpid1, bpid2, dmid, tdid, ttid, tmid;

/* Controls */
static GObject *hcol, *htcol, *font, *dcol, *dtcol, *dmod, *dpic, *barh, *bcol, *btcol, *rb1, *rb2, *isz, *cb1, *cb2, *cb3, *csz, *cmsg;

static void backup_file (char *filepath);
static void backup_config_files (void);
static int restore_file (char *filepath);
static int restore_config_files (void);
static void delete_file (char *filepath);
static void reset_to_defaults (void);
static void load_lxsession_settings (void);
static void load_pcman_settings (void);
static void load_libfm_settings (void);
static void load_lxpanel_settings (void);
static void load_lxterm_settings (void);
static void load_libreoffice_settings (void);
static void load_obconf_settings (void);
static void save_lxpanel_settings (void);
static void save_gtk3_settings (void);
static void save_lxsession_settings (void);
static void save_pcman_settings (void);
static void save_libfm_settings (void);
static void save_obconf_settings (void);
static void save_lxterm_settings (void);
static void save_greeter_settings (void);
static void save_libreoffice_settings (void);
static void save_qt_settings (void);
static void add_or_amend (const char *conffile, const char *block, const char *param, const char *repl);
static void save_scrollbar_settings (void);
static void set_controls (void);
static void create_defaults (void);
static void on_menu_size_set (GtkComboBox* btn, gpointer ptr);
static void on_theme_colour_set (GtkColorButton* btn, gpointer ptr);
static void on_themetext_colour_set (GtkColorButton* btn, gpointer ptr);
static void on_bar_colour_set (GtkColorButton* btn, gpointer ptr);
static void on_bartext_colour_set (GtkColorButton* btn, gpointer ptr);
static void on_desktop_colour_set (GtkColorButton* btn, gpointer ptr);
static void on_desktoptext_colour_set (GtkColorButton* btn, gpointer ptr);
static void on_desktop_picture_set (GtkFileChooser* btn, gpointer ptr);
static void on_desktop_font_set (GtkFontButton* btn, gpointer ptr);
static void on_desktop_mode_set (GtkComboBox* btn, gpointer ptr);
static void on_bar_pos_set (GtkRadioButton* btn, gpointer ptr);
static void on_toggle_docs (GtkCheckButton* btn, gpointer ptr);
static void on_toggle_trash (GtkCheckButton* btn, gpointer ptr);
static void on_toggle_mnts (GtkCheckButton* btn, gpointer ptr);
static void on_cursor_size_set (GtkComboBox* btn, gpointer ptr);
static void on_set_defaults (GtkButton* btn, gpointer ptr);

/* Shell commands to reload data */

static void reload_lxpanel (void)
{
    int res = system ("lxpanelctl refresh");
}

static void reload_openbox (void)
{
    int res = system ("openbox --reconfigure");
}

static void reload_pcmanfm (void)
{
    int res = system ("pcmanfm --reconfigure");
}

static void reload_lxsession (void)
{
    if (needs_refresh)
    {
        int res = system ("lxsession -r");
    }
}

static char *openbox_file (void)
{
    const char *session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    char *lc_sess = g_ascii_strdown (session_name, -1);
    char *fname = g_strconcat (lc_sess, "-rc.xml", NULL);
    char *path = g_build_filename (g_get_user_config_dir (), "openbox", fname, NULL);
    g_free (lc_sess);
    g_free (fname);
    return path;
}

static char *lxsession_file (void)
{
    const char *session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    return g_build_filename (g_get_user_config_dir (), "lxsession", session_name, "desktop.conf", NULL);
}

static char *lxpanel_file (void)
{
    const char *session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    return g_build_filename (g_get_user_config_dir (), "lxpanel", session_name, "panels/panel", NULL);
}

static char *pcmanfm_file (void)
{
    const char *session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    return g_build_filename (g_get_user_config_dir (), "pcmanfm", session_name, "desktop-items-0.conf", NULL);
}

static char *libfm_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "libfm/libfm.conf", NULL);
}

static void check_directory (char *path)
{
    char *dir = g_path_get_dirname (path);
    g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (dir);
}

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
    int len = 0;
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

static int read_version (char *package, int *maj, int *min, int *sub)
{
    char *cmd, *res;
    int val;

    cmd = g_strdup_printf ("dpkg -s %s | grep Version | rev | cut -d : -f 1 | rev", package);
    res = get_string (cmd);
    val = sscanf (res, "%d.%d.%d", maj, min, sub);
    g_free (cmd);
    g_free (res);
    return val;
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
    const char *session_name = g_getenv ("DESKTOP_SESSION");
    char *path, *lc_sess, *fname;

    if (!session_name) session_name = DEFAULT_SES;

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

    backup_file (".config/libfm/libfm.conf");
    backup_file (".config/gtk-3.0/gtk.css");
    backup_file (".config/qt5ct/qt5ct.conf");
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
    const char *session_name = g_getenv ("DESKTOP_SESSION");
    char *path, *lc_sess, *fname;
    int changed = 0;

    if (!session_name) session_name = DEFAULT_SES;

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

    if (restore_file (".config/libfm/libfm.conf")) changed = 1;
    if (restore_file (".config/gtk-3.0/gtk.css")) changed = 1;
    if (restore_file (".config/qt5ct/qt5ct.conf")) changed = 1;
    if (restore_file (".gtkrc-2.0")) changed = 1;

    // app-specific
    if (restore_file (".config/lxterminal/lxterminal.conf")) changed = 1;
    if (restore_file (".config/libreoffice/4/user/registrymodifications.xcu")) changed = 1;

    return changed;
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
    const char *session_name = g_getenv ("DESKTOP_SESSION");
    char *path, *lc_sess, *fname;

    if (!session_name) session_name = DEFAULT_SES;

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

    delete_file (".config/libfm/libfm.conf");
    delete_file (".config/gtk-3.0/gtk.css");
    delete_file (".config/qt5ct/qt5ct.conf");
    delete_file (".gtkrc-2.0");
}


/* Functions to load required values from user config files */

static void load_lxsession_settings (void)
{
    char *user_config_file, *ret, *cptr, *nptr;
    GKeyFile *kf;
    GError *err;
    int val;

    user_config_file = lxsession_file ();

    // read in data from file to a key file structure
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        g_key_file_free (kf);
        g_free (user_config_file);
        DEFAULT (bar_colour);
        DEFAULT (bartext_colour);
        DEFAULT (theme_colour);
        DEFAULT (themetext_colour);
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
    ret = g_key_file_get_string (kf, "GTK", "sGtk/ColorScheme", &err);
    if (err == NULL)
    {
        nptr = strtok (ret, ":\n");
        while (nptr)
        {
            cptr = strtok (NULL, ":\n");
            if (!strcmp (nptr, "bar_fg_color"))
            {
                if (!gdk_color_parse (cptr, &cur_conf.bartext_colour))
                    DEFAULT (bartext_colour);
            }
            else if (!strcmp (nptr, "bar_bg_color"))
            {
                if (!gdk_color_parse (cptr, &cur_conf.bar_colour))
                    DEFAULT (bar_colour);
            }
            else if (!strcmp (nptr, "selected_fg_color"))
            {
                if (!gdk_color_parse (cptr, &cur_conf.themetext_colour))
                    DEFAULT (themetext_colour);
            }
            else if (!strcmp (nptr, "selected_bg_color"))
            {
                if (!gdk_color_parse (cptr, &cur_conf.theme_colour))
                    DEFAULT (theme_colour);
            }
            nptr = strtok (NULL, ":\n");
        }
    }
    else
    {
        DEFAULT (bar_colour);
        DEFAULT (bartext_colour);
        DEFAULT (theme_colour);
        DEFAULT (themetext_colour);
    }
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

static void load_pcman_settings (void)
{
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from file to a key file
    user_config_file = pcmanfm_file ();
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_bg", &err);
        if (err == NULL)
        {
            if (!gdk_color_parse (ret, &cur_conf.desktop_colour))
                DEFAULT (desktop_colour);
        }
        else DEFAULT (desktop_colour);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_fg", &err);
        if (err == NULL)
        {
            if (!gdk_color_parse (ret, &cur_conf.desktoptext_colour))
                DEFAULT (desktoptext_colour);
        }
        else DEFAULT (desktoptext_colour);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper", &err);
        if (err == NULL && ret) cur_conf.desktop_picture = g_strdup (ret);
        else DEFAULT (desktop_picture);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper_mode", &err);
        if (err == NULL && ret) cur_conf.desktop_mode = g_strdup (ret);
        else DEFAULT (desktop_mode);
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_documents", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.show_docs = val;
        else DEFAULT (show_docs);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_trash", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.show_trash = val;
        else DEFAULT (show_trash);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_mounts", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.show_mnts = val;
        else DEFAULT (show_mnts);
    }
    else
    {
        DEFAULT (desktop_colour);
        DEFAULT (desktoptext_colour);
        DEFAULT (desktop_picture);
        DEFAULT (desktop_mode);
        DEFAULT (show_docs);
        DEFAULT (show_trash);
        DEFAULT (show_mnts);
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_libfm_settings (void)
{
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from file manager config file
    user_config_file = libfm_file ();
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "big_icon_size", &err);
        if (err == NULL && val >= 8 && val <= 256) cur_conf.folder_size = val;
        else DEFAULT (folder_size);

        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "thumbnail_size", &err);
        if (err == NULL && val >= 8 && val <= 256) cur_conf.thumb_size = val;
        else DEFAULT (thumb_size);

        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "pane_icon_size", &err);
        if (err == NULL && val >= 8 && val <= 256) cur_conf.pane_size = val;
        else DEFAULT (pane_size);

        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "small_icon_size", &err);
        if (err == NULL && val >= 8 && val <= 256) cur_conf.sicon_size = val;
        else DEFAULT (sicon_size);
    }
    else
    {
        DEFAULT (folder_size);
        DEFAULT (thumb_size);
        DEFAULT (pane_size);
        DEFAULT (sicon_size);
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_lxpanel_settings (void)
{
    char *user_config_file, *cmdbuf, *res;
    int val;

    user_config_file = lxpanel_file ();
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        DEFAULT (barpos);
        DEFAULT (icon_size);
        DEFAULT (task_width);
        g_free (user_config_file);
        return;
    }

    if (!vsystem ("grep -q edge=bottom %s", user_config_file)) cur_conf.barpos = 1;
    else DEFAULT (barpos);

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

static void load_lxterm_settings (void)
{
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxterminal/lxterminal.conf", NULL);

    // read in data from file to a key file
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        g_key_file_free (kf);
        g_free (user_config_file);
        DEFAULT (terminal_font);
        return;
    }

    // get data from the key file
    err = NULL;
    ret = g_key_file_get_string (kf, "general", "fontname", &err);
    if (err == NULL) cur_conf.terminal_font = g_strdup (ret);
    else DEFAULT (terminal_font);
    g_free (ret);

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_libreoffice_settings (void)
{
    char *user_config_file;
    int res = 2, val;

    DEFAULT (lo_icon_size);

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "libreoffice/4/user/registrymodifications.xcu", NULL);
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
        // need to create XML doc here, potentially with directory tree...
        g_free (user_config_file);
        return;
    }

    xmlNode *rootnode, *itemnode, *propnode, *valnode;
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext (xDoc);
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='items']", xpathCtx);

    // search nodes in XML for SymbolSet value
    rootnode = xpathObj->nodesetval->nodeTab[0];
    for (itemnode = rootnode->children; itemnode; itemnode = itemnode->next)
    {
        if (itemnode->type == XML_ELEMENT_NODE && !xmlStrcmp (itemnode->name, "item") && !xmlStrcmp (xmlGetProp (itemnode, "path"), "/org.openoffice.Office.Common/Misc"))
        {
            xmlNode *propnode = itemnode->children;
            if (propnode->type == XML_ELEMENT_NODE && !xmlStrcmp (propnode->name, "prop") && !xmlStrcmp (xmlGetProp (propnode, "name"), "SymbolSet"))
            {
                xmlNode *valnode = propnode->children;
                if (valnode->type == XML_ELEMENT_NODE && !xmlStrcmp (valnode->name, "value"))
                {
                    if (sscanf (xmlNodeGetContent (valnode), "%d", &val) == 1)
                    {
                        if (val >= 0 && val <= 2) res = val;
                    }
                }
                break;
            }
        }
    }

    // cleanup XML
    xmlXPathFreeObject (xpathObj);
    xmlXPathFreeContext (xpathCtx);
    xmlSaveFile (user_config_file, xDoc);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_config_file);
    cur_conf.lo_icon_size = res;
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
         if (sscanf (xmlNodeGetContent (node), "%d", &val) == 1 && val > 0) cur_conf.handle_width = val;
    }

    // cleanup XML
    xmlXPathFreeObject (xpathObj);
    xmlXPathFreeContext (xpathCtx);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_config_file);
}

/* Functions to save settings back to relevant files */

static void save_lxpanel_settings (void)
{
    const char *session_name;
    char *user_config_file;

    // sanity check
    if (cur_conf.icon_size > MAX_ICON || cur_conf.icon_size < MIN_ICON) return;

    user_config_file = lxpanel_file ();
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        // need a local copy to take the changes
        check_directory (user_config_file);
        const char *session_name = g_getenv ("DESKTOP_SESSION");
        if (!session_name) session_name = DEFAULT_SES;
        vsystem ("cp /etc/xdg/lxpanel/%s/panels/panel %s", session_name, user_config_file);
    }

    // use sed to write
    vsystem ("sed -i s/iconsize=.*/iconsize=%d/g %s", cur_conf.icon_size, user_config_file);
    vsystem ("sed -i s/height=.*/height=%d/g %s", cur_conf.icon_size, user_config_file);
    vsystem ("sed -i s/edge=.*/edge=%s/g %s", cur_conf.barpos ? "bottom" : "top", user_config_file);
    vsystem ("sed -i s/MaxTaskWidth=.*/MaxTaskWidth=%d/g %s", cur_conf.task_width, user_config_file);

    g_free (user_config_file);
}

static void save_gtk3_settings (void)
{
    char *user_config_file, *cstrb, *cstrf;

    cstrb = gdk_color_to_string (&cur_conf.theme_colour);
    cstrf = gdk_color_to_string (&cur_conf.themetext_colour);

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "gtk-3.0/gtk.css", NULL);
    check_directory (user_config_file);

    // amend entries already in file, or add if not present
    if (vsystem ("grep -q theme_selected_bg_color %s\n", user_config_file))
    {
        vsystem ("echo '@define-color theme_selected_bg_color #%c%c%c%c%c%c;' >> %s",
            cstrb[1], cstrb[2], cstrb[5], cstrb[6], cstrb[9], cstrb[10], user_config_file);
    }
    else
    {
        vsystem ("sed -i s/'theme_selected_bg_color #......'/'theme_selected_bg_color #%c%c%c%c%c%c'/g %s",
            cstrb[1], cstrb[2], cstrb[5], cstrb[6], cstrb[9], cstrb[10], user_config_file);
    }

    if (vsystem ("grep -q theme_selected_fg_color %s\n", user_config_file))
    {
        vsystem ("echo '@define-color theme_selected_fg_color #%c%c%c%c%c%c;' >> %s",
            cstrf[1], cstrf[2], cstrf[5], cstrf[6], cstrf[9], cstrf[10], user_config_file);
    }
    else
    {
        vsystem ("sed -i s/'theme_selected_fg_color #......'/'theme_selected_fg_color #%c%c%c%c%c%c'/g %s",
            cstrf[1], cstrf[2], cstrf[5], cstrf[6], cstrf[9], cstrf[10], user_config_file);
    }

    g_free (cstrf);
    g_free (cstrb);
    g_free (user_config_file);
}

static void save_lxsession_settings (void)
{
    char *user_config_file, *str, *ostr;
    GKeyFile *kf;
    gsize len;
    GError *err;

    user_config_file = lxsession_file ();
    check_directory (user_config_file);

    // read in data from file to a key file
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    // update changed values in the key file
    str = g_strdup_printf ("selected_bg_color:%s\nselected_fg_color:%s\nbar_bg_color:%s\nbar_fg_color:%s\n",
        gdk_color_to_string (&cur_conf.theme_colour), gdk_color_to_string (&cur_conf.themetext_colour),
        gdk_color_to_string (&cur_conf.bar_colour), gdk_color_to_string (&cur_conf.bartext_colour));
    g_key_file_set_string (kf, "GTK", "sGtk/ColorScheme", str);
    g_free (str);

    g_key_file_set_string (kf, "GTK", "sGtk/FontName", cur_conf.desktop_font);
    int tbi = 3;
    if (cur_conf.tb_icon_size == 16) tbi = 1;
    if (cur_conf.tb_icon_size == 48) tbi = 6;
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
}

static void save_pcman_settings (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = pcmanfm_file ();
    check_directory (user_config_file);

    // process pcmanfm config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    str = gdk_color_to_string (&cur_conf.desktop_colour);
    g_key_file_set_string (kf, "*", "desktop_bg", str);
    g_key_file_set_string (kf, "*", "desktop_shadow", str);
    g_free (str);

    str = gdk_color_to_string (&cur_conf.desktoptext_colour);
    g_key_file_set_string (kf, "*", "desktop_fg", str);
    g_free (str);

    g_key_file_set_string (kf, "*", "desktop_font", cur_conf.desktop_font);
    g_key_file_set_string (kf, "*", "wallpaper", cur_conf.desktop_picture);
    g_key_file_set_string (kf, "*", "wallpaper_mode", cur_conf.desktop_mode);
    g_key_file_set_integer (kf, "*", "show_documents", cur_conf.show_docs);
    g_key_file_set_integer (kf, "*", "show_trash", cur_conf.show_trash);
    g_key_file_set_integer (kf, "*", "show_mounts", cur_conf.show_mnts);

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

static void save_greeter_settings (void)
{
    char *str, *tfname, *col;
    GKeyFile *kf;
    GError *err;
    gsize len;
    gint handle;
    int res;
    gboolean changed = FALSE;

    // read the current config
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, GREETER_CONFIG_FILE, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    err = NULL;
    str = g_key_file_get_string (kf, "greeter", "wallpaper", &err);
    if (err != NULL || g_strcmp0 (str, cur_conf.desktop_picture) != 0)
    {
        g_key_file_set_string (kf, "greeter", "wallpaper", cur_conf.desktop_picture);
        changed = TRUE;
    }
    g_free (str);

    err = NULL;
    str = g_key_file_get_string (kf, "greeter", "wallpaper_mode", &err);
    if (err != NULL || g_strcmp0 (str, cur_conf.desktop_mode) != 0)
    {
        g_key_file_set_string (kf, "greeter", "wallpaper_mode", cur_conf.desktop_mode);
        changed = TRUE;
    }
    g_free (str);

    err = NULL;
    str = g_key_file_get_string (kf, "greeter", "gtk-font-name", &err);
    if (err != NULL || g_strcmp0 (str, cur_conf.desktop_font) != 0)
    {
        g_key_file_set_string (kf, "greeter", "gtk-font-name", cur_conf.desktop_font);
        changed = TRUE;
    }
    g_free (str);

    col = gdk_color_to_string (&cur_conf.desktop_colour);
    err = NULL;
    str = g_key_file_get_string (kf, "greeter", "desktop_bg", &err);
    if (err != NULL || g_strcmp0 (str, col) != 0)
    {
        g_key_file_set_string (kf, "greeter", "desktop_bg", col);
        changed = TRUE;
    }
    g_free (str);
    g_free (col);

    if (changed)
    {
        // just in case...
        g_key_file_set_string (kf, "greeter", "gtk-theme-name", "PiX");
        g_key_file_set_string (kf, "greeter", "gtk-icon-theme-name", "PiX");

        // write the modified key file out to a temp file
        str = g_key_file_to_data (kf, &len, NULL);
        handle = g_file_open_tmp ("XXXXXX", &tfname, NULL);
        res = write (handle, str, len);
        close (handle);

        g_free (str);
        g_key_file_free (kf);

        // copy the temp file to the correct place with sudo
        if (res != -1) vsystem ("sudo -A cp %s %s", tfname, GREETER_CONFIG_FILE);
    }
}

static void save_obconf_settings (void)
{
    char *user_config_file, *font, *cptr;
    int count, size;
    const gchar *weight = NULL, *style = NULL;
    char buf[10];

    xmlDocPtr xDoc;
    xmlNodePtr root, theme, cur_node;
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
            cur_node = xmlNewChild (xpathObj->nodesetval->nodeTab[0], NULL, "font", NULL);

            xmlSetProp (cur_node, "place", count == 0 ? "ActiveWindow" : "InactiveWindow");
            sprintf (buf, "%d", size);
            xmlNewChild (cur_node, NULL, "name", font);
            xmlNewChild (cur_node, NULL, "size", buf);
            xmlNewChild (cur_node, NULL, "weight", weight);
            xmlNewChild (cur_node, NULL, "slant", style);
        }
    }
    else
    {
        for (count = 0; count < xpathObj->nodesetval->nodeNr; count++)
        {
            xmlNode *node = xpathObj->nodesetval->nodeTab[count];
            xmlAttr *attr = node->properties;
            cur_node = NULL;
            for (cur_node = node->children; cur_node; cur_node = cur_node->next)
            {
                if (cur_node->type == XML_ELEMENT_NODE)
                {
                    sprintf (buf, "%d", size);
                    if (!strcmp (cur_node->name, "name")) xmlNodeSetContent (cur_node, font);
                    if (!strcmp (cur_node->name, "size")) xmlNodeSetContent (cur_node, buf);
                    if (!strcmp (cur_node->name, "weight")) xmlNodeSetContent (cur_node, weight);
                    if (!strcmp (cur_node->name, "slant"))  xmlNodeSetContent (cur_node, style);
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
        xmlNewChild (cur_node, NULL, "invHandleWidth", buf);
    }
    else
    {
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNodeSetContent (cur_node, buf);
    }

    sprintf (buf, "#%02x%02x%02x", cur_conf.theme_colour.red >> 8, cur_conf.theme_colour.green >> 8, cur_conf.theme_colour.blue >> 8);
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titleColor']", xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNewChild (cur_node, NULL, "titleColor", buf);
    }
    else
    {
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNodeSetContent (cur_node, buf);
    }

    sprintf (buf, "#%02x%02x%02x", cur_conf.themetext_colour.red >> 8, cur_conf.themetext_colour.green >> 8, cur_conf.themetext_colour.blue >> 8);
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='textColor']", xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNewChild (cur_node, NULL, "textColor", buf);
    }
    else
    {
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNodeSetContent (cur_node, buf);
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
        rootnode = xmlNewNode (NULL, (xmlChar *) "oor:items");
        xmlSetProp (rootnode, "xmlns:oor", "http://openoffice.org/2001/registry");
        xmlSetProp (rootnode, "xmlns:xs", "http://www.w3.org/2001/XMLSchema");
        xmlSetProp (rootnode, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
        xmlDocSetRootElement (xDoc, rootnode);
    }
    else rootnode = xpathObj->nodesetval->nodeTab[0];

    for (itemnode = rootnode->children; itemnode; itemnode = itemnode->next)
    {
        if (itemnode->type == XML_ELEMENT_NODE && !xmlStrcmp (itemnode->name, "item") && !xmlStrcmp (xmlGetProp (itemnode, "path"), "/org.openoffice.Office.Common/Misc"))
        {
            xmlNode *propnode = itemnode->children;
            if (propnode->type == XML_ELEMENT_NODE && !xmlStrcmp (propnode->name, "prop") && !xmlStrcmp (xmlGetProp (propnode, "name"), "SymbolSet"))
            {
                xmlNode *valnode = propnode->children;
                if (valnode->type == XML_ELEMENT_NODE && !xmlStrcmp (valnode->name, "value"))
                    xmlNodeSetContent (valnode, buf);
                found = TRUE;
                break;
            }
        }
    }

    // if node not found, add it with desired value
    if (!found)
    {
        itemnode = xmlNewNode (NULL, "item");
        xmlSetProp (itemnode, "oor:path", "/org.openoffice.Office.Common/Misc");
        propnode = xmlNewNode (NULL, "prop");
        xmlSetProp (propnode, "oor:name", "SymbolSet");
        xmlSetProp (propnode, "oor:op", "fuse");
        xmlAddChild (itemnode, propnode);
        valnode = xmlNewNode (NULL, "value");
        xmlNodeSetContent (valnode, buf);
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
    conffile = g_build_filename (g_get_user_config_dir (), "gtk-3.0/gtk.css", NULL);
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
    save_lxpanel_settings ();
    reload_lxpanel ();
}

static void on_theme_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &cur_conf.theme_colour);
    save_lxsession_settings ();
    save_obconf_settings ();
    save_gtk3_settings ();
    reload_lxsession ();
    reload_openbox ();
    reload_pcmanfm ();
}

static void on_themetext_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &cur_conf.themetext_colour);
    save_lxsession_settings ();
    save_obconf_settings ();
    save_gtk3_settings ();
    reload_lxsession ();
    reload_openbox ();
    reload_pcmanfm ();
}

static void on_bar_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &cur_conf.bar_colour);
    save_lxsession_settings ();
    reload_lxsession ();
    reload_pcmanfm ();
}

static void on_bartext_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &cur_conf.bartext_colour);
    save_lxsession_settings ();
    reload_lxsession ();
    reload_pcmanfm ();
}

static void on_desktop_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &cur_conf.desktop_colour);
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_desktoptext_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &cur_conf.desktoptext_colour);
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_desktop_picture_set (GtkFileChooser* btn, gpointer ptr)
{
    char *picture = gtk_file_chooser_get_filename (btn);
    if (picture) cur_conf.desktop_picture = picture;
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_desktop_font_set (GtkFontButton* btn, gpointer ptr)
{
    const char *font = gtk_font_button_get_font_name (btn);
    if (font) cur_conf.desktop_font = font;

    save_lxsession_settings ();
    save_pcman_settings ();
    save_obconf_settings ();
    save_gtk3_settings ();
    save_qt_settings ();

    reload_lxsession ();
    reload_lxpanel ();
    reload_openbox ();
    reload_pcmanfm ();
}

static void on_desktop_mode_set (GtkComboBox* btn, gpointer ptr)
{
    gint val = gtk_combo_box_get_active (btn);
    switch (val)
    {
        case 0 :    cur_conf.desktop_mode = "color";
                    break;
        case 1 :    cur_conf.desktop_mode = "center";
                    break;
        case 2 :    cur_conf.desktop_mode = "fit";
                    break;
        case 3 :    cur_conf.desktop_mode = "crop";
                    break;
        case 4 :    cur_conf.desktop_mode = "stretch";
                    break;
        case 5 :    cur_conf.desktop_mode = "tile";
                    break;
    }

    if (!strcmp (cur_conf.desktop_mode, "color")) gtk_widget_set_sensitive (GTK_WIDGET (dpic), FALSE);
    else gtk_widget_set_sensitive (GTK_WIDGET (dpic), TRUE);
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_bar_pos_set (GtkRadioButton* btn, gpointer ptr)
{
    // only respond to the button which is now active
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

    // find out which button in the group is active
    GSList *group = gtk_radio_button_get_group (btn);
    if (gtk_toggle_button_get_active (group->data)) cur_conf.barpos = 1;
    else cur_conf.barpos = 0;
    save_lxpanel_settings ();
    reload_lxpanel ();
}

static void on_toggle_docs (GtkCheckButton* btn, gpointer ptr)
{
    cur_conf.show_docs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_toggle_trash (GtkCheckButton* btn, gpointer ptr)
{
    cur_conf.show_trash = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_toggle_mnts (GtkCheckButton* btn, gpointer ptr)
{
    cur_conf.show_mnts = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings ();
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
    reload_lxsession ();

    if (cur_conf.cursor_size != orig_cursor_size)
        gtk_widget_show (GTK_WIDGET (cmsg));
    else
        gtk_widget_hide (GTK_WIDGET (cmsg));
}


static void set_controls (void)
{
    // block widget handlers
    g_signal_handler_block (isz, iid);
    g_signal_handler_block (csz, cid);
    g_signal_handler_block (dmod, dmid);
    g_signal_handler_block (rb1, bpid1);
    g_signal_handler_block (rb2, bpid2);
    g_signal_handler_block (cb1, tdid);
    g_signal_handler_block (cb2, ttid);
    g_signal_handler_block (cb3, tmid);

    gtk_font_button_set_font_name (GTK_FONT_BUTTON (font), cur_conf.desktop_font);
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dpic), cur_conf.desktop_picture);
    gtk_widget_set_sensitive (GTK_WIDGET (dpic), TRUE);
    if (!strcmp (cur_conf.desktop_mode, "center")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 1);
    else if (!strcmp (cur_conf.desktop_mode, "fit")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 2);
    else if (!strcmp (cur_conf.desktop_mode, "crop")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 3);
    else if (!strcmp (cur_conf.desktop_mode, "stretch")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 4);
    else if (!strcmp (cur_conf.desktop_mode, "tile")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 5);
    else
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 0);
        gtk_widget_set_sensitive (GTK_WIDGET (dpic), FALSE);
    }
    gtk_color_button_set_color (GTK_COLOR_BUTTON (hcol), &cur_conf.theme_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (dcol), &cur_conf.desktop_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (dtcol), &cur_conf.desktoptext_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (btcol), &cur_conf.bartext_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (bcol), &cur_conf.bar_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (htcol), &cur_conf.themetext_colour);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb1), cur_conf.show_docs);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb2), cur_conf.show_trash);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb3), cur_conf.show_mnts);

    if (cur_conf.icon_size <= 20) gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 3);
    else if (cur_conf.icon_size <= 28) gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 2);
    else if (cur_conf.icon_size <= 36) gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 1);
    else gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 0);

    if (cur_conf.cursor_size >= 48) gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 0);
    else if (cur_conf.cursor_size >= 36) gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 1);
    else gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 2);

    if (cur_conf.barpos) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb2), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb1), TRUE);

    if (cur_conf.cursor_size != orig_cursor_size)
        gtk_widget_show (GTK_WIDGET (cmsg));
    else
        gtk_widget_hide (GTK_WIDGET (cmsg));

    // unblock widget handlers
    g_signal_handler_unblock (isz, iid);
    g_signal_handler_unblock (csz, cid);
    g_signal_handler_unblock (dmod, dmid);
    g_signal_handler_unblock (rb1, bpid1);
    g_signal_handler_unblock (rb2, bpid2);
    g_signal_handler_unblock (cb1, tdid);
    g_signal_handler_unblock (cb2, ttid);
    g_signal_handler_unblock (cb3, tmid);
}

static void on_set_defaults (GtkButton* btn, gpointer ptr)
{
    // clear all the config files
    reset_to_defaults ();

    // set config structure to a default
    if (* (int *) ptr == 3)
        cur_conf = def_lg;
    else if (* (int *) ptr == 1)
        cur_conf = def_sm;
    else cur_conf = def_med;

    // reset the GUI controls to match the variables
    set_controls ();

    // save changes to files if not using medium (the global default)
    if (* (int *) ptr != 2)
    {
        save_lxsession_settings ();
        save_pcman_settings ();
        save_libfm_settings ();
        save_obconf_settings ();
        save_gtk3_settings ();
        save_lxpanel_settings ();
        save_qt_settings ();
        save_scrollbar_settings ();
    }

    // save application-specific config - we don't delete these files first...
    save_lxterm_settings ();
    save_libreoffice_settings ();

    // reload everything to reflect the current state
    reload_lxsession ();
    reload_lxpanel ();
    reload_openbox ();
    reload_pcmanfm ();
}

static void create_defaults (void)
{
    // defaults for controls - medium values must match those in global config files
    // /etc/xdg/lxpanel/LXDE-pi/panels/panel
    def_med.barpos = 0;
    def_med.icon_size = 36;

    // /etc/xdg/lxsession/LXDE-pi/desktop.conf
    def_med.desktop_font = "PibotoLt 12";
    gdk_color_parse ("#4D98F5", &def_med.theme_colour);
    gdk_color_parse ("#FFFFFF", &def_med.themetext_colour);
    gdk_color_parse ("#EDECEB", &def_med.bar_colour);
    gdk_color_parse ("#000000", &def_med.bartext_colour);
    def_med.cursor_size = 24;

    // /etc/xdg/pcmanfm/LXDE-pi/desktop-items-0.conf
    def_med.desktop_picture = "/usr/share/rpd-wallpaper/road.jpg";
    def_med.desktop_mode = "crop";
    gdk_color_parse ("#D6D3DE", &def_med.desktop_colour);
    gdk_color_parse ("#E8E8E8", &def_med.desktoptext_colour);
    def_med.show_docs = 0;
    def_med.show_trash = 1;
    def_med.show_mnts = 0;

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
    def_lg.lo_icon_size = (lo_ver == 6 ? 3 : 1);
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


/* The dialog... */

int main (int argc, char *argv[])
{
    GtkBuilder *builder;
    GObject *item;
    GtkWidget *dlg;
    int maj, min, sub;
    int flag1 = 1, flag2 = 2, flag3 = 3;

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    // check to see if lxsession will auto-refresh - version 0.4.9 or later
    if (read_version ("lxsession", &maj, &min, &sub) != 3) needs_refresh = 1;
    else
    {
        if (min >= 5) needs_refresh = 0;
        else if (min == 4 && sub == 9) needs_refresh = 0;
        else needs_refresh = 1;
    }

    // get libreoffice version
    if (read_version ("libreoffice", &maj, &min, &sub) != 3) lo_ver = 5;
    else lo_ver = maj;

    // load data from config files
    create_defaults ();

    load_lxsession_settings ();
    load_pcman_settings ();
    load_lxpanel_settings ();
    load_obconf_settings ();

    backup_config_files ();

    orig_desktop_font = cur_conf.desktop_font;
    orig_desktop_picture = cur_conf.desktop_picture;
    orig_desktop_mode = cur_conf.desktop_mode;
    orig_desktop_colour = cur_conf.desktop_colour;
    orig_desktoptext_colour = cur_conf.desktoptext_colour;
    orig_cursor_size = cur_conf.cursor_size;

    // GTK setup
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    // build the UI
    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/pipanel.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "dialog1");
    gtk_dialog_set_alternative_button_order (GTK_DIALOG (dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);

    font = gtk_builder_get_object (builder, "fontbutton1");
    g_signal_connect (font, "font-set", G_CALLBACK (on_desktop_font_set), NULL);

    dpic = gtk_builder_get_object (builder, "filechooserbutton1");
    g_signal_connect (dpic, "file-set", G_CALLBACK (on_desktop_picture_set), NULL);

    hcol = gtk_builder_get_object (builder, "colorbutton1");
    g_signal_connect (hcol, "color-set", G_CALLBACK (on_theme_colour_set), NULL);

    dcol = gtk_builder_get_object (builder, "colorbutton2");
    g_signal_connect (dcol, "color-set", G_CALLBACK (on_desktop_colour_set), NULL);

    bcol = gtk_builder_get_object (builder, "colorbutton3");
    g_signal_connect (bcol, "color-set", G_CALLBACK (on_bar_colour_set), NULL);

    btcol = gtk_builder_get_object (builder, "colorbutton4");
    g_signal_connect (btcol, "color-set", G_CALLBACK (on_bartext_colour_set), NULL);

    htcol = gtk_builder_get_object (builder, "colorbutton5");
    g_signal_connect (htcol, "color-set", G_CALLBACK (on_themetext_colour_set), NULL);

    dtcol = gtk_builder_get_object (builder, "colorbutton6");
    g_signal_connect (dtcol, "color-set", G_CALLBACK (on_desktoptext_colour_set), NULL);

    dmod = gtk_builder_get_object (builder, "comboboxtext1");
    dmid = g_signal_connect (dmod, "changed", G_CALLBACK (on_desktop_mode_set), NULL);

    item = gtk_builder_get_object (builder, "defs_lg");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), &flag3);

    item = gtk_builder_get_object (builder, "defs_med");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), &flag2);

    item = gtk_builder_get_object (builder, "defs_sml");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), &flag1);

    rb1 = gtk_builder_get_object (builder, "radiobutton1");
    bpid1 = g_signal_connect (rb1, "toggled", G_CALLBACK (on_bar_pos_set), NULL);

    rb2 = gtk_builder_get_object (builder, "radiobutton2");
    bpid2 = g_signal_connect (rb2, "toggled", G_CALLBACK (on_bar_pos_set), NULL);

    isz = gtk_builder_get_object (builder, "comboboxtext2");
    iid = g_signal_connect (isz, "changed", G_CALLBACK (on_menu_size_set), NULL);

    csz = gtk_builder_get_object (builder, "comboboxtext3");
    cid = g_signal_connect (csz, "changed", G_CALLBACK (on_cursor_size_set), NULL);

    cb1 = gtk_builder_get_object (builder, "checkbutton1");
    tdid = g_signal_connect (cb1, "toggled", G_CALLBACK (on_toggle_docs), NULL);

    cb2 = gtk_builder_get_object (builder, "checkbutton2");
    ttid = g_signal_connect (cb2, "toggled", G_CALLBACK (on_toggle_trash), NULL);

    cb3 = gtk_builder_get_object (builder, "checkbutton3");
    tmid = g_signal_connect (cb3, "toggled", G_CALLBACK (on_toggle_mnts), NULL);

    cmsg = gtk_builder_get_object (builder, "label35");

    g_object_unref (builder);

    set_controls ();

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_CANCEL)
    {
        if (restore_config_files ())
        {
            reload_lxsession ();
            reload_lxpanel ();
            reload_openbox ();
            reload_pcmanfm ();
        }
    }
    else save_greeter_settings ();
    gtk_widget_destroy (dlg);

    return 0;
}
