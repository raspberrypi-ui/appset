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

#define THEME_COL "#4D98F5"
#define TEXT_COL "#FFFFFF"

/* Global variables for window values */

static const char *desktop_font, *orig_desktop_font;
static const char *desktop_picture, *orig_desktop_picture;
static const char *desktop_mode, *orig_desktop_mode;
static const char *orig_lxsession_theme;
static const char *orig_openbox_theme;
static const char *terminal_font;
static int icon_size;
static GdkColor theme_colour;
static GdkColor themetext_colour;
static GdkColor desktop_colour, orig_desktop_colour;
static GdkColor desktoptext_colour, orig_desktoptext_colour;
static GdkColor bar_colour;
static GdkColor bartext_colour;
static int barpos;
static int show_docs;
static int show_trash;
static int show_mnts;
static int folder_size;
static int thumb_size;
static int pane_size;
static int sicon_size;
static int tb_icon_size;
static int lo_icon_size;
static int cursor_size, orig_cursor_size;
static int task_width;
static int handle_width;
static int scrollbar_width;

/* Flag to indicate whether lxsession is version 4.9 or later, in which case no need to refresh manually */

static gboolean needs_refresh;

/* Version of Libreoffice installed - affects toolbar icon setting */

static char lo_ver;

/* Controls */
static GObject *hcol, *htcol, *font, *dcol, *dtcol, *dmod, *dpic, *barh, *bcol, *btcol, *rb1, *rb2, *isz, *cb1, *cb2, *cb3, *csz, *cmsg;

static void backup_file (char *filepath);
static void backup_config_files (void);
static void restore_file (char *filepath);
static void restore_config_files (void);
static void delete_file (char *filepath);
static void reset_to_defaults (void);
static void load_lxsession_settings (void);
static void load_pcman_settings (void);
static void load_lxpanel_settings (void);
static void load_lxterm_settings (void);
static void load_libreoffice_settings (void);
static void load_obconf_settings (void);
static void save_lxpanel_settings (void);
static void save_gtk3_settings (void);
static void save_lxsession_settings (void);
static void save_pcman_settings (void);
static void save_obconf_settings (void);
static void save_lxterm_settings (void);
static void save_greeter_settings (void);
static void save_libreoffice_settings (void);
static void save_qt_settings (void);
static void add_or_amend (const char *conffile, const char *block, const char *param, const char *repl);
static void save_scrollbar_settings (void);
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
    char *dir = g_path_get_dirname (backup);

    if (g_file_test (orig, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("mkdir -p %s", dir);
        vsystem ("cp %s %s", orig, backup);
    }
    g_free (dir);
    g_free (backup);
    g_free (orig);
}

static void backup_config_files (void)
{
    const char *session_name = g_getenv ("DESKTOP_SESSION");
    char *backupd, *path, *lc_sess, *fname;

    if (!session_name) session_name = DEFAULT_SES;
    backupd = g_build_filename (g_get_home_dir (), ".pp_backup", NULL);

    // delete any old backups
    if (g_file_test (backupd, G_FILE_TEST_IS_DIR)) vsystem ("rm -rf %s", backupd);

    // create the backup directory
    vsystem ("mkdir -p %s", backupd);

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
    backup_file (".config/lxterminal/lxterminal.conf");
    backup_file (".config/libreoffice/4/user/registrymodifications.xcu");
    backup_file (".gtkrc-2.0");
}

