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
#include "taskbar.h"
#include "desktop.h"
#include "defaults.h"
#include "system.h"

#include "labwc.h"

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

/* Controls */
static GtkWidget *colour_hilite, *colour_hilitetext, *font_system, *toggle_icon, *toggle_cust;

/* Handler IDs */
static gulong id_icon, id_cust;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static char *openbox_file (void);
static char *labwc_file (void);
static void get_font (const char *desc, char **font, char **weight, char **style, char **size);
static void set_xml_theme_parameter (xmlXPathContextPtr xpathCtx, const char *name, const char *value);
static void get_xml_theme_parameter (xmlXPathContextPtr xpathCtx, const char *name, char **value);
static void load_wm_settings (void);
static void load_labwc_to_settings (void);
static void save_labwc_to_settings (void);
static void on_wm_colour_set (GtkColorChooser *btn, gpointer);
static void on_wm_textcolour_set (GtkColorChooser *btn, gpointer);
static void on_wm_font_set (GtkFontChooser *btn, gpointer);
static void on_toggle_icon (GtkSwitch *btn, gpointer, gpointer);
static void on_toggle_cust (GtkSwitch *btn, gpointer, gpointer);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

static char *openbox_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "openbox", "rpd-rc.xml", NULL);
}

static char *labwc_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "labwc", "rc.xml", NULL);
}

static void get_font (const char *desc, char **font, char **weight, char **style, char **size)
{
    // set the font description variables for XML from the font name
    PangoFontDescription *pfd = pango_font_description_from_string (desc);
    *font = g_strdup (pango_font_description_get_family (pfd));
    *size = g_strdup_printf ("%d", pango_font_description_get_size (pfd) / (pango_font_description_get_size_is_absolute (pfd) ? 1 : PANGO_SCALE));
    PangoWeight pweight = pango_font_description_get_weight (pfd);
    PangoStyle pstyle = pango_font_description_get_style (pfd);

    if (wm == WM_OPENBOX)
    {
        // openbox only recognises bold and italic - anything else is ignored
        if (pweight == PANGO_WEIGHT_BOLD) *weight = g_strdup ("Bold");
        else *weight = g_strdup ("Normal");

        if (pstyle == PANGO_STYLE_ITALIC) *style = g_strdup ("Italic");
        else *style = g_strdup ("Normal");
    }
    else
    {
        // labwc recognises all weights and styles
        switch (pweight)
        {
            case PANGO_WEIGHT_THIN :        *weight = g_strdup ("Thin");
                                            break;
            case PANGO_WEIGHT_ULTRALIGHT :  *weight = g_strdup ("Ultralight");
                                            break;
            case PANGO_WEIGHT_LIGHT :       *weight = g_strdup ("Light");
                                            break;
            case PANGO_WEIGHT_SEMILIGHT :   *weight = g_strdup ("Semilight");
                                            break;
            case PANGO_WEIGHT_BOOK :        *weight = g_strdup ("Book");
                                            break;
            case PANGO_WEIGHT_NORMAL :      *weight = g_strdup ("Normal");
                                            break;
            case PANGO_WEIGHT_MEDIUM :      *weight = g_strdup ("Medium");
                                            break;
            case PANGO_WEIGHT_SEMIBOLD :    *weight = g_strdup ("Semibold");
                                            break;
            case PANGO_WEIGHT_BOLD :        *weight = g_strdup ("Bold");
                                            break;
            case PANGO_WEIGHT_ULTRABOLD :   *weight = g_strdup ("Ultrabold");
                                            break;
            case PANGO_WEIGHT_HEAVY :       *weight = g_strdup ("Heavy");
                                            break;
            case PANGO_WEIGHT_ULTRAHEAVY :  *weight = g_strdup ("Ultraheavy");
                                            break;
        }

        switch (pstyle)
        {
            case PANGO_STYLE_NORMAL :       *style = g_strdup ("Normal");
                                            break;
            case PANGO_STYLE_ITALIC :       *style = g_strdup ("Italic");
                                            break;
            case PANGO_STYLE_OBLIQUE :      *style = g_strdup ("Oblique");
                                            break;
        }
    }
    pango_font_description_free (pfd);
}

static void set_xml_theme_parameter (xmlXPathContextPtr xpathCtx, const char *name, const char *value)
{
    xmlXPathObjectPtr xpathObj;
    xmlNodePtr node;

    char *path = g_strdup_printf ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='%s']", name);

    xpathObj = xmlXPathEvalExpression (XC (path), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']"), xpathCtx);
        node = xpathObj->nodesetval->nodeTab[0];
        xmlNewChild (node, NULL, XC (name), XC (value));
    }
    else
    {
        node = xpathObj->nodesetval->nodeTab[0];
        xmlNodeSetContent (node, XC (value));
    }
    xmlXPathFreeObject (xpathObj);

    g_free (path);
}

