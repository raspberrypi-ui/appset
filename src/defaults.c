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

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

Config def_med;
static Config def_lg, def_sm;

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
static void on_set_defaults (GtkButton* btn, gpointer ptr);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

void reload_gsettings (void)
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

/*----------------------------------------------------------------------------*/
/* Create and use defaults                                                    */
/*----------------------------------------------------------------------------*/

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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            monname = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), i);
#pragma GCC diagnostic pop
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

void create_defaults (void)
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
    def_lg.lo_icon_size = 3;
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

/*----------------------------------------------------------------------------*/
/* Control handlers                                                           */
/*----------------------------------------------------------------------------*/

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
    set_desktop_controls ();
    set_taskbar_controls ();
    set_system_controls ();

    // save changes to files if not using medium (the global default)
    if ((long int) ptr != 2)
    {
        save_lxsession_settings ();
        save_xsettings ();
        save_pcman_g_settings ();
        for (i = 0; i < ndesks; i++)
            save_pcman_settings (i);
        save_libfm_settings ();
        if (wm == WM_OPENBOX) save_obconf_settings (FALSE);
        if (wm == WM_LABWC) save_obconf_settings (TRUE);
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
    reload_xsettings ();
    reload_panel ();
    reload_wm ();
    reload_desktop ();
    reload_theme (FALSE);
}

/*----------------------------------------------------------------------------*/
/* Initialisation                                                             */
/*----------------------------------------------------------------------------*/

void load_defaults_tab (GtkBuilder *builder)
{
    GObject *item;

    item = gtk_builder_get_object (builder, "defs_lg");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), (void *) 3);

    item = gtk_builder_get_object (builder, "defs_med");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), (void *) 2);

    item = gtk_builder_get_object (builder, "defs_sml");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), (void *) 1);
}

/* End of file */
/*----------------------------------------------------------------------------*/
