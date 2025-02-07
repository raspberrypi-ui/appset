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
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "pipanel.h"
#include "taskbar.h"
#include "desktop.h"
#include "defaults.h"

#include "system.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#define LARGE_ICON_THRESHOLD 20

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

/* Controls */
static GtkWidget *colour_hilite, *colour_hilitetext, *font_system, *rb_light;
static GtkWidget *rb_dark, *combo_cursor, *label_cursor;

/* Handler IDs */
static gulong id_cursor, id_dark;

static int orig_csize, orig_tbsize;
static char *orig_font;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void set_config_param (const char *file, const char *section, const char *tag, const char *value);
static void add_or_amend (const char *conffile, const char *block, const char *param, const char *repl);
static char *openbox_file (void);
static char *labwc_file (void);
static void load_obconf_settings (void);
static void load_lxsession_settings (void);
static void load_gsettings (void);
static void load_gtk3_settings (void);
static void save_wm_settings (void);
static void save_lxsession_settings (void);
static void save_gsettings (void);
static void save_xsettings (void);
static void save_labwc_to_settings (void);
static int is_dark (void);
static gboolean restore_theme (gpointer data);
static void on_theme_colour_set (GtkColorChooser* btn, gpointer ptr);
static void on_theme_textcolour_set (GtkColorChooser* btn, gpointer ptr);
static void on_theme_font_set (GtkFontChooser* btn, gpointer ptr);
static void on_theme_dark_set (GtkRadioButton* btn, gpointer ptr);
static void on_theme_cursor_size_set (GtkComboBox* btn, gpointer ptr);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

void reload_session (void)
{
    if (wm != WM_OPENBOX) vsystem ("pgrep xsettingsd > /dev/null && killall -HUP xsettingsd");
    if (wm == WM_LABWC) vsystem ("labwc --reconfigure");
    if (wm == WM_OPENBOX) vsystem ("openbox --reconfigure");
}

void restore_gsettings (void)
{
    if (wm != WM_OPENBOX)
    {
        cur_conf.cursor_size = orig_csize;
        cur_conf.tb_icon_size = orig_tbsize;
        cur_conf.desktop_font = orig_font;
        save_gsettings ();
    }
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

/*----------------------------------------------------------------------------*/
/* Load / save data                                                           */
/*----------------------------------------------------------------------------*/

char *lxsession_file (gboolean global)
{
    return g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "lxsession", session (), "desktop.conf", NULL);
}

char *xsettings_file (gboolean global)
{
    return g_build_filename (global ? "/etc" : g_get_user_config_dir (), "xsettingsd/xsettingsd.conf", NULL);
}

char *openbox_file (void)
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

static void load_gsettings (void)
{
    char *res;
    int val;

    res = get_quoted_string ("gsettings get org.gnome.desktop.interface font-name");
    if (!res[0]) DEFAULT (desktop_font);
    else cur_conf.desktop_font = g_strdup (res);
    g_free (res);

    res = get_string ("gsettings get org.gnome.desktop.interface cursor-size");
    if (res[0] && sscanf (res, "%d", &val) == 1 && val >= 24 && val <= 48) cur_conf.cursor_size = val;
    else DEFAULT (cursor_size);
    g_free (res);

    res = get_string ("gsettings get org.gnome.desktop.interface toolbar-icons-size");
    if (res[0])
    {
        if (!g_strcmp0 (res, "small")) cur_conf.tb_icon_size = 16;
        else if (!g_strcmp0 (res, "large")) cur_conf.tb_icon_size = 48;
        else cur_conf.tb_icon_size = 24;
    }
    else DEFAULT (tb_icon_size);
    g_free (res);
}

static void load_gtk3_settings (void)
{
    char *user_config_file, *sys_config_file, *cmdbuf, *res;
    int dark;

    cur_conf.darkmode = (is_dark () == 1) ? TRUE : FALSE;

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

static void save_wm_settings (void)
{
    char *user_config_file, *font, *cptr;
    int count, size;
    const gchar *weight = NULL, *style = NULL;
    char buf[10];

    xmlDocPtr xDoc;
    xmlNodePtr root, cur_node, node;
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;

    if (wm == WM_LABWC) user_config_file = labwc_file ();
    else if (wm == WM_OPENBOX) user_config_file = openbox_file ();
    else return;

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

    if (wm == WM_LABWC) save_labwc_to_settings ();
    else
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
}

static void save_gsettings (void)
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
}

void save_gtk3_settings (void)
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

