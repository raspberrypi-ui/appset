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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
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

#define GREY    "#808080"

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

Config def_med;
static Config def_vlg, def_lg, def_sm;

static GtkWidget *rb_classic, *rb_dock;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void delete_file (char *filepath);
static char *libfm_file (void);
static void defaults_lxpanel (void);
static void defaults_lxsession (void);
static void defaults_pcman (int desktop);
static void defaults_pcman_g (void);
static void defaults_gtk3 (void);
static void save_libfm_settings (void);
static void save_lxterm_settings (void);
static void save_libreoffice_settings (void);
static void reset_to_defaults (void);
static void on_set_defaults (GtkButton *btn, gpointer ptr);
static void enable_dock (void);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

static void delete_file (char *filepath)
{
    char *orig = g_build_filename (g_get_home_dir (), filepath, NULL);

    if (g_file_test (orig, G_FILE_TEST_IS_REGULAR))
    {
        g_remove (orig);
    }
    g_free (orig);
}

/*----------------------------------------------------------------------------*/
/* Load / save data                                                           */
/*----------------------------------------------------------------------------*/

static char *libfm_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "libfm/libfm.conf", NULL);
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

static void defaults_wfpanel (void)
{
    char *user_config_file, *ret, *buf;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from file to a key file
    user_config_file = wfpanel_file (TRUE);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "panel", "position", &err);
        if (err == NULL && ret && !strcmp (ret, "bottom")) def_med.barpos = 1;
        else def_med.barpos = 0;
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "dock", "position", &err);
        if (err == NULL && ret && !strcmp (ret, "top")) def_med.dockpos = 0;
        else def_med.dockpos = 1;
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "panel", "icon_size", &err);
        if (err == NULL && val >= 16 && val <= 48) def_med.icon_size = val + 4;
        else def_med.icon_size = 36;

        err = NULL;
        val = g_key_file_get_integer (kf, "dock", "icon_size", &err);
        if (err == NULL && val >= 16 && val <= 48) def_med.dock_icon_size = val + 4;
        else def_med.dock_icon_size = 52;

        err = NULL;
        ret = g_key_file_get_string (kf, "panel", "autohide", &err);
        if (err == NULL && ret && !strcmp (ret, "true")) def_med.barahide = 1;
        else def_med.barahide = 0;
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "dock", "autohide", &err);
        if (err == NULL && ret && !strcmp (ret, "true")) def_med.dockahide = 1;
        else def_med.dockahide = 0;
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "panel", "exclusive", &err);
        if (err == NULL && ret && !strcmp (ret, "false")) def_med.barexcl = 0;
        else def_med.barexcl = 1;
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "dock", "exclusive", &err);
        if (err == NULL && ret && !strcmp (ret, "true")) def_med.dockexcl = 1;
        else def_med.dockexcl = 0;
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "panel", "monitor", &err);
        DEFAULT (monitor);
        if (err == NULL && ret)
        {
            for (val = 0; val < ndesks; val++)
            {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), val);
#pragma GCC diagnostic pop
                if (!g_strcmp0 (buf, ret)) def_med.monitor = val;
                else def_med.monitor = 0;
                g_free (buf);
            }
        }
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "dock", "monitor", &err);
        DEFAULT (dmonitor);
        if (err == NULL && ret)
        {
            for (val = 0; val < ndesks; val++)
            {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), val);
