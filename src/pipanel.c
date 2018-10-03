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
static const char *terminal_font, *orig_terminal_font;
static int icon_size, orig_icon_size;
static GdkColor theme_colour, orig_theme_colour;
static GdkColor themetext_colour, orig_themetext_colour;
static GdkColor desktop_colour, orig_desktop_colour;
static GdkColor desktoptext_colour, orig_desktoptext_colour;
static GdkColor bar_colour, orig_bar_colour;
static GdkColor bartext_colour, orig_bartext_colour;
static int barpos, orig_barpos;
static int show_docs, orig_show_docs;
static int show_trash, orig_show_trash;
static int show_mnts, orig_show_mnts;
static int folder_size, orig_folder_size;
static int thumb_size, orig_thumb_size;
static int pane_size, orig_pane_size;
static int sicon_size, orig_sicon_size;
static int tb_icon_size, orig_tb_icon_size;
static int lo_icon_size, orig_lo_icon_size;
static int cursor_size, orig_cursor_size;
static int task_width, orig_task_width;
static int handle_width, orig_handle_width;

/* Flag to indicate whether lxsession is version 4.9 or later, in which case no need to refresh manually */

static gboolean needs_refresh;

/* Version of Libreoffice installed - affects toolbar icon setting */

static char lo_ver;

/* Controls */
static GObject *hcol, *htcol, *font, *dcol, *dtcol, *dmod, *dpic, *barh, *bcol, *btcol, *rb1, *rb2, *isz, *cb1, *cb2, *cb3, *csz, *cmsg;

static void backup_values (void);
static int restore_values (void);
static void check_themes (void);
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
static void save_libreoffice_settings (void);
static void save_qt_settings (void);
static void set_openbox_theme (const char *theme);
static void set_lxsession_theme (const char *theme);
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
static void on_set_scrollbars (int width);
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

static void backup_values (void)
{
    orig_desktop_font = desktop_font;
    orig_desktop_picture = desktop_picture;
    orig_desktop_mode = desktop_mode;
    orig_icon_size = icon_size;
    orig_theme_colour = theme_colour;
    orig_themetext_colour = themetext_colour;
    orig_desktop_colour = desktop_colour;
    orig_desktoptext_colour = desktoptext_colour;
    orig_bar_colour = bar_colour;
    orig_bartext_colour = bartext_colour;
    orig_barpos = barpos;
    orig_show_docs = show_docs;
    orig_show_trash = show_trash;
    orig_show_mnts = show_mnts;
    orig_folder_size = folder_size;
    orig_thumb_size = thumb_size;
    orig_pane_size = pane_size;
    orig_sicon_size = sicon_size;
    orig_tb_icon_size = tb_icon_size;
    orig_terminal_font = terminal_font;
    orig_lo_icon_size = lo_icon_size;
    orig_cursor_size = cursor_size;
    orig_task_width = task_width;
    orig_handle_width = handle_width;
}