void save_session_settings (void)
{
    set_theme (TEMP_THEME);
    if (wm == WM_OPENBOX) save_lxsession_settings ();
    else 
    {
        save_xsettings ();
        save_gsettings ();
    }
    save_wm_settings ();
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

void save_qt_settings (void)
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

void save_app_settings (void)
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

/*----------------------------------------------------------------------------*/
/* GTK theme manipulation                                                     */
/*----------------------------------------------------------------------------*/

void set_theme (const char *theme)
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
        reload_session ();
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

void reload_theme (long int quit)
{
    g_timeout_add (100, restore_theme, (gpointer) quit);
}

/*----------------------------------------------------------------------------*/
/* Set controls to match data                                                 */
/*----------------------------------------------------------------------------*/

void set_system_controls (void)
{
    // block widget handlers
    g_signal_handler_block (combo_cursor, id_cursor);
    g_signal_handler_block (rb_light, id_dark);

    gtk_font_chooser_set_font (GTK_FONT_CHOOSER (font_system), cur_conf.desktop_font);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_hilite), &cur_conf.theme_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_hilitetext), &cur_conf.themetext_colour[cur_conf.darkmode]);

    if (cur_conf.cursor_size >= 48) gtk_combo_box_set_active (GTK_COMBO_BOX (combo_cursor), 0);
    else if (cur_conf.cursor_size >= 36) gtk_combo_box_set_active (GTK_COMBO_BOX (combo_cursor), 1);
    else gtk_combo_box_set_active (GTK_COMBO_BOX (combo_cursor), 2);

    if (cur_conf.darkmode) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_dark), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_light), TRUE);

    // unblock widget handlers
    g_signal_handler_unblock (combo_cursor, id_cursor);
    g_signal_handler_unblock (rb_light, id_dark);

    if (wm == WM_OPENBOX && cur_conf.cursor_size != orig_csize) gtk_widget_show (label_cursor);
    else gtk_widget_hide (label_cursor);
}

/*----------------------------------------------------------------------------*/
/* Control handlers                                                           */
/*----------------------------------------------------------------------------*/

static void on_theme_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.theme_colour[cur_conf.darkmode]);

    save_session_settings ();
    save_gtk3_settings ();

    reload_session ();
    reload_theme (FALSE);
}

static void on_theme_textcolour_set (GtkColorChooser* btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.themetext_colour[cur_conf.darkmode]);

    save_session_settings ();
    save_gtk3_settings ();

    reload_session ();
    reload_theme (FALSE);
}

static void on_theme_font_set (GtkFontChooser* btn, gpointer ptr)
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

    save_session_settings ();
    for (i = 0; i < ndesks; i++)
        save_pcman_settings (i);
    save_qt_settings ();

    reload_session ();
    reload_panel ();
    reload_desktop ();
    reload_theme (FALSE);
}

static void on_theme_dark_set (GtkRadioButton* btn, gpointer ptr)
{
    if (!system ("pgrep geany > /dev/null"))
    {
        g_signal_handler_block (rb_light, id_dark);
        if (cur_conf.darkmode) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_dark), TRUE);
        else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_light), TRUE);
        message_ok (_("The theme for Geany cannot be changed while it is open.\nPlease close it and try again."));
        g_signal_handler_unblock (rb_light, id_dark);
        return;
    }

    if (!system ("pgrep galculator > /dev/null"))
    {
        g_signal_handler_block (rb_light, id_dark);
        if (cur_conf.darkmode) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_dark), TRUE);
        else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_light), TRUE);
        message_ok (_("The theme for Calculator cannot be changed while it is open.\nPlease close it and try again."));
        g_signal_handler_unblock (rb_light, id_dark);
        return;
    }

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) cur_conf.darkmode = 0;
    else cur_conf.darkmode = 1;
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_hilite), &cur_conf.theme_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_hilitetext), &cur_conf.themetext_colour[cur_conf.darkmode]);
    
    set_taskbar_controls ();

    save_session_settings ();
    save_gtk3_settings ();
    save_app_settings ();

    reload_session ();
    reload_theme (FALSE);
}

static void on_theme_cursor_size_set (GtkComboBox* btn, gpointer ptr)
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

    if (wm == WM_OPENBOX && cur_conf.cursor_size != orig_csize) gtk_widget_show (label_cursor);
    else gtk_widget_hide (label_cursor);

    save_session_settings ();
    reload_session ();
    reload_theme (FALSE);
}

/*----------------------------------------------------------------------------*/
/* Initialisation                                                             */
/*----------------------------------------------------------------------------*/

void load_system_tab (GtkBuilder *builder)
{
    if (wm == WM_OPENBOX) load_lxsession_settings ();
    else load_gsettings ();
    load_obconf_settings ();
    load_gtk3_settings ();

    orig_csize = cur_conf.cursor_size;
    orig_tbsize = cur_conf.tb_icon_size;
    orig_font = g_strdup (cur_conf.desktop_font);

    font_system = (GtkWidget *) gtk_builder_get_object (builder, "fontbutton1");
    g_signal_connect (font_system, "font-set", G_CALLBACK (on_theme_font_set), NULL);

    colour_hilite = (GtkWidget *) gtk_builder_get_object (builder, "colorbutton1");
    g_signal_connect (colour_hilite, "color-set", G_CALLBACK (on_theme_colour_set), NULL);

    colour_hilitetext = (GtkWidget *) gtk_builder_get_object (builder, "colorbutton5");
    g_signal_connect (colour_hilitetext, "color-set", G_CALLBACK (on_theme_textcolour_set), NULL);

    combo_cursor = (GtkWidget *) gtk_builder_get_object (builder, "comboboxtext3");
    id_cursor = g_signal_connect (combo_cursor, "changed", G_CALLBACK (on_theme_cursor_size_set), NULL);

    if (is_dark () != -1)
    {
        rb_light = (GtkWidget *) gtk_builder_get_object (builder, "radiobutton5");
        rb_dark = (GtkWidget *) gtk_builder_get_object (builder, "radiobutton6");
        id_dark = g_signal_connect (rb_light, "toggled", G_CALLBACK (on_theme_dark_set), NULL);
    }
    else gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox35")));

    label_cursor = (GtkWidget *) gtk_builder_get_object (builder, "label36");
    gtk_widget_hide (label_cursor);
}

/* End of file */
/*----------------------------------------------------------------------------*/