#pragma GCC diagnostic pop
                if (!g_strcmp0 (buf, ret)) def_med.dmonitor = val;
                else def_med.dmonitor = 0;
                g_free (buf);
            }
        }
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "panel", "window-list_max_width", &err);
        if (err == NULL) def_med.task_width = val;
        else def_med.task_width = 200;

        err = NULL;
        ret = g_key_file_get_string (kf, "dock", "widgets_left", &err);
        if (err == NULL && ret && strlen (ret)) def_med.dock = 1;
        else def_med.dock = 0;
        g_free (ret);
    }
    else
    {
        def_med.barpos = 0;
        def_med.icon_size = 36;
        def_med.dock_icon_size = 52;
        def_med.monitor = 0;
        def_med.dockpos = 1;
        def_med.dmonitor = 0;
        def_med.task_width = 200;
        def_med.dock = 0;
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void defaults_lxsession (void)
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
        val = g_key_file_get_integer (kf, "*", "show_home", &err);
        if (err == NULL && val >= 0 && val <= 1) def_med.desktops[desktop].show_home = val;
        else def_med.desktops[desktop].show_home = 0;

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
        def_med.desktops[desktop].show_home = 0;
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

        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "use_swaybg", &err);
        if (err == NULL && val >= 0 && val <= 1) def_med.passive_desktop = val;
        else def_med.passive_desktop = 0;
    }
    else
    {
        def_med.common_bg = 0;
        def_med.passive_desktop = 0;
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
        sys_config_file = g_build_filename ("/usr/share/themes", theme_name (dark), "gtk-3.0", "gtk-colours.css", NULL);

        cmdbuf = g_strdup_printf ("grep -hPo '(?<=@define-color\\stheme_selected_bg_color\\s)[^;]*' %s 2> /dev/null", sys_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.theme_colour[dark], res)) gdk_rgba_parse (&def_med.theme_colour[dark], GREY);
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -hPo '(?<=@define-color\\stheme_selected_fg_color\\s)[^;]*' %s 2> /dev/null", sys_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.themetext_colour[dark], res)) gdk_rgba_parse (&def_med.themetext_colour[dark], GREY);
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -hPo '(?<=@define-color\\sbar_bg_color\\s)[^;]*' %s 2> /dev/null", sys_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.bar_colour[dark], res)) gdk_rgba_parse (&def_med.bar_colour[dark], GREY);
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -hPo '(?<=@define-color\\sbar_fg_color\\s)[^;]*' %s 2> /dev/null", sys_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.bartext_colour[dark], res)) gdk_rgba_parse (&def_med.bartext_colour[dark], GREY);
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -hPo '(?<=@define-color\\sdock_bg_color\\s)[^;]*' %s 2> /dev/null", sys_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.dock_colour[dark], res)) gdk_rgba_parse (&def_med.dock_colour[dark], GREY);
        g_free (res);

        cmdbuf = g_strdup_printf ("grep -hPo '(?<=@define-color\\sdock_fg_color\\s)[^;]*' %s 2> /dev/null", sys_config_file);
        res = get_string (cmdbuf);
        g_free (cmdbuf);
        if (!res[0] || !gdk_rgba_parse (&def_med.docktext_colour[dark], res)) gdk_rgba_parse (&def_med.docktext_colour[dark], GREY);
        g_free (res);

        g_free (sys_config_file);
    }
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

static void save_libreoffice_settings (void)
{
    char *user_config_file;
    char buf[2];

    xmlDocPtr xDoc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlNodePtr node;

    sprintf (buf, "%d", cur_conf.lo_icon_size);

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "libreoffice/4/user/registrymodifications.xcu", NULL);
    check_directory (user_config_file);

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    if (g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        xDoc = xmlReadFile (user_config_file, NULL, XML_PARSE_NOBLANKS);
        if (!xDoc) xDoc = xmlNewDoc (XC ("1.0"));
    }
    else xDoc = xmlNewDoc (XC ("1.0"));
    xpathCtx = xmlXPathNewContext (xDoc);
    xmlXPathRegisterNs (xpathCtx, XC ("oor"), XC ("http://openoffice.org/2001/registry"));

    // create root node if needed
    xpathObj = xmlXPathEvalExpression (XC ("/oor:items"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        node = xmlNewNode (NULL, XC ("oor:items"));
        xmlNewNs (node, XC ("http://openoffice.org/2001/registry"), XC ("oor"));
        xmlNewNs (node, XC ("http://www.w3.org/2001/XMLSchema"), XC ("xs"));
        xmlNewNs (node, XC ("http://www.w3.org/2001/XMLSchema-instance"), XC ("xsi"));
        xmlDocSetRootElement (xDoc, node);
    }
    else node = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    // update value node or create if needed
    xpathObj = xmlXPathEvalExpression (XC ("/oor:items/item[@oor:path='/org.openoffice.Office.Common/Misc']/prop[@oor:name='SymbolSet']/value"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        node = xmlNewChild (node, NULL, XC ("item"), NULL);
        xmlSetProp (node, XC ("oor:path"), XC ("/org.openoffice.Office.Common/Misc"));
        node = xmlNewChild (node, NULL, XC ("prop"), NULL);
        xmlSetProp (node, XC ("oor:name"), XC ("SymbolSet"));
        xmlSetProp (node, XC ("oor:op"), XC ("fuse"));
        xmlNewChild (node, NULL, XC ("value"), XC (buf));
    }
    else xmlNodeSetContent (xpathObj->nodesetval->nodeTab[0], XC (buf));
    xmlXPathFreeObject (xpathObj);

    // cleanup XML
    xmlXPathFreeContext (xpathCtx);
    xmlSaveFormatFile (user_config_file, xDoc, 1);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_config_file);
}