static int restore_values (void)
{
    int ret = 0;
    if (desktop_font != orig_desktop_font)
    {
        desktop_font = orig_desktop_font;
        ret = 1;
    }
    if (desktop_picture != orig_desktop_picture)
    {
        desktop_picture = orig_desktop_picture;
        ret = 1;
    }
    if (desktop_mode != orig_desktop_mode)
    {
        desktop_mode = orig_desktop_mode;
        ret = 1;
    }
    if (icon_size != orig_icon_size)
    {
        icon_size = orig_icon_size;
        ret = 1;
    }
    if (!gdk_color_equal (&theme_colour, &orig_theme_colour))
    {
        theme_colour = orig_theme_colour;
        ret = 1;
    }
    if (!gdk_color_equal (&themetext_colour, &orig_themetext_colour))
    {
        themetext_colour = orig_themetext_colour;
        ret = 1;
    }
    if (!gdk_color_equal (&desktop_colour, &orig_desktop_colour))
    {
        desktop_colour = orig_desktop_colour;
        ret = 1;
    }
    if (!gdk_color_equal (&desktoptext_colour, &orig_desktoptext_colour))
    {
        desktoptext_colour = orig_desktoptext_colour;
        ret = 1;
    }
    if (!gdk_color_equal (&bar_colour, &orig_bar_colour))
    {
        bar_colour = orig_bar_colour;
        ret = 1;
    }
    if (!gdk_color_equal (&bartext_colour, &orig_bartext_colour))
    {
        bartext_colour = orig_bartext_colour;
        ret = 1;
    }
    if (barpos != orig_barpos)
    {
        barpos = orig_barpos;
        ret = 1;
    }
    if (strcmp ("PiX", orig_lxsession_theme))
    {
        set_lxsession_theme (orig_lxsession_theme);
        ret = 1;
    }
    if (strcmp ("PiX", orig_openbox_theme))
    {
        set_openbox_theme (orig_openbox_theme);
        ret = 1;
    }
    if (show_docs != orig_show_docs)
    {
        show_docs = orig_show_docs;
        ret = 1;
    }
    if (show_trash != orig_show_trash)
    {
        show_trash = orig_show_trash;
        ret = 1;
    }
    if (show_mnts != orig_show_mnts)
    {
        show_mnts = orig_show_mnts;
        ret = 1;
    }
    if (folder_size != orig_folder_size)
    {
        folder_size = orig_folder_size;
        ret = 1;
    }
    if (thumb_size != orig_thumb_size)
    {
        thumb_size = orig_thumb_size;
        ret = 1;
    }
    if (pane_size != orig_pane_size)
    {
        pane_size = orig_pane_size;
        ret = 1;
    }
    if (sicon_size != orig_sicon_size)
    {
        sicon_size = orig_sicon_size;
        ret = 1;
    }
    if (tb_icon_size != orig_tb_icon_size)
    {
        tb_icon_size = orig_tb_icon_size;
        ret = 1;
    }
    if (terminal_font != orig_terminal_font)
    {
        terminal_font = orig_terminal_font;
        ret = 1;
    }
    if (lo_icon_size != orig_lo_icon_size)
    {
        lo_icon_size = orig_lo_icon_size;
        ret = 1;
    }
    if (cursor_size != orig_cursor_size)
    {
        cursor_size = orig_cursor_size;
        ret = 1;
    }
    if (task_width != orig_task_width)
    {
        task_width = orig_task_width;
        ret = 1;
    }
    if (handle_width != orig_handle_width)
    {
        handle_width = orig_handle_width;
        ret = 1;
    }
    return ret;
}

/* Functions to load required values from user config files */

static void check_themes (void)
{
    const char *session_name;
    char *user_config_file, *ret, *cptr, *nptr, *fname;
    GKeyFile *kf;
    GError *err;
    int count;

    orig_lxsession_theme = "";
    orig_openbox_theme = "";

    // construct the file path for lxsession settings
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxsession/", session_name, "/desktop.conf", NULL);

    // read in data from file to a key file structure
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "GTK", "sNet/ThemeName", &err);
        if (err == NULL) orig_lxsession_theme = g_strdup (ret);
        g_free (ret);
    }
    g_key_file_free (kf);
    g_free (user_config_file);

    // construct the file path for openbox settings
    fname = g_strconcat (g_ascii_strdown (session_name, -1), "-rc.xml", NULL);
    user_config_file = g_build_filename (g_get_user_config_dir (), "openbox/", fname, NULL);
    g_free (fname);

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    xmlDocPtr xDoc = xmlParseFile (user_config_file);
    if (xDoc)
    {
        xmlXPathContextPtr xpathCtx = xmlXPathNewContext (xDoc);
        xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='name']", xpathCtx);
        xmlNode *node = xpathObj->nodesetval->nodeTab[0];
        if (node) orig_openbox_theme = xmlNodeGetContent (node);

        // cleanup XML
        xmlXPathFreeObject (xpathObj);
        xmlXPathFreeContext (xpathCtx);
        xmlSaveFile (user_config_file, xDoc);
        xmlFreeDoc (xDoc);
        xmlCleanupParser ();
    }

    g_free (user_config_file);

    // set the new themes if needed
    if (strcmp ("PiX", orig_lxsession_theme))
    {
        set_lxsession_theme ("PiX");
        reload_lxsession ();
    }
    if (strcmp ("PiX", orig_openbox_theme))
    {
        set_openbox_theme ("PiX");
        reload_openbox ();
    }
}