static void restore_file (char *filepath)
{
    // filepath must be relative to current user's home directory
    char *orig = g_build_filename (g_get_home_dir (), filepath, NULL);
    char *backup = g_build_filename (g_get_home_dir (), ".pp_backup", filepath, NULL);

    if (g_file_test (backup, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("cp %s %s", backup, orig);
    }
    else if (g_file_test (orig, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("rm %s", orig);
    }
    g_free (backup);
    g_free (orig);
}

static void restore_config_files (void)
{
    const char *session_name = g_getenv ("DESKTOP_SESSION");
    char *path, *lc_sess, *fname;

    if (!session_name) session_name = DEFAULT_SES;

    lc_sess = g_ascii_strdown (session_name, -1);
    fname = g_strconcat (lc_sess, "-rc.xml", NULL);
    path = g_build_filename (".config/openbox", fname, NULL);
    restore_file (path);
    g_free (path);
    g_free (fname);
    g_free (lc_sess);

    path = g_build_filename (".config/lxsession", session_name, "desktop.conf", NULL);
    restore_file (path);
    g_free (path);

    path = g_build_filename (".config/lxpanel", session_name, "panels/panel", NULL);
    restore_file (path);
    g_free (path);

    path = g_build_filename (".config/pcmanfm", session_name, "desktop-items-0.conf", NULL);
    restore_file (path);
    g_free (path);

    restore_file (".config/libfm/libfm.conf");
    restore_file (".config/gtk-3.0/gtk.css");
    restore_file (".config/qt5ct/qt5ct.conf");
    restore_file (".config/lxterminal/lxterminal.conf");
    restore_file (".config/libreoffice/4/user/registrymodifications.xcu");
    restore_file (".gtkrc-2.0");
}

static void delete_file (char *filepath)
{
    char *orig = g_build_filename (g_get_home_dir (), filepath, NULL);

    if (g_file_test (orig, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("rm %s", orig);
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
    delete_file (".config/lxterminal/lxterminal.conf");
    delete_file (".config/libreoffice/4/user/registrymodifications.xcu");
    delete_file (".gtkrc-2.0");

    // copy in a clean version of the QT config file, which annoyingly has no global version...
    vsystem ("cp /usr/share/raspi-ui-overrides/qt5ct.conf %s/.config/qt5ct/qt5ct.conf", g_get_home_dir ());
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
        gdk_color_parse ("#EDECEB", &bar_colour);
        gdk_color_parse ("#000000", &bartext_colour);
        gdk_color_parse (THEME_COL, &theme_colour);
        gdk_color_parse (TEXT_COL, &themetext_colour);
        desktop_font = "PibotoLt 12";
        tb_icon_size = 24;
        cursor_size = 24;
        return;
    }

    // get data from the key file
    err = NULL;
    ret = g_key_file_get_string (kf, "GTK", "sGtk/FontName", &err);
    if (err == NULL) desktop_font = g_strdup (ret);
    else desktop_font = "PibotoLt 12";
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
                if (!gdk_color_parse (cptr, &bartext_colour))
                    gdk_color_parse ("#000000", &bartext_colour);
            }
            else if (!strcmp (nptr, "bar_bg_color"))
            {
                if (!gdk_color_parse (cptr, &bar_colour))
                    gdk_color_parse ("#EDECEB", &bar_colour);
            }
            else if (!strcmp (nptr, "selected_fg_color"))
            {
                if (!gdk_color_parse (cptr, &themetext_colour))
                    gdk_color_parse (TEXT_COL, &themetext_colour);
            }
            else if (!strcmp (nptr, "selected_bg_color"))
            {
                if (!gdk_color_parse (cptr, &theme_colour))
                    gdk_color_parse (THEME_COL, &theme_colour);
            }
            nptr = strtok (NULL, ":\n");
        }
    }
    else
    {
        gdk_color_parse ("#000000", &bartext_colour);
        gdk_color_parse ("#EDECEB", &bar_colour);
        gdk_color_parse (THEME_COL, &theme_colour);
        gdk_color_parse (TEXT_COL, &themetext_colour);
    }
    g_free (ret);

    err = NULL;
    ret = g_key_file_get_string (kf, "GTK", "sGtk/IconSizes", &err);
    tb_icon_size = 24;
    if (err == NULL)
    {
        if (sscanf (ret, "gtk-large-toolbar=%d,", &val) == 1)
        {
            if (val >= 8 && val <= 256) tb_icon_size = val;
        }
    }
    g_free (ret);

    err = NULL;
    val = g_key_file_get_integer (kf, "GTK", "iGtk/CursorThemeSize", &err);
    if (err == NULL && val >= 24 && val <= 48) cursor_size = val;
    else cursor_size = 24;

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
            if (!gdk_color_parse (ret, &desktop_colour))
                gdk_color_parse ("#D6D3DE", &desktop_colour);
        }
        else gdk_color_parse ("#D6D3DE", &desktop_colour);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_fg", &err);
        if (err == NULL)
        {
            if (!gdk_color_parse (ret, &desktoptext_colour))
                gdk_color_parse ("#000000", &desktoptext_colour);
        }
        else gdk_color_parse ("#000000", &desktoptext_colour);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper", &err);
        if (err == NULL && ret) desktop_picture = g_strdup (ret);
        else desktop_picture = "/usr/share/rpd-wallpaper/road.jpg";
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper_mode", &err);
        if (err == NULL && ret) desktop_mode = g_strdup (ret);
        else desktop_mode = "crop";
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_documents", &err);
        if (err == NULL && val >= 0 && val <= 1) show_docs = val;
        else show_docs = 0;

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_trash", &err);
        if (err == NULL && val >= 0 && val <= 1) show_trash = val;
        else show_trash = 1;

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_mounts", &err);
        if (err == NULL && val >= 0 && val <= 1) show_mnts = val;
        else show_mnts = 0;
    }
    else
    {
        gdk_color_parse ("#D6D3DE", &desktop_colour);
        gdk_color_parse ("#000000", &desktoptext_colour);
        desktop_picture = "/usr/share/rpd-wallpaper/road.jpg";
        desktop_mode = "crop";
        show_docs = 0;
        show_trash = 1;
        show_mnts = 0;
    }
    g_key_file_free (kf);
    g_free (user_config_file);

    // read in data from file manager config file
    user_config_file = libfm_file ();
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "big_icon_size", &err);
        if (err == NULL && val >= 8 && val <= 256) folder_size = val;
        else folder_size = 48;

        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "thumbnail_size", &err);
        if (err == NULL && val >= 8 && val <= 256) thumb_size = val;
        else thumb_size = 128;

        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "pane_icon_size", &err);
        if (err == NULL && val >= 8 && val <= 256) pane_size = val;
        else pane_size = 24;

        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "small_icon_size", &err);
        if (err == NULL && val >= 8 && val <= 256) sicon_size = val;
        else sicon_size = 24;
    }
    else
    {
        folder_size = 48;
        thumb_size = 128;
        pane_size = 24;
        sicon_size = 24;
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_lxpanel_settings (void)
{
    char *user_config_file, *cmdbuf, *res;
    int val;

    user_config_file = lxpanel_file ();

    if (!vsystem ("grep -q edge=bottom %s", user_config_file)) barpos = 1;
    else barpos = 0;

    cmdbuf = g_strdup_printf ("grep -Po '(?<=iconsize=)[0-9]+' %s", user_config_file);
    res = get_string (cmdbuf);
    if (res[0] && sscanf (res, "%d", &val) == 1) icon_size = val;
    else icon_size = 36;
    g_free (res);
    g_free (cmdbuf);

    cmdbuf = g_strdup_printf ("grep -Po '(?<=MaxTaskWidth=)[0-9]+' %s", user_config_file);
    res = get_string (cmdbuf);
    if (res[0] && sscanf (res, "%d", &val) == 1) task_width = val;
    else task_width = 200;
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
        terminal_font = "Monospace 10";
        return;
    }

    // get data from the key file
    err = NULL;
    ret = g_key_file_get_string (kf, "general", "fontname", &err);
    if (err == NULL) terminal_font = g_strdup (ret);
    else terminal_font = "Monospace 10";
    g_free (ret);

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_libreoffice_settings (void)
{
    char *user_config_file;
    int res = 2, val;

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "libreoffice/4/user/registrymodifications.xcu", NULL);

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    xmlDocPtr xDoc = xmlParseFile (user_config_file);
    if (xDoc == NULL)
    {
        // need to create XML doc here, potentially with directory tree...
        g_free (user_config_file);
        lo_icon_size = 1;
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
    lo_icon_size = res;
}

static void load_obconf_settings (void)
{
    char *user_config_file;
    int val;

    handle_width = 10;

    user_config_file = openbox_file ();

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
         if (sscanf (xmlNodeGetContent (node), "%d", &val) == 1 && val > 0) handle_width = val;
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
    if (icon_size > MAX_ICON || icon_size < MIN_ICON) return;

    user_config_file = lxpanel_file ();
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        // need a local copy to take the changes
        check_directory (user_config_file);
        const char *session_name = g_getenv ("DESKTOP_SESSION");
        if (!session_name) session_name = DEFAULT_SES;
        vsystem ("cp /usr/share/lxpanel/profile/%s/panels/panel %s", session_name, user_config_file);
    }

    // use sed to write
    vsystem ("sed -i s/iconsize=.*/iconsize=%d/g %s", icon_size, user_config_file);
    vsystem ("sed -i s/height=.*/height=%d/g %s", icon_size, user_config_file);
    vsystem ("sed -i s/edge=.*/edge=%s/g %s", barpos ? "bottom" : "top", user_config_file);
    vsystem ("sed -i s/MaxTaskWidth=.*/MaxTaskWidth=%d/g %s", task_width, user_config_file);

    g_free (user_config_file);
}