/*----------------------------------------------------------------------------*/
/* Create and use defaults                                                    */
/*----------------------------------------------------------------------------*/

void init_session (const char *theme)
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

static void reset_to_defaults (void)
{
    char *path, *monname;
    int i;

    delete_file (".config/openbox/rpd-rc.xml");
    delete_file (".config/lxsession/rpd-x/desktop.conf");
    delete_file (".config/lxpanel-pi/panels/panel");
    delete_file (".config/wf-panel-pi/wf-panel-pi.ini");
    delete_file (".config/pcmanfm/default/pcmanfm.conf");

    for (i = 0; i < ndesks; i++)
    {
        path = g_strdup_printf (".config/pcmanfm/default/desktop-items-%d.conf", i);
        delete_file (path);
        g_free (path);

        if (wm != WM_OPENBOX)
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            monname = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), i);
#pragma GCC diagnostic pop
            path = g_strdup_printf (".config/pcmanfm/default/desktop-items-%s.conf", monname);
            delete_file (path);
            g_free (path);
            g_free (monname);
        }
    }

    path = g_build_filename (".local/share/themes", theme_name (LIGHT), "gtk-3.0/gtk.css", NULL);
    delete_file (path);
    g_free (path);

    path = g_build_filename (".local/share/themes", theme_name (DARK), "gtk-3.0/gtk.css", NULL);
    delete_file (path);
    g_free (path);

    delete_file (".config/libfm/libfm.conf");
    delete_file (".config/gtk-3.0/gtk.css");
    delete_file (".config/qt5ct/qt5ct.conf");
    delete_file (".config/qt6ct/qt6ct.conf");
    delete_file (".config/xsettingsd/xsettingsd.conf");
    delete_file (".config/labwc/themerc-override");
    delete_file (".config/labwc/rc.xml");
    delete_file (".gtkrc-2.0");

    init_session (theme_name (TEMP));
}

void create_defaults (void)
{
    int i;
    // defaults for controls

    // /etc/xdg/lxpanel-pi/panels/panel
    if (wm == WM_OPENBOX) defaults_lxpanel ();
    else defaults_wfpanel ();

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

    def_vlg = def_lg = def_sm = def_med;

    def_lg.icon_size = 52;
    def_lg.dock_icon_size = 68;
    def_lg.cursor_size = 36;

    def_lg.terminal_font = "Monospace 15";
    def_lg.folder_size = 80;
    def_lg.thumb_size = 160;
    def_lg.pane_size = 32;
    def_lg.sicon_size = 32;
    def_lg.tb_icon_size = 48;
    def_lg.lo_icon_size = 3;
    def_lg.task_width = 300;
    def_lg.handle_width = 20;
    def_lg.scrollbar_width = 17;

    def_sm.icon_size = 20;
    def_sm.dock_icon_size = 28;
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

    def_vlg.icon_size = 68;
    def_vlg.dock_icon_size = 100;
    def_vlg.cursor_size = 48;

    def_vlg.terminal_font = "Monospace 20";
    def_vlg.folder_size = 96;
    def_vlg.thumb_size = 192;
    def_vlg.pane_size = 48;
    def_vlg.sicon_size = 48;
    def_vlg.tb_icon_size = 48;
    def_vlg.lo_icon_size = 3;
    def_vlg.task_width = 300;
    def_vlg.handle_width = 20;
    def_vlg.scrollbar_width = 17;

    if (trix_theme)
    {
        def_vlg.desktop_font = "Nunito Sans Light 24";
        def_lg.desktop_font = "Nunito Sans Light 16";
        def_sm.desktop_font = "Nunito Sans Light 8";
    }
    else
    {
        def_lg.desktop_font = "PibotoLt 24";
        def_lg.desktop_font = "PibotoLt 16";
        def_sm.desktop_font = "PibotoLt 8";
    }
}