static void load_lxsession_settings (void)
{
    const char *session_name;
    char *user_config_file, *ret, *cptr, *nptr;
    GKeyFile *kf;
    GError *err;
    int val;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxsession/", session_name, "/desktop.conf", NULL);

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
        desktop_font = "<not set>";
        return;
    }

    // get data from the key file
    err = NULL;
    ret = g_key_file_get_string (kf, "GTK", "sGtk/FontName", &err);
    if (err == NULL) desktop_font = g_strdup (ret);
    else desktop_font = "<not set>";
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
    const char *session_name;
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;
    gint val;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "pcmanfm/", session_name, "/desktop-items-0.conf", NULL);

    // read in data from file to a key file
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        g_key_file_free (kf);
        g_free (user_config_file);
        gdk_color_parse ("#D6D3DE", &desktop_colour);
        gdk_color_parse ("#000000", &desktoptext_colour);
        desktop_picture = "<not set>";
        desktop_mode = "color";
        return;
    }

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
    else desktop_picture = "<not set>";
    g_free (ret);

    err = NULL;
    ret = g_key_file_get_string (kf, "*", "wallpaper_mode", &err);
    if (err == NULL && ret) desktop_mode = g_strdup (ret);
    else desktop_mode = "color";
    g_free (ret);

    err = NULL;
    val = g_key_file_get_integer (kf, "*", "show_documents", &err);
    if (err == NULL && val >= 0 && val <= 1) show_docs = val;
    else show_docs = 0;

    err = NULL;
    val = g_key_file_get_integer (kf, "*", "show_trash", &err);
    if (err == NULL && val >= 0 && val <= 1) show_trash = val;
    else show_trash = 0;

    err = NULL;
    val = g_key_file_get_integer (kf, "*", "show_mounts", &err);
    if (err == NULL && val >= 0 && val <= 1) show_mnts = val;
    else show_mnts = 0;

    g_key_file_free (kf);
    g_free (user_config_file);

    // read in data from file manager config file
    user_config_file = g_build_filename (g_get_user_config_dir (), "libfm/", "/libfm.conf", NULL);
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        g_key_file_free (kf);
        g_free (user_config_file);
        folder_size = 48;
        thumb_size = 128;
        pane_size = 24;
        sicon_size = 24;
        return;
    }

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

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_lxpanel_settings (void)
{
    const char *session_name;
    char *user_config_file, *cmdbuf, *res;
    int val;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxpanel/", session_name, "/panels/panel", NULL);

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
    const char *session_name;
    char *user_config_file, *fname;
    int val;

    handle_width = 10;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    fname = g_strconcat (g_ascii_strdown (session_name, -1), "-rc.xml", NULL);
    user_config_file = g_build_filename (g_get_user_config_dir (), "openbox/", fname, NULL);
    g_free (fname);

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

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxpanel/", session_name, "/panels/panel", NULL);

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
    user_config_file = g_build_filename (g_get_home_dir (), ".config/gtk-3.0/gtk.css", NULL);

    // check if the file exists - if not, create it...
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("echo '@define-color theme_selected_bg_color #%c%c%c%c%c%c\n@define-color theme_selected_fg_color #%c%c%c%c%c%c' > %s", 
            cstrb[1], cstrb[2], cstrb[5], cstrb[6], cstrb[9], cstrb[10],
            cstrf[1], cstrf[2], cstrf[5], cstrf[6], cstrf[9], cstrf[10], user_config_file);
    }
    else
    {
        if (vsystem ("grep -q theme_selected_bg_color %s\n", user_config_file))
        {
            vsystem ("echo '@define-color theme_selected_bg_color #%c%c%c%c%c%c' >> %s", 
                cstrb[1], cstrb[2], cstrb[5], cstrb[6], cstrb[9], cstrb[10], user_config_file);
        }
        else
        {
            vsystem ("sed -i s/'theme_selected_bg_color #......'/'theme_selected_bg_color #%c%c%c%c%c%c'/g %s",
                cstrb[1], cstrb[2], cstrb[5], cstrb[6], cstrb[9], cstrb[10], user_config_file);
        }

        if (vsystem ("grep -q theme_selected_fg_color %s\n", user_config_file))
        {
            vsystem ("echo '@define-color theme_selected_fg_color #%c%c%c%c%c%c' >> %s",
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
    const char *session_name;
    char *user_config_file, *str;
    char colbuf[256];
    GKeyFile *kf;
    gsize len;
    GError *err;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxsession/", session_name, "/desktop.conf", NULL);

    // read in data from file to a key file
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        g_key_file_free (kf);
        g_free (user_config_file);
        return;
    }

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

    // write the modified key file out
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_pcman_settings (void)
{
    const char *session_name;
    char *user_config_file, *str;
    char colbuf[32];
    GKeyFile *kf;
    gsize len;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "pcmanfm/", session_name, "/desktop-items-0.conf", NULL);

    // read in data from file to a key file
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        g_key_file_free (kf);
        g_free (user_config_file);
        return;
    }

    // update changed values in the key file
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

    // write the modified key file out
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);

    // read in data from file to a key file
    user_config_file = g_build_filename (g_get_user_config_dir (), "libfm/", "/libfm.conf", NULL);
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // failed to load the key file - there may be no default user copy, so try creating one...
        struct stat attr;
        if (stat (user_config_file, &attr) == -1)
        {
            g_free (user_config_file);
            user_config_file = g_build_filename (g_get_user_config_dir (), "libfm/", NULL);
            vsystem ("mkdir -p %s; cp /etc/xdg/libfm/libfm.conf %s", user_config_file, user_config_file);
            g_free (user_config_file);
            user_config_file = g_build_filename (g_get_user_config_dir (), "libfm/", "/libfm.conf", NULL);

            if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
            {
                g_key_file_free (kf);
                g_free (user_config_file);
                return;
            }
        }
        else
        {
            g_key_file_free (kf);
            g_free (user_config_file);
            return;
        }
    }
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
    const char *session_name;
    char *user_config_file, *fname, *font,*cptr;
    int count, size;
    const gchar *weight = NULL, *style = NULL;
    char buf[10];

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    fname = g_strconcat (g_ascii_strdown (session_name, -1), "-rc.xml", NULL);
    user_config_file = g_build_filename (g_get_user_config_dir (), "openbox/", fname, NULL);
    g_free (fname);

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
    xmlDocPtr xDoc = xmlParseFile (user_config_file);
    if (xDoc == NULL)
    {
        pango_font_description_free (pfd);
        g_free (font);
        g_free (user_config_file);
        return;
    }

    xmlNode *cur_node;
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext (xDoc);
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='font']", xpathCtx);

    // update relevant nodes with new values
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
    xmlXPathFreeObject (xpathObj);

    sprintf (buf, "%d", handle_width);
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='invHandleWidth']", xpathCtx);
    cur_node = xpathObj->nodesetval->nodeTab[0];
    if (cur_node) xmlNodeSetContent (cur_node, buf);
    else
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNewChild (cur_node, NULL, "invHandleWidth", buf);
    }

    sprintf (buf, "#%02x%02x%02x", theme_colour.red >> 8, theme_colour.green >> 8, theme_colour.blue >> 8);
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titleColor']", xpathCtx);
    cur_node = xpathObj->nodesetval->nodeTab[0];
    if (cur_node) xmlNodeSetContent (cur_node, buf);
    else
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNewChild (cur_node, NULL, "titleColor", buf);
    }

    sprintf (buf, "#%02x%02x%02x", themetext_colour.red >> 8, themetext_colour.green >> 8, themetext_colour.blue >> 8);
    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='textColor']", xpathCtx);
    cur_node = xpathObj->nodesetval->nodeTab[0];
    if (cur_node) xmlNodeSetContent (cur_node, buf);
    else
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNewChild (cur_node, NULL, "textColor", buf);
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