static void save_gtk3_settings (void)
{
    char *user_config_file, *cstrb, *cstrf;

    cstrb = gdk_color_to_string (&theme_colour);
    cstrf = gdk_color_to_string (&themetext_colour);

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "gtk-3.0/gtk.css", NULL);
    check_directory (user_config_file);

    // check if the file exists - if not, create it...
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("echo '@define-color theme_selected_bg_color #%c%c%c%c%c%c;\n@define-color theme_selected_fg_color #%c%c%c%c%c%c;' > %s", 
            cstrb[1], cstrb[2], cstrb[5], cstrb[6], cstrb[9], cstrb[10],
            cstrf[1], cstrf[2], cstrf[5], cstrf[6], cstrf[9], cstrf[10], user_config_file);
    }
    else
    {
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
    }

    g_free (cstrf);
    g_free (cstrb);
    g_free (user_config_file);
}

static void save_lxsession_settings (void)
{
    char *user_config_file, *str;
    char colbuf[256];
    GKeyFile *kf;
    gsize len;
    GError *err;

    user_config_file = lxsession_file ();
    check_directory (user_config_file);

    // read in data from file to a key file
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    // update changed values in the key file
    sprintf (colbuf, "selected_bg_color:%s\nselected_fg_color:%s\nbar_bg_color:%s\nbar_fg_color:%s\n",
        gdk_color_to_string (&theme_colour), gdk_color_to_string (&themetext_colour),
        gdk_color_to_string (&bar_colour), gdk_color_to_string (&bartext_colour));
    g_key_file_set_string (kf, "GTK", "sGtk/ColorScheme", colbuf);
    g_key_file_set_string (kf, "GTK", "sGtk/FontName", desktop_font);
    int tbi = 3;
    if (tb_icon_size == 16) tbi = 1;
    if (tb_icon_size == 48) tbi = 6;
    g_key_file_set_integer (kf, "GTK", "iGtk/ToolbarIconSize", tbi);

    err = NULL;
    str = g_key_file_get_string (kf, "GTK", "sGtk/IconSizes", &err);
    if (err == NULL && str)
    {
        if (strstr (str, "gtk-large-toolbar"))
        {
            // replace values in existing string
            colbuf[0] = 0;
            char *cptr = strtok (str, ":");
            while (cptr)
            {
                if (colbuf[0] != 0) strcat (colbuf, ":");
                if (strstr (cptr, "gtk-large-toolbar"))
                {
                    char *nstr = g_strdup_printf ("gtk-large-toolbar=%d,%d", tb_icon_size, tb_icon_size);
                    strcat (colbuf, nstr);
                    g_free (nstr);
                }
                else strcat (colbuf, cptr);

                cptr = strtok (NULL, ":");
            }
            g_key_file_set_string (kf, "GTK", "sGtk/IconSizes", colbuf);
        }
        else
        {
            // append this element to existing string
            sprintf (colbuf, "%s:gtk-large-toolbar=%d,%d", str, tb_icon_size, tb_icon_size);
            g_key_file_set_string (kf, "GTK", "sGtk/IconSizes", colbuf);
        }
    }
    else
    {
        // new string with just this element
        sprintf (colbuf, "gtk-large-toolbar=%d,%d", tb_icon_size, tb_icon_size);
        g_key_file_set_string (kf, "GTK", "sGtk/IconSizes", colbuf);
    }
    g_free (str);

    g_key_file_set_integer (kf, "GTK", "iGtk/CursorThemeSize", cursor_size);

    // set the lxsession defaults - needed if this is a new file
    g_key_file_set_string (kf, "Session", "window_manager", "openbox");
    g_key_file_set_string (kf, "Environment", "menu_prefix", "lxde-pi-");
    g_key_file_set_string (kf, "GTK", "sNet/ThemeName", "PiX");
    g_key_file_set_string (kf, "GTK", "sNet/IconThemeName", "PiX");
    g_key_file_set_string (kf, "GTK", "sGtk/CursorThemeName", "PiX");
    g_key_file_set_integer (kf, "GTK", "iGtk/ButtonImages", 0);
    g_key_file_set_integer (kf, "GTK", "iGtk/MenuImages", 0);
    g_key_file_set_integer (kf, "GTK", "iGtk/AutoMnemonics", 1);
    g_key_file_set_integer (kf, "GTK", "iGtk/EnableMnemonics", 1);

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
    char colbuf[32];
    GKeyFile *kf;
    gsize len;

    user_config_file = pcmanfm_file ();
    check_directory (user_config_file);

    // process pcmanfm config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    sprintf (colbuf, "%s", gdk_color_to_string (&desktop_colour));
    g_key_file_set_string (kf, "*", "desktop_bg", colbuf);
    g_key_file_set_string (kf, "*", "desktop_shadow", colbuf);
    sprintf (colbuf, "%s", gdk_color_to_string (&desktoptext_colour));
    g_key_file_set_string (kf, "*", "desktop_fg", colbuf);
    g_key_file_set_string (kf, "*", "desktop_font", desktop_font);
    g_key_file_set_string (kf, "*", "wallpaper", desktop_picture);
    g_key_file_set_string (kf, "*", "wallpaper_mode", desktop_mode);
    g_key_file_set_integer (kf, "*", "show_documents", show_docs);
    g_key_file_set_integer (kf, "*", "show_trash", show_trash);
    g_key_file_set_integer (kf, "*", "show_mounts", show_mnts);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);

    // process libfm config data
    user_config_file = libfm_file ();
    check_directory (user_config_file);

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_integer (kf, "ui", "big_icon_size", folder_size);
    g_key_file_set_integer (kf, "ui", "thumbnail_size", thumb_size);
    g_key_file_set_integer (kf, "ui", "pane_icon_size", pane_size);
    g_key_file_set_integer (kf, "ui", "small_icon_size", sicon_size);

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

    // read in data from file to a key file
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        g_key_file_free (kf);
        g_free (user_config_file);
        return;
    }

    // update changed values in the key file
    g_key_file_set_string (kf, "general", "fontname", terminal_font);

    // write the modified key file out
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_greeter_settings (void)
{
    char *str, *tfname;
    char buffer[256];
    GKeyFile *kf;
    gsize len;
    gint handle;
    int res;

    if (desktop_font != orig_desktop_font || desktop_picture != orig_desktop_picture ||
        desktop_mode != orig_desktop_mode || !gdk_color_equal (&desktop_colour, &orig_desktop_colour))
    {
        // read in data from file to a key file
        kf = g_key_file_new ();
        if (!g_key_file_load_from_file (kf, GREETER_CONFIG_FILE, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
        {
            g_key_file_free (kf);
            return;
        }

        // update changed values in the key file
        sprintf (buffer, "%s", gdk_color_to_string (&desktop_colour));
        g_key_file_set_string (kf, "greeter", "desktop_bg", buffer);
        g_key_file_set_string (kf, "greeter", "wallpaper", desktop_picture);
        g_key_file_set_string (kf, "greeter", "wallpaper_mode", desktop_mode);
        g_key_file_set_string (kf, "greeter", "gtk-font-name", desktop_font);
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
    xmlNode *cur_node;
    xmlNodePtr root, theme;
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;

    user_config_file = openbox_file ();
    check_directory (user_config_file);

    // set the font description variables for XML from the font name
    PangoFontDescription *pfd = pango_font_description_from_string (desktop_font);
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
    xDoc = xmlParseFile (user_config_file);
    if (!xDoc) xDoc = xmlNewDoc ((xmlChar *) "1.0");
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

    sprintf (buf, "%d", handle_width);
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

    sprintf (buf, "#%02x%02x%02x", theme_colour.red >> 8, theme_colour.green >> 8, theme_colour.blue >> 8);
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

    sprintf (buf, "#%02x%02x%02x", themetext_colour.red >> 8, themetext_colour.green >> 8, themetext_colour.blue >> 8);
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

    sprintf (buf, "%d", lo_icon_size);

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "libreoffice/4/user/registrymodifications.xcu", NULL);

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    xmlDocPtr xDoc = xmlParseFile (user_config_file);
    if (xDoc == NULL)
    {
        // check and create directory tree
        struct stat attr;
        gchar *user_config_dir = g_build_filename (g_get_user_config_dir (), "libreoffice/4/user/", NULL);
        if (stat (user_config_dir, &attr) == -1) g_mkdir_with_parents (user_config_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        g_free (user_config_dir);

        // create XML doc
        vsystem ("echo '<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<oor:items xmlns:oor=\"http://openoffice.org/2001/registry\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n<item oor:path=\"/org.openoffice.Office.Common/Misc\"><prop oor:name=\"SymbolSet\" oor:op=\"fuse\"><value>%d</value></prop></item>\n</oor:items>\n' > %s", lo_icon_size, user_config_file);
        return;
    }

    xmlNode *rootnode, *itemnode, *propnode, *valnode;
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext (xDoc);
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='items']", xpathCtx);

    // search nodes in XML for SymbolSet value and modify it if found
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

    // read in data from file to a key file
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        g_key_file_free (kf);
        g_free (user_config_file);
        return;
    }

    // create the Qt font representation
    PangoFontDescription *pfd = pango_font_description_from_string (desktop_font);
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

    // check the block exists - add an empty block if not
    if (vsystem ("cat %s | tr -d '\\n' | grep -q '%s\\s*{.*}'", conffile, block_ws))
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
    repl = g_strdup_printf ("GtkRange::slider-width = %d", scrollbar_width);
    add_or_amend (conffile, "style \"scrollbar\"", "GtkRange::slider-width\\s*=\\s*[0-9]*", repl);
    g_free (repl);

    repl = g_strdup_printf ("GtkRange::stepper-size = %d", scrollbar_width);
    add_or_amend (conffile, "style \"scrollbar\"", "GtkRange::stepper-size\\s*=\\s*[0-9]*", repl);
    g_free (repl);

    g_free (conffile);

    // GTK3 override file
    conffile = g_build_filename (g_get_user_config_dir (), "gtk-3.0/gtk.css", NULL);

    // check if the scrollbar button entry is in the file - if not, add it...
    repl = g_strdup_printf ("min-width: %dpx;", scrollbar_width - 6);
    add_or_amend (conffile, "scrollbar button", "min-width:\\s*[0-9]*px;", repl);
    g_free (repl);

    repl = g_strdup_printf ("min-height: %dpx;", scrollbar_width - 6);
    add_or_amend (conffile, "scrollbar button", "min-height:\\s*[0-9]*px;", repl);
    g_free (repl);

    // check if the scrollbar slider entry is in the file - if not, add it...
    repl = g_strdup_printf ("min-width: %dpx;", scrollbar_width - 6);
    add_or_amend (conffile, "scrollbar slider", "min-width:\\s*[0-9]*px;", repl);
    g_free (repl);

    repl = g_strdup_printf ("min-height: %dpx;", scrollbar_width - 6);
    add_or_amend (conffile, "scrollbar slider", "min-height:\\s*[0-9]*px;", repl);
    g_free (repl);

    // process active scrollbar button icons
    int i;
    const char *dl[4] = { "d", "u", "r", "l" };
    for (i = 0; i < 4; i++)
    {
        block = g_strdup_printf ("scrollbar.%s button.%s", i < 2 ? "vertical" : "horizontal", i % 2 ? "up" : "down");
        repl = g_strdup_printf ("-gtk-icon-source: -gtk-icontheme(\"%sscroll_%s\");", scrollbar_width >= 18 ? "l" : "", dl[i]);

        add_or_amend (conffile, block, "-gtk-icon-source:.*;", repl);
        g_free (repl);
        g_free (block);
    }

    for (i = 0; i < 4; i++)
    {
        block = g_strdup_printf ("scrollbar.%s button:disabled.%s", i < 2 ? "vertical" : "horizontal", i % 2 ? "up" : "down");
        repl = g_strdup_printf ("-gtk-icon-source: -gtk-icontheme(\"%sscroll_%s_d\");", scrollbar_width >= 18 ? "l" : "", dl[i]);

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
        case 0 :    icon_size = 52;
                    break;
        case 1 :    icon_size = 36;
                    break;
        case 2 :    icon_size = 28;
                    break;
        case 3 :    icon_size = 20;
                    break;
    }
    save_lxpanel_settings ();
    reload_lxpanel ();
}

static void on_theme_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &theme_colour);
    save_lxsession_settings ();
    save_obconf_settings ();
    save_gtk3_settings ();
    reload_lxsession ();
    reload_openbox ();
    reload_pcmanfm ();
}