/*----------------------------------------------------------------------------*/
/* Control handlers                                                           */
/*----------------------------------------------------------------------------*/

static void on_set_defaults (GtkButton *btn, gpointer ptr)
{
    int i;

    if (cur_conf.darkmode == 1)
    {
        if (!system ("pgrep geany > /dev/null"))
        {
            message (_("The theme for Geany cannot be changed while it is open.\nPlease close it and try again."), TRUE);
            return;
        }

        if (!system ("pgrep galculator > /dev/null"))
        {
            message (_("The theme for Calculator cannot be changed while it is open.\nPlease close it and try again."), TRUE);
            return;
        }
    }

    // clear all the config files
    reset_to_defaults ();

    // set config structure to a default
    switch ((long int) ptr)
    {
        case 4 :    cur_conf = def_vlg;
                    break;
        case 3 :    cur_conf = def_lg;
                    break;
        case 1 :    cur_conf = def_sm;
                    break;
        default :   cur_conf = def_med;
    }

    // save changes to files if not using medium (the global default)
    if ((long int) ptr != 2)
    {
        save_pcman_g_settings ();
        for (i = 0; i < ndesks; i++)
            save_pcman_settings (i);
        save_libfm_settings ();
        save_qt_settings ();
    }

    save_session_settings ();
    save_gtk3_settings ();
    save_panel_settings ();
    save_greeter_settings ();

    // save application-specific config - we don't delete these files first...
    save_lxterm_settings ();
    save_libreoffice_settings ();
    save_app_settings ();

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rb_dock)))
    {
        cur_conf.dock = TRUE;
        enable_dock ();
    }
    else cur_conf.dock = FALSE;

    // reset the GUI controls to match the variables
    set_desktop_controls ();
    set_taskbar_controls ();
    set_system_controls ();

    // reload everything to reflect the current state
    reload_session ();
    reload_panel ();
    reload_desktop ();
    reload_theme (FALSE);
}