static void set_openbox_theme (const char *theme)
{
    const char *session_name;
    char *user_config_file, *fname;
    int count;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    fname = g_strconcat (g_ascii_strdown (session_name, -1), "-rc.xml", NULL);
    user_config_file = g_build_filename (g_get_user_config_dir (), "openbox/", fname, NULL);
    g_free (fname);

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
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);

    // update relevant nodes with new values
    for (count = 0; count < xpathObj->nodesetval->nodeNr; count++)
    {
        xmlNode *node = xpathObj->nodesetval->nodeTab[count];
        xmlAttr *attr = node->properties;
        xmlNode *cur_node = NULL;
        for (cur_node = node->children; cur_node; cur_node = cur_node->next)
        {
            if (cur_node->type == XML_ELEMENT_NODE)
            {
                if (!strcmp (cur_node->name, "name")) xmlNodeSetContent (cur_node, theme);
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
}

static void set_lxsession_theme (const char *theme)
{
    const char *session_name;
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxsession/", session_name, "/desktop.conf", NULL);

    // read in data from file to a key file
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        g_key_file_free (kf);
        g_free (user_config_file);
        return;
    }

    // update changed values in the key file
    g_key_file_set_string (kf, "GTK", "sNet/ThemeName", theme);

    // write the modified key file out
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);
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

static void on_set_scrollbars (int width)
{
    char *conffile;

    // GTK2 override file
    conffile = g_build_filename (g_get_home_dir (), ".gtkrc-2.0", NULL);

    // check if the file exists - if not, create it...
    if (!g_file_test (conffile, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("echo 'style \"scrollbar\"\n{\n\tGtkRange::slider-width = %d\n\tGtkRange::stepper-size = %d\n}\n' > %s", width, width, conffile);
    }
    else
    {
        // check if the scrollbar button entry is in the file - if not, add it...
        if (system ("cat ~/.gtkrc-2.0 | tr '\\n' '\\a' | grep -q 'style \"scrollbar\".*{.*}'"))
        {
            vsystem ("echo '\n\nstyle \"scrollbar\"\n{\n\tGtkRange::slider-width = %d\n\tGtkRange::stepper-size = %d\n}\n' >> %s", width, width, conffile);
        }
        else
        {
            // block exists - check for relevant entries in it and add / amend as appropriate
            if (system ("cat ~/.gtkrc-2.0 | tr '\\n' '\\a' | grep -q 'style \"scrollbar\".*{.*GtkRange::slider-width.*}'"))
            {
                // entry does not exist - add it
                vsystem ("sed -i '/style \"scrollbar\"/,/}/ s/}/\tGtkRange::slider-width = %d\\n}/' ~/.gtkrc-2.0", width);
            }
            else
            {
                // entry exists - amend it with sed
                vsystem ("sed -i '/style \"scrollbar\"/,/}/ s/GtkRange::slider-width =\\s*[0-9]*/GtkRange::slider-width = %d/' ~/.gtkrc-2.0", width);
            }

            if (system ("cat ~/.gtkrc-2.0 | tr '\\n' '\\a' | grep -q 'style \"scrollbar\".*{.*GtkRange::stepper-size.*}'"))
            {
                // entry does not exist - add it
                vsystem ("sed -i '/style \"scrollbar\"/,/}/ s/}/\tGtkRange::stepper-size = %d\\n}/' ~/.gtkrc-2.0", width);
            }
            else
            {
                // entry exists - amend it with sed
                vsystem ("sed -i '/style \"scrollbar\"/,/}/ s/GtkRange::stepper-size =\\s*[0-9]*/GtkRange::stepper-size = %d/' ~/.gtkrc-2.0", width);
            }
        }
    }
    g_free (conffile);

    // GTK3 override file
    width -= 6; // GTK3 parameter sets the width of the slider, not the entire bar
    conffile = g_build_filename (g_get_user_config_dir (), "gtk-3.0/gtk.css", NULL);

    // check if the file exists - if not, create it...
    if (!g_file_test (conffile, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("echo 'scrollbar slider {\n min-width: %dpx;\n min-height: %dpx;\n}\n\nscrollbar button {\n min-width: %dpx; min-height: %dpx;\n}\n' > %s", width, width, width, width, conffile);
    }
    else
    {
        // check if the scrollbar button entry is in the file - if not, add it...
        if (system ("cat ~/.config/gtk-3.0/gtk.css | tr '\\n' '\\a' | grep -q 'scrollbar\\s*button\\s*{.*}'"))
        {
            vsystem ("echo '\n\nscrollbar button {\n min-width: %dpx;\n min-height: %dpx;\n}\n' >> %s", width, width, conffile);
        }
        else
        {
            // block exists - check for relevant entries in it and add / amend as appropriate
            if (system ("cat ~/.config/gtk-3.0/gtk.css | tr '\\n' '\\a' | grep -q -P 'scrollbar\\s*button\\s*{[^{]*?min-width[^}]*?}'"))
            {
                // entry does not exist - add it
                vsystem ("sed -i '/scrollbar\\s*button\\s*{/,/}/ s/}/ min-width: %dpx;\\n}/' ~/.config/gtk-3.0/gtk.css", width);
            }
            else
            {
                // entry exists - amend it with sed
                vsystem ("sed -i '/scrollbar\\s*button\\s*{/,/}/ s/min-width:\\s*[0-9]*px/min-width: %dpx/' ~/.config/gtk-3.0/gtk.css", width);
            }

            if (system ("cat ~/.config/gtk-3.0/gtk.css | tr '\\n' '\\a' | grep -q -P 'scrollbar\\s*button\\s*{[^{]*?min-height[^}]*?}'"))
            {
               // entry does not exist - add it
                vsystem ("sed -i '/scrollbar\\s*button\\s*{/,/}/ s/}/ min-height: %dpx;\\n}/' ~/.config/gtk-3.0/gtk.css", width);
            }
            else
            {
                // entry exists - amend it with sed
                vsystem ("sed -i '/scrollbar\\s*button\\s*{/,/}/ s/min-height:\\s*[0-9]*px/min-height: %dpx/' ~/.config/gtk-3.0/gtk.css", width);
            }
        }

        // check if the scrollbar slider entry is in the file - if not, add it...
        if (system ("cat ~/.config/gtk-3.0/gtk.css | tr '\\n' '\\a' | grep -q 'scrollbar\\s*slider\\s*{.*}'"))
        {
            vsystem ("echo '\n\nscrollbar slider {\n min-width: %dpx;\n min-height: %dpx;\n}\n' >> %s", width, width, conffile);
        }
        else
        {
            // block exists - check for relevant entries in it and add / amend as appropriate
            if (system ("cat ~/.config/gtk-3.0/gtk.css | tr '\\n' '\\a' | grep -q -P 'scrollbar\\s*slider\\s*{[^{]*?min-width[^}]*?}'"))
            {
                // entry does not exist - add it
                vsystem ("sed -i '/scrollbar\\s*slider\\s*{/,/}/ s/}/ min-width: %dpx;\\n}/' ~/.config/gtk-3.0/gtk.css", width);
            }
            else
            {
                // entry exists - amend it with sed
                vsystem ("sed -i '/scrollbar\\s*slider\\s*{/,/}/ s/min-width:\\s*[0-9]*px/min-width: %dpx/' ~/.config/gtk-3.0/gtk.css", width);
            }

            if (system ("cat ~/.config/gtk-3.0/gtk.css | tr '\\n' '\\a' | grep -q -P 'scrollbar\\s*slider\\s*{[^{]*?min-height[^}]*?}'"))
            {
                // entry does not exist - add it
                vsystem ("sed -i '/scrollbar\\s*slider\\s*{/,/}/ s/}/ min-height: %dpx;\\n}/' ~/.config/gtk-3.0/gtk.css", width);
            }
            else
            {
                // entry exists - amend it with sed
                vsystem ("sed -i '/scrollbar\\s*slider\\s*{/,/}/ s/min-height:\\s*[0-9]*px/min-height: %dpx/' ~/.config/gtk-3.0/gtk.css", width);
            }
        }
    }
    g_free (conffile);
}

static void on_set_defaults (GtkButton* btn, gpointer ptr)
{
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
        gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 0);
        gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 1);
        on_set_scrollbars (18);
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
        gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 1);
        gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 2);
        on_set_scrollbars (13);
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
        gtk_combo_box_set_active (GTK_COMBO_BOX (isz), 3);
        gtk_combo_box_set_active (GTK_COMBO_BOX (csz), 2);
        on_set_scrollbars (13);
    }
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
    set_openbox_theme ("PiX");
    set_lxsession_theme ("PiX");
    save_lxsession_settings ();
    save_pcman_settings ();
    save_obconf_settings ();
    save_gtk3_settings ();
    save_lxpanel_settings ();
    save_lxterm_settings ();
    save_libreoffice_settings ();
    save_qt_settings ();
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
    check_themes ();
    load_lxsession_settings ();
    //load_obpix_settings ();
    load_pcman_settings ();
    load_lxpanel_settings ();
    load_lxterm_settings ();
    load_libreoffice_settings ();
    load_obconf_settings ();
    backup_values ();

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
        if (restore_values ())
        {
            save_lxsession_settings ();
            save_pcman_settings ();
            save_obconf_settings ();
            save_gtk3_settings ();
            save_lxpanel_settings ();
            save_lxterm_settings ();
            save_libreoffice_settings ();
            save_qt_settings ();
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