static void get_xml_theme_parameter (xmlXPathContextPtr xpathCtx, const char *name, char **value)
{
    xmlXPathObjectPtr xpathObj;
    xmlNodePtr node;
    xmlChar *xstr;

    char *path = g_strdup_printf ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='%s']", name);

    xpathObj = xmlXPathEvalExpression (XC (path), xpathCtx);
    if (xpathObj->nodesetval)
    {
        node = xpathObj->nodesetval->nodeTab[0];
        if (node)
        {
            xstr = xmlNodeGetContent (node);
            *value = g_strdup ((char *) xstr);
            xmlFree (xstr);
        }
    }
    xmlXPathFreeObject (xpathObj);

    g_free (path);
}

/*----------------------------------------------------------------------------*/
/* Load / save data                                                           */
/*----------------------------------------------------------------------------*/

static void load_wm_settings (void)
{
    char *user_config_file, *res;
    int val;
    xmlChar *font, *size, *weight, *slant, *place, *xstr;

    xmlDocPtr xDoc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlNodePtr node, cur;

    cur_conf.title_font = cur_conf.desktop_font;
    cur_conf.title_colour = cur_conf.theme_colour[cur_conf.darkmode];
    cur_conf.titletext_colour = cur_conf.themetext_colour[cur_conf.darkmode];
    DEFAULT (show_labwc_icon);

    if (wm == WM_LABWC) user_config_file = labwc_file ();
    else if (wm == WM_OPENBOX) user_config_file = openbox_file ();
    else return;
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        g_free (user_config_file);
        return;
    }

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    xDoc = xmlParseFile (user_config_file);
    if (xDoc == NULL)
    {
        g_free (user_config_file);
        return;
    }
    xpathCtx = xmlXPathNewContext (xDoc);

    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='font']", xpathCtx);
    if (xpathObj->nodesetval)
    {
        for (val = 0; val < 2; val++)
        {
            node = xpathObj->nodesetval->nodeTab[val];
            if (node)
            {
                place = xmlGetProp (node, XC ("place"));
                if (!xmlStrcmp (place, XC ("ActiveWindow")))
                {
                    for (cur = node->children; cur != NULL; cur = cur->next)
                    {
                        if (!xmlStrcmp (cur->name, XC ("name"))) font = xmlNodeGetContent (cur);
                        if (!xmlStrcmp (cur->name, XC ("size"))) size = xmlNodeGetContent (cur);
                        if (!xmlStrcmp (cur->name, XC ("weight"))) weight = xmlNodeGetContent (cur);
                        if (!xmlStrcmp (cur->name, XC ("slant"))) slant = xmlNodeGetContent (cur);
                    }
                    cur_conf.title_font = g_strdup_printf ("%s %s %s %s", font, weight, slant, size);

                    xmlFree (font);
                    xmlFree (size);
                    xmlFree (weight);
                    xmlFree (slant);
                }
                xmlFree (place);
            }
        }
    }
    xmlXPathFreeObject (xpathObj);

    if (wm == WM_LABWC)
    {
        load_labwc_to_settings ();

        xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titlebar']/*[local-name()='layout']", xpathCtx);
        if (xpathObj->nodesetval)
        {
            node = xpathObj->nodesetval->nodeTab[0];
            if (node)
            {
                xstr = xmlNodeGetContent (node);
                if (xmlStrstr (xstr, XC ("icon:"))) cur_conf.show_labwc_icon = TRUE;
                else cur_conf.show_labwc_icon = FALSE;
                xmlFree (xstr);
            }
        }
        xmlXPathFreeObject (xpathObj);
    }
    else if (wm == WM_OPENBOX)
    {
        get_xml_theme_parameter (xpathCtx, "titleColor", &res);
        if (!gdk_rgba_parse (&cur_conf.title_colour, res)) cur_conf.title_colour = cur_conf.theme_colour[cur_conf.darkmode];
        g_free (res);

        get_xml_theme_parameter (xpathCtx, "textColor", &res);
        if (!gdk_rgba_parse (&cur_conf.titletext_colour, res)) cur_conf.titletext_colour = cur_conf.themetext_colour[cur_conf.darkmode];
        g_free (res);

        get_xml_theme_parameter (xpathCtx, "titleLayout", &res);
        if (strstr (res, "N")) cur_conf.show_labwc_icon = TRUE;
        else cur_conf.show_labwc_icon = FALSE;
        g_free (res);

        get_xml_theme_parameter (xpathCtx, "invHandleWidth", &res);
        if (sscanf (res, "%d", &val) == 1 && val > 0) cur_conf.handle_width = val;
        g_free (res);
    }

    // cleanup XML
    xmlXPathFreeContext (xpathCtx);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_config_file);
}