static void enable_dock (void)
{
    // add the dock to wf-panel-pi.ini
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = wfpanel_file (FALSE);
    check_directory (user_config_file);

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_string (kf, "panel", "widgets_left", "");
    g_key_file_set_string (kf, "panel", "widgets_right", "");
    g_key_file_set_string (kf, "dock", "widgets_left", "nmenu spacing0 tlist spacing0 clock spacing0");
    g_key_file_set_string (kf, "dock", "widgets_right", "netman volumepulse updater ejecter power tray bluetooth connect");
    g_key_file_set_string (kf, "panel", "nmenu_overlay_text_col", "rgb(255,255,255)");
    g_key_file_set_string (kf, "panel", "clock_analogue", "1");

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);

    // set the desktop
    cur_conf.passive_desktop = TRUE;
    save_pcman_g_settings ();

    cur_conf.desktops[0].desktop_picture = g_strdup ("/usr/share/rpd-wallpaper/turbines.jpg");
    save_pcman_settings (0);

    restart_desktop ();

    // configure libfm
    user_config_file = libfm_file ();
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        check_directory (user_config_file);
        vsystem ("cp /etc/xdg/libfm/libfm.conf %s", user_config_file);
    }

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_integer (kf, "places", "places_trash", 1);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);

    // set the theme colours
    gdk_rgba_parse (&cur_conf.bar_colour[0], "rgba(0,0,0,0)");
    gdk_rgba_parse (&cur_conf.bar_colour[1], "rgba(0,0,0,0)");
    gdk_rgba_parse (&cur_conf.bartext_colour[0], "rgb(255,255,255)");
    gdk_rgba_parse (&cur_conf.bartext_colour[1], "rgb(255,255,255)");
    set_theme (theme_name (TEMP));
    save_gtk3_settings ();
    reload_theme (FALSE);

    // set the new menu shortcut
    xmlDocPtr xDoc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlNodePtr cur_node;

    user_config_file = labwc_file ();
    check_directory (user_config_file);

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    if (g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        xDoc = xmlReadFile (user_config_file, NULL, XML_PARSE_NOBLANKS);
        if (!xDoc) xDoc = xmlNewDoc (XC ("1.0"));
    }
    else xDoc = xmlNewDoc (XC ("1.0"));
    xpathCtx = xmlXPathNewContext (xDoc);
    xmlXPathRegisterNs (xpathCtx, XC ("o"), XC ("http://openbox.org/3.4/rc"));

    // check that the config and keyboard nodes exist in the document - create them if not
    xpathObj = xmlXPathEvalExpression (XC ("/o:openbox_config"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        cur_node = xmlNewNode (NULL, XC ("openbox_config"));
        xmlNewNs (cur_node, XC ("http://openbox.org/3.4/rc"), NULL);
        xmlDocSetRootElement (xDoc, cur_node);
    }
    else cur_node = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    xpathObj = xmlXPathEvalExpression (XC ("./o:keyboard"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        cur_node = xmlNewChild (cur_node, NULL, XC ("keyboard"), NULL);
    }
    else cur_node = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    // create relevant nodes with new values
    xpathObj = xmlXPathEvalExpression (XC ("./o:keybind[@key = 'Super_L']"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        cur_node = xmlNewChild (cur_node, NULL, XC ("keybind"), NULL);
        xmlSetProp (cur_node, XC ("key"), XC ("Super_L"));
        xmlSetProp (cur_node, XC ("onRelease"), XC ("yes"));
    }
    else cur_node = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    xpathObj = xmlXPathEvalExpression (XC ("./o:action[@name = 'Execute']"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        cur_node = xmlNewChild (cur_node, NULL, XC ("action"), NULL);
        xmlSetProp (cur_node, XC ("name"), XC ("Execute"));
    }
    else cur_node = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    xpathObj = xmlXPathEvalExpression (XC ("./o:command"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        xmlNewChild (cur_node, NULL, XC ("command"), XC ("wfpanelctl nmenu menu"));
    else
        xmlNodeSetContent (xpathObj->nodesetval->nodeTab[0], XC ("wfpanelctl nmenu menu"));
    xmlXPathFreeObject (xpathObj);

    // cleanup XML
    xmlXPathFreeContext (xpathCtx);
    xmlSaveFormatFile (user_config_file, xDoc, 1);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_config_file);
}

static void on_switch_dock (GtkRadioButton *btn, gpointer ptr)
{
    switch (cur_conf.icon_size)
    {
        case 20 :   on_set_defaults (NULL, (void *) 1);
                    break;
        case 52 :   on_set_defaults (NULL, (void *) 3);
                    break;
        case 68 :   on_set_defaults (NULL, (void *) 4);
                    break;
        default :   on_set_defaults (NULL, (void *) 2);
                    break;
    }
}

/*----------------------------------------------------------------------------*/
/* Initialisation                                                             */
/*----------------------------------------------------------------------------*/

void load_defaults_tab (GtkBuilder *builder)
{
    GObject *item;

    item = gtk_builder_get_object (builder, "defs_vlg");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), (void *) 4);

    item = gtk_builder_get_object (builder, "defs_lg");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), (void *) 3);

    item = gtk_builder_get_object (builder, "defs_med");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), (void *) 2);

    item = gtk_builder_get_object (builder, "defs_sml");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), (void *) 1);

    rb_classic = (GtkWidget *) gtk_builder_get_object (builder, "radiobutton7");
    rb_dock = (GtkWidget *) gtk_builder_get_object (builder, "radiobutton8");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_dock), cur_conf.dock);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_classic), !cur_conf.dock);
    g_signal_connect (rb_classic, "clicked", G_CALLBACK (on_switch_dock), NULL);
}

/* End of file */
/*----------------------------------------------------------------------------*/