static void on_themetext_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &themetext_colour);
    save_lxsession_settings ();
    save_obconf_settings ();
    save_gtk3_settings ();
    reload_lxsession ();
    reload_openbox ();
    reload_pcmanfm ();
}

static void on_bar_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &bar_colour);
    save_lxsession_settings ();
    reload_lxsession ();
    reload_pcmanfm ();
}

static void on_bartext_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &bartext_colour);
    save_lxsession_settings ();
    reload_lxsession ();
    reload_pcmanfm ();
}

static void on_desktop_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &desktop_colour);
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_desktoptext_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &desktoptext_colour);
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_desktop_picture_set (GtkFileChooser* btn, gpointer ptr)
{
    char *picture = gtk_file_chooser_get_filename (btn);
    if (picture) desktop_picture = picture;
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_desktop_font_set (GtkFontButton* btn, gpointer ptr)
{
    const char *font = gtk_font_button_get_font_name (btn);
    if (font) desktop_font = font;

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
        case 0 :    desktop_mode = "color";
                    break;
        case 1 :    desktop_mode = "center";
                    break;
        case 2 :    desktop_mode = "fit";
                    break;
        case 3 :    desktop_mode = "crop";
                    break;
        case 4 :    desktop_mode = "stretch";
                    break;
        case 5 :    desktop_mode = "tile";
                    break;
    }

    if (!strcmp (desktop_mode, "color")) gtk_widget_set_sensitive (GTK_WIDGET (ptr), FALSE);
    else gtk_widget_set_sensitive (GTK_WIDGET (ptr), TRUE);
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_bar_pos_set (GtkRadioButton* btn, gpointer ptr)
{
    // only respond to the button which is now active
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

    // find out which button in the group is active
    GSList *group = gtk_radio_button_get_group (btn);
    if (gtk_toggle_button_get_active (group->data)) barpos = 1;
    else barpos = 0;
    save_lxpanel_settings ();
    reload_lxpanel ();
}

static void on_toggle_docs (GtkCheckButton* btn, gpointer ptr)
{
    show_docs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_toggle_trash (GtkCheckButton* btn, gpointer ptr)
{
    show_trash = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_toggle_mnts (GtkCheckButton* btn, gpointer ptr)
{
    show_mnts = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings ();
    reload_pcmanfm ();
}

static void on_cursor_size_set (GtkComboBox* btn, gpointer ptr)
{
    gint val = gtk_combo_box_get_active (btn);
    switch (val)
    {
        case 0 :    cursor_size = 48;
                    break;
        case 1 :    cursor_size = 36;
                    break;
        case 2 :    cursor_size = 24;
                    break;
    }
    save_lxsession_settings ();
    reload_lxsession ();

    if (cursor_size != orig_cursor_size)
        gtk_widget_show (GTK_WIDGET (cmsg));
    else
        gtk_widget_hide (GTK_WIDGET (cmsg));
}

static void on_set_defaults (GtkButton* btn, gpointer ptr)
{
    // clear all the config files
    reset_to_defaults ();

    // reset all the variables for current values
    if (* (int *) ptr == 3)
    {
        desktop_font = "PibotoLt 16";
        terminal_font = "Monospace 15";
        icon_size = 52;
        folder_size = 80;
        thumb_size = 160;
        pane_size = 32;
        sicon_size = 32;
        tb_icon_size = 48;
        if (lo_ver == 6) lo_icon_size = 3;
        else lo_icon_size = 1;
        cursor_size = 36;
        task_width = 300;
        handle_width = 20;
        scrollbar_width = 18;
        gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 0);
        gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 1);
    }
    else if (* (int *) ptr == 2)
    {
        desktop_font = "PibotoLt 12";
        terminal_font = "Monospace 10";
        icon_size = 36;
        folder_size = 48;
        thumb_size = 128;
        pane_size = 24;
        sicon_size = 24;
        tb_icon_size = 24;
        lo_icon_size = 1;
        cursor_size = 24;
        task_width = 200;
        handle_width = 10;
        scrollbar_width = 13;
        gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 1);
        gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 2);
    }
    else if (* (int *) ptr == 1)
    {
        desktop_font = "PibotoLt 8";
        terminal_font = "Monospace 8";
        icon_size = 20;
        folder_size = 32;
        thumb_size = 64;
        pane_size = 16;
        sicon_size = 16;
        tb_icon_size = 16;
        lo_icon_size = 0;
        cursor_size = 24;
        task_width = 150;
        handle_width = 10;
        scrollbar_width = 13;
        gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 3);
        gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 2);
    }

    // reset the GUI controls to match the variables
    if (cursor_size != orig_cursor_size)
        gtk_widget_show (GTK_WIDGET (cmsg));
    else
        gtk_widget_hide (GTK_WIDGET (cmsg));
    gtk_font_button_set_font_name (GTK_FONT_BUTTON (font), desktop_font);
    desktop_picture = "/usr/share/rpd-wallpaper/road.jpg";
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dpic), desktop_picture);
    desktop_mode = "crop";
    gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 3);
    gdk_color_parse (THEME_COL, &theme_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (hcol), &theme_colour);
    gdk_color_parse ("#D6D3DE", &desktop_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (dcol), &desktop_colour);
    gdk_color_parse ("#E8E8E8", &desktoptext_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (dtcol), &desktoptext_colour);
    gdk_color_parse ("#000000", &bartext_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (btcol), &bartext_colour);
    gdk_color_parse ("#EDECEB", &bar_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (bcol), &bar_colour);
    gdk_color_parse (TEXT_COL, &themetext_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (htcol), &themetext_colour);
    barpos = 0;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb1), TRUE);
    show_docs = 0;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb1), show_docs);
    show_trash = 1;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb2), show_trash);
    show_mnts = 0;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb3), show_mnts);

    // save changes to files if not using medium (the global default)
    if (* (int *) ptr != 2)
    {
        save_lxsession_settings ();
        save_pcman_settings ();
        save_obconf_settings ();
        save_gtk3_settings ();
        save_lxpanel_settings ();
        save_lxterm_settings ();
        save_libreoffice_settings ();
        save_qt_settings ();
        save_scrollbar_settings ();
    }

    // reload everything to reflect the current state
    reload_lxsession ();
    reload_lxpanel ();
    reload_openbox ();
    reload_pcmanfm ();
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
    load_lxsession_settings ();
    load_pcman_settings ();
    load_lxpanel_settings ();
    load_lxterm_settings ();
    load_libreoffice_settings ();
    load_obconf_settings ();
    backup_config_files ();
    orig_desktop_font = desktop_font;
    orig_desktop_picture = desktop_picture;
    orig_desktop_mode = desktop_mode;
    orig_desktop_colour = desktop_colour;
    orig_desktoptext_colour = desktoptext_colour;
    orig_cursor_size = cursor_size;

    // GTK setup
    gtk_init (&argc, &argv);
    gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    // build the UI
    builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, PACKAGE_DATA_DIR "/pipanel.ui", NULL);
    dlg = (GtkWidget *) gtk_builder_get_object (builder, "dialog1");
    gtk_dialog_set_alternative_button_order (GTK_DIALOG (dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);

    font = gtk_builder_get_object (builder, "fontbutton1");
    gtk_font_button_set_font_name (GTK_FONT_BUTTON (font), desktop_font);
    g_signal_connect (font, "font-set", G_CALLBACK (on_desktop_font_set), NULL);

    dpic = gtk_builder_get_object (builder, "filechooserbutton1");
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dpic), desktop_picture);
    g_signal_connect (dpic, "file-set", G_CALLBACK (on_desktop_picture_set), NULL);
    if (!strcmp (desktop_mode, "color")) gtk_widget_set_sensitive (GTK_WIDGET (dpic), FALSE);
    else gtk_widget_set_sensitive (GTK_WIDGET (dpic), TRUE);

    hcol = gtk_builder_get_object (builder, "colorbutton1");
    gtk_color_button_set_color (GTK_COLOR_BUTTON (hcol), &theme_colour);
    g_signal_connect (hcol, "color-set", G_CALLBACK (on_theme_colour_set), NULL);

    dcol = gtk_builder_get_object (builder, "colorbutton2");
    gtk_color_button_set_color (GTK_COLOR_BUTTON (dcol), &desktop_colour);
    g_signal_connect (dcol, "color-set", G_CALLBACK (on_desktop_colour_set), NULL);

    bcol = gtk_builder_get_object (builder, "colorbutton3");
    gtk_color_button_set_color (GTK_COLOR_BUTTON (bcol), &bar_colour);
    g_signal_connect (bcol, "color-set", G_CALLBACK (on_bar_colour_set), NULL);

    btcol = gtk_builder_get_object (builder, "colorbutton4");
    gtk_color_button_set_color (GTK_COLOR_BUTTON (btcol), &bartext_colour);
    g_signal_connect (btcol, "color-set", G_CALLBACK (on_bartext_colour_set), NULL);

    htcol = gtk_builder_get_object (builder, "colorbutton5");
    gtk_color_button_set_color (GTK_COLOR_BUTTON (htcol), &themetext_colour);
    g_signal_connect (htcol, "color-set", G_CALLBACK (on_themetext_colour_set), NULL);

    dtcol = gtk_builder_get_object (builder, "colorbutton6");
    gtk_color_button_set_color (GTK_COLOR_BUTTON (dtcol), &desktoptext_colour);
    g_signal_connect (dtcol, "color-set", G_CALLBACK (on_desktoptext_colour_set), NULL);

    dmod = gtk_builder_get_object (builder, "comboboxtext1");
    if (!strcmp (desktop_mode, "center")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 1);
    else if (!strcmp (desktop_mode, "fit")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 2);
    else if (!strcmp (desktop_mode, "crop")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 3);
    else if (!strcmp (desktop_mode, "stretch")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 4);
    else if (!strcmp (desktop_mode, "tile")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 5);
    else gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 0);
    g_signal_connect (dmod, "changed", G_CALLBACK (on_desktop_mode_set), gtk_builder_get_object (builder, "filechooserbutton1"));

    item = gtk_builder_get_object (builder, "defs_lg");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), &flag3);
    item = gtk_builder_get_object (builder, "defs_med");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), &flag2);
    item = gtk_builder_get_object (builder, "defs_sml");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), &flag1);

    rb1 = gtk_builder_get_object (builder, "radiobutton1");
    g_signal_connect (rb1, "toggled", G_CALLBACK (on_bar_pos_set), NULL);
    rb2 = gtk_builder_get_object (builder, "radiobutton2");
    g_signal_connect (rb2, "toggled", G_CALLBACK (on_bar_pos_set), NULL);
    if (barpos) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb2), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb1), TRUE);

    isz = gtk_builder_get_object (builder, "comboboxtext2");
    if (icon_size <= 20) gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 3);
    else if (icon_size <= 28) gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 2);
    else if (icon_size <= 36) gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 1);
    else gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 0);
    g_signal_connect (isz, "changed", G_CALLBACK (on_menu_size_set), NULL);

    csz = gtk_builder_get_object (builder, "comboboxtext3");
    if (cursor_size >= 48) gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 0);
    else if (cursor_size >= 36) gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 1);
    else gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 2);
    g_signal_connect (csz, "changed", G_CALLBACK (on_cursor_size_set), NULL);

    cb1 = gtk_builder_get_object (builder, "checkbutton1");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb1), show_docs);
    g_signal_connect (cb1, "toggled", G_CALLBACK (on_toggle_docs), NULL);
    cb2 = gtk_builder_get_object (builder, "checkbutton2");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb2), show_trash);
    g_signal_connect (cb2, "toggled", G_CALLBACK (on_toggle_trash), NULL);
    cb3 = gtk_builder_get_object (builder, "checkbutton3");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb3), show_mnts);
    g_signal_connect (cb3, "toggled", G_CALLBACK (on_toggle_mnts), NULL);

    cmsg = gtk_builder_get_object (builder, "label35");
    gtk_widget_hide (GTK_WIDGET (cmsg));

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_CANCEL)
    {
        restore_config_files ();
        reload_lxsession ();
        reload_lxpanel ();
        reload_openbox ();
        reload_pcmanfm ();
    }
    else save_greeter_settings ();
    gtk_widget_destroy (dlg);

    return 0;
}