static void load_labwc_to_settings (void)
{
    char *user_config_file, *cmdbuf, *res;

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "labwc", "themerc-override", NULL);
    check_directory (user_config_file);

    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        cur_conf.title_colour = cur_conf.theme_colour[cur_conf.darkmode];
        cur_conf.titletext_colour = cur_conf.themetext_colour[cur_conf.darkmode];
        g_free (user_config_file);
        return;
    }

    cmdbuf = g_strdup_printf ("grep window.active.title.bg.color %s | cut -d : -f 2 | xargs", user_config_file);
    res = get_string (cmdbuf);
    if (!res[0] || !gdk_rgba_parse (&cur_conf.title_colour, res)) cur_conf.title_colour = cur_conf.theme_colour[cur_conf.darkmode];
    g_free (res);
    g_free (cmdbuf);

    cmdbuf = g_strdup_printf ("grep window.active.label.text.color %s | cut -d : -f 2 | xargs", user_config_file);
    res = get_string (cmdbuf);
    if (!res[0] || !gdk_rgba_parse (&cur_conf.titletext_colour, res)) cur_conf.titletext_colour = cur_conf.themetext_colour[cur_conf.darkmode];
    g_free (res);
    g_free (cmdbuf);

    g_free (user_config_file);
}

void save_wm_settings (void)
{
    char *user_config_file, *cptr;
    char *font = NULL, *weight = NULL, *style = NULL, *size = NULL;
    int count;

    xmlDocPtr xDoc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlNodePtr root, cur_node, node;
    xmlChar *place;

    if (wm == WM_LABWC) user_config_file = labwc_file ();
    else if (wm == WM_OPENBOX) user_config_file = openbox_file ();
    else return;

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

    // fonts for active and inactive window headers
    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='font']"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']"), xpathCtx);
        for (count = (cur_conf.custom_tb == FALSE ? 0 : 1); count < 2; count ++)
        {
            cur_node = xmlNewChild (xpathObj->nodesetval->nodeTab[0], NULL, XC ("font"), NULL);

            xmlSetProp (cur_node, XC ("place"), count == 0 ? XC ("ActiveWindow") : XC ("InactiveWindow"));

            get_font (count == 0 ? cur_conf.title_font : cur_conf.desktop_font, &font, &weight, &style, &size);

            xmlNewChild (cur_node, NULL, XC ("name"), XC (font));
            xmlNewChild (cur_node, NULL, XC ("size"), XC (size));
            xmlNewChild (cur_node, NULL, XC ("weight"), XC (weight));
            xmlNewChild (cur_node, NULL, XC ("slant"), XC (style));

            g_free (font);
            g_free (weight);
            g_free (style);
            g_free (size);
        }
    }
    else
    {
        for (count = 0; count < xpathObj->nodesetval->nodeNr; count++)
        {
            node = xpathObj->nodesetval->nodeTab[count];
            place = xmlGetProp (node, XC ("place"));
            if (!xmlStrcmp (place, XC ("ActiveWindow")))
                get_font (cur_conf.title_font, &font, &weight, &style, &size);
            else if (!xmlStrcmp (place, XC ("InactiveWindow")))
                get_font (cur_conf.desktop_font, &font, &weight, &style, &size);
            else
            {
                xmlFree (place);
                continue;
            }

            for (cur_node = node->children; cur_node; cur_node = cur_node->next)
            {
                if (cur_node->type == XML_ELEMENT_NODE)
                {
                    if (!xmlStrcmp (cur_node->name, XC ("name"))) xmlNodeSetContent (cur_node, XC (font));
                    if (!xmlStrcmp (cur_node->name, XC ("size"))) xmlNodeSetContent (cur_node, XC (size));
                    if (!xmlStrcmp (cur_node->name, XC ("weight"))) xmlNodeSetContent (cur_node, XC (weight));
                    if (!xmlStrcmp (cur_node->name, XC ("slant")))  xmlNodeSetContent (cur_node, XC (style));
                }
            }

            g_free (font);
            g_free (weight);
            g_free (style);
            g_free (size);
            xmlFree (place);
        }
    }
    xmlXPathFreeObject (xpathObj);

    cptr = g_strdup_printf ("%s%s", theme_name (cur_conf.darkmode), cur_conf.scrollbar_width >= 17 ? "_l" : "");
    set_xml_theme_parameter (xpathCtx, "name", cptr);
    g_free (cptr);

    if (wm == WM_LABWC)
    {
        save_labwc_to_settings ();

        cptr = g_strdup_printf ("%s:iconify,max,close", cur_conf.show_labwc_icon ? "icon" : "");
        set_xml_theme_parameter (xpathCtx, "titlebar", NULL);
        xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titlebar']/*[local-name()='layout']"), xpathCtx);
        if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        {
            xmlXPathFreeObject (xpathObj);
            xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titlebar']"), xpathCtx);
            cur_node = xpathObj->nodesetval->nodeTab[0];
            xmlNewChild (cur_node, NULL, XC ("layout"), XC (cptr));
        }
        else
        {
            cur_node = xpathObj->nodesetval->nodeTab[0];
            xmlNodeSetContent (cur_node, XC (cptr));
        }
        xmlXPathFreeObject (xpathObj);
        g_free (cptr);
    }
    else
    {
        cptr = rgba_to_gdk_color_string (&cur_conf.title_colour);
        set_xml_theme_parameter (xpathCtx, "titleColor", cptr);
        g_free (cptr);

        cptr = rgba_to_gdk_color_string (&cur_conf.titletext_colour);
        set_xml_theme_parameter (xpathCtx, "textColor", cptr);
        g_free (cptr);

        cptr = g_strdup_printf ("%sLIMC", cur_conf.show_labwc_icon ? "N" : "");
        set_xml_theme_parameter (xpathCtx, "titleLayout", cptr);
        g_free (cptr);

        cptr = g_strdup_printf ("%d", cur_conf.handle_width);
        set_xml_theme_parameter (xpathCtx, "invHandleWidth", cptr);
        g_free (cptr);
    }

    // cleanup XML
    xmlXPathFreeContext (xpathCtx);
    xmlSaveFile (user_config_file, xDoc);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_config_file);
}

#define LABWC_THEME_UPDATE(param,value) \
    if (vsystem ("grep -q %s %s\n", param, user_config_file)) \
        vsystem ("echo '%s: %s' >> %s", param, value, user_config_file); \
    else \
        vsystem ("sed -i s/'%s.*'/'%s: %s'/g %s", param, param, value, user_config_file);

static void save_labwc_to_settings (void)
{
    char *user_config_file, *cstrb, *cstrf;

    // construct the file path
    user_config_file = g_build_filename (g_get_user_config_dir (), "labwc", "themerc-override", NULL);
    check_directory (user_config_file);

    cstrb = rgba_to_gdk_color_string (&cur_conf.title_colour);
    cstrf = rgba_to_gdk_color_string (&cur_conf.titletext_colour);

    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        vsystem ("echo 'window.active.title.bg.color: %s' >> %s", cstrb, user_config_file);
        vsystem ("echo 'window.active.label.text.color: %s' >> %s", cstrf, user_config_file);
        vsystem ("echo 'window.active.button.unpressed.image.color: %s' >> %s", cstrf, user_config_file);
    }
    else
    {
        // amend entries already in file, or add if not present
        LABWC_THEME_UPDATE ("window.active.title.bg.color", cstrb);
        LABWC_THEME_UPDATE ("window.active.label.text.color", cstrf);
        LABWC_THEME_UPDATE ("window.active.button.unpressed.image.color", cstrf);
    }

    g_free (cstrf);
    g_free (cstrb);
    g_free (user_config_file);
}

/*----------------------------------------------------------------------------*/
/* Set controls to match data                                                 */
/*----------------------------------------------------------------------------*/

void set_wm_controls (void)
{
    g_signal_handler_block (toggle_icon, id_icon);
    g_signal_handler_block (toggle_cust, id_cust);
    gtk_font_chooser_set_font (GTK_FONT_CHOOSER (font_system), cur_conf.title_font);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_hilite), &cur_conf.title_colour);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_hilitetext), &cur_conf.titletext_colour);
    gtk_switch_set_active (GTK_SWITCH (toggle_icon), cur_conf.show_labwc_icon);
    gtk_switch_set_active (GTK_SWITCH (toggle_cust), cur_conf.custom_tb);
    g_signal_handler_unblock (toggle_icon, id_icon);
    g_signal_handler_unblock (toggle_cust, id_cust);

    if (!cur_conf.custom_tb)
    {
        gtk_widget_set_tooltip_text (font_system, _("Turn on 'Customise Active Window' to set font"));
        gtk_widget_set_tooltip_text (colour_hilite, _("Turn on 'Customise Active Window' to set colour"));
        gtk_widget_set_tooltip_text (colour_hilitetext, _("Turn on 'Customise Active Window' to set colour"));
    }
    else
    {
        gtk_widget_set_tooltip_text (font_system, _("Choose the font used for the titlebar of the active window"));
        gtk_widget_set_tooltip_text (colour_hilite, _("Choose the colour used for the titlebar of the active window"));
        gtk_widget_set_tooltip_text (colour_hilitetext, _("Choose the colour used for text on the titlebar of the active window"));
    }

    gtk_widget_set_sensitive (colour_hilite, cur_conf.custom_tb);
    gtk_widget_set_sensitive (colour_hilitetext, cur_conf.custom_tb);
    gtk_widget_set_sensitive (font_system, cur_conf.custom_tb);
}

/*----------------------------------------------------------------------------*/
/* Control handlers                                                           */
/*----------------------------------------------------------------------------*/

static void on_wm_colour_set (GtkColorChooser *btn, gpointer)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.title_colour);

    save_wm_settings ();
    reload_session ();
}

static void on_wm_textcolour_set (GtkColorChooser *btn, gpointer)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.titletext_colour);

    save_wm_settings ();
    reload_session ();
}

static void on_wm_font_set (GtkFontChooser *btn, gpointer)
{
    const char *font = gtk_font_chooser_get_font (btn);
    if (font) cur_conf.title_font = font;

    save_wm_settings ();
    reload_session ();
}

static void on_toggle_icon (GtkSwitch *btn, gpointer, gpointer)
{
    cur_conf.show_labwc_icon = gtk_switch_get_active (btn);

    save_wm_settings ();
    reload_session ();
}

static void on_toggle_cust (GtkSwitch *btn, gpointer, gpointer)
{
    cur_conf.custom_tb = gtk_switch_get_active (btn);

    if (!cur_conf.custom_tb)
    {
        cur_conf.title_font = cur_conf.desktop_font;
        cur_conf.title_colour = cur_conf.theme_colour[cur_conf.darkmode];
        cur_conf.titletext_colour = cur_conf.themetext_colour[cur_conf.darkmode];

        save_wm_settings ();
        reload_session ();
    }

    set_wm_controls ();
}

/*----------------------------------------------------------------------------*/
/* Initialisation                                                             */
/*----------------------------------------------------------------------------*/

void load_wm_tab (GtkBuilder *builder)
{
    load_wm_settings ();

    cur_conf.custom_tb = FALSE;

    if (!gdk_rgba_equal (&cur_conf.title_colour, &cur_conf.theme_colour[cur_conf.darkmode])) cur_conf.custom_tb = TRUE;
    if (!gdk_rgba_equal (&cur_conf.titletext_colour, &cur_conf.themetext_colour[cur_conf.darkmode])) cur_conf.custom_tb = TRUE;

    PangoFontDescription *pfdd, *pfdt;
    pfdd = pango_font_description_from_string (cur_conf.desktop_font);
    pfdt = pango_font_description_from_string (cur_conf.title_font);
    if (!pango_font_description_equal (pfdt, pfdd)) cur_conf.custom_tb = TRUE;
    pango_font_description_free (pfdd);
    pango_font_description_free (pfdt);

    font_system = (GtkWidget *) gtk_builder_get_object (builder, "fontbutton2");
    g_signal_connect (font_system, "font-set", G_CALLBACK (on_wm_font_set), NULL);

    colour_hilite = (GtkWidget *) gtk_builder_get_object (builder, "colorbutton7");
    g_signal_connect (colour_hilite, "color-set", G_CALLBACK (on_wm_colour_set), NULL);

    colour_hilitetext = (GtkWidget *) gtk_builder_get_object (builder, "colorbutton8");
    g_signal_connect (colour_hilitetext, "color-set", G_CALLBACK (on_wm_textcolour_set), NULL);

    toggle_icon = (GtkWidget *) gtk_builder_get_object (builder, "switch4");
    id_icon = g_signal_connect (toggle_icon, "notify::active", G_CALLBACK (on_toggle_icon), NULL);

    toggle_cust = (GtkWidget *) gtk_builder_get_object (builder, "switch5");
    id_cust = g_signal_connect (toggle_cust, "notify::active", G_CALLBACK (on_toggle_cust), NULL);
}

/* End of file */
/*----------------------------------------------------------------------------*/
