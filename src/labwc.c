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

static char *labwc_file (void);
static void load_labwc_settings (void);
static void load_labwc_to_settings (void);
static void save_labwc_settings (void);
static void save_labwc_to_settings (void);
static void on_labwc_colour_set (GtkColorChooser *btn, gpointer ptr);
static void on_labwc_textcolour_set (GtkColorChooser *btn, gpointer ptr);
static void on_labwc_font_set (GtkFontChooser *btn, gpointer ptr);
static gboolean on_toggle_icon (GtkSwitch *btn, gboolean state, gpointer ptr);
static gboolean on_toggle_cust (GtkSwitch *btn, gboolean state, gpointer ptr);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Load / save data                                                           */
/*----------------------------------------------------------------------------*/

static char *labwc_file (void)
{
    return g_build_filename (g_get_user_config_dir (), "labwc", "rc.xml", NULL);
}

static void load_labwc_settings (void)
{
    char *user_config_file;
    int val;
    xmlChar *font, *size, *weight, *slant, *layout, *place;

    xmlDocPtr xDoc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlNodePtr node, cur;

    DEFAULT (title_font);
    DEFAULT (show_labwc_icon);

    user_config_file = labwc_file ();
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
    xmlXPathFreeObject (xpathObj);

    xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titlebar']/*[local-name()='layout']", xpathCtx);
    node = xpathObj->nodesetval->nodeTab[0];
    if (node)
    {
        layout = xmlNodeGetContent (node);
        if (xmlStrstr (layout, XC ("icon:"))) cur_conf.show_labwc_icon = TRUE;
        else cur_conf.show_labwc_icon = FALSE;
        xmlFree (layout);
    }
    xmlXPathFreeObject (xpathObj);

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

    cmdbuf = g_strdup_printf ("grep window.active.title.bg.color %s | cut -d : -f 2 | xargs", user_config_file);
    res = get_string (cmdbuf);
    if (res[0])
    {
        if (!gdk_rgba_parse (&cur_conf.title_colour[cur_conf.darkmode], res))
            DEFAULT (title_colour[cur_conf.darkmode]);
    }
    else DEFAULT (title_colour[cur_conf.darkmode]);
    g_free (res);
    g_free (cmdbuf);

    cmdbuf = g_strdup_printf ("grep window.active.label.text.color %s | cut -d : -f 2 | xargs", user_config_file);
    res = get_string (cmdbuf);
    if (res[0])
    {
        if (!gdk_rgba_parse (&cur_conf.titletext_colour[cur_conf.darkmode], res))
            DEFAULT (titletext_colour[cur_conf.darkmode]);
    }
    else DEFAULT (titletext_colour[cur_conf.darkmode]);
    g_free (res);
    g_free (cmdbuf);

    g_free (user_config_file);
}

static void save_labwc_settings (void)
{
    char *user_config_file, *icons;
    int count, size;
    const gchar *font = NULL, *weight = NULL, *style = NULL;
    char buf[10];

    xmlDocPtr xDoc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlNodePtr root, cur_node, node;
    xmlChar *place;

    user_config_file = labwc_file ();
    check_directory (user_config_file);

    // set the font description variables for XML from the font name
    PangoFontDescription *pfd = pango_font_description_from_string (cur_conf.title_font);
    font = pango_font_description_get_family (pfd);
    size = pango_font_description_get_size (pfd) / (pango_font_description_get_size_is_absolute (pfd) ? 1 : PANGO_SCALE);
    sprintf (buf, "%d", size);
    PangoWeight pweight = pango_font_description_get_weight (pfd);
    PangoStyle pstyle = pango_font_description_get_style (pfd);

    switch (pweight)
    {
        case PANGO_WEIGHT_THIN :        weight = "Thin";
                                        break;
        case PANGO_WEIGHT_ULTRALIGHT :  weight = "Ultralight";
                                        break;
        case PANGO_WEIGHT_LIGHT :       weight = "Light";
                                        break;
        case PANGO_WEIGHT_SEMILIGHT :   weight = "Semilight";
                                        break;
        case PANGO_WEIGHT_BOOK :        weight = "Book";
                                        break;
        case PANGO_WEIGHT_NORMAL :      weight = "Normal";
                                        break;
        case PANGO_WEIGHT_MEDIUM :      weight = "Medium";
                                        break;
        case PANGO_WEIGHT_SEMIBOLD :    weight = "Semibold";
                                        break;
        case PANGO_WEIGHT_BOLD :        weight = "Bold";
                                        break;
        case PANGO_WEIGHT_ULTRABOLD :   weight = "Ultrabold";
                                        break;
        case PANGO_WEIGHT_HEAVY :       weight = "Heavy";
                                        break;
        case PANGO_WEIGHT_ULTRAHEAVY :  weight = "Ultraheavy";
                                        break;
    }

    switch (pstyle)
    {
        case PANGO_STYLE_NORMAL :       style = "Normal";
                                        break;
        case PANGO_STYLE_ITALIC :       style = "Italic";
                                        break;
        case PANGO_STYLE_OBLIQUE :      style = "Oblique";
                                        break;
    }

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
        cur_node = xmlNewChild (xpathObj->nodesetval->nodeTab[0], NULL, XC ("font"), NULL);
        xmlSetProp (cur_node, XC ("place"), XC ("ActiveWindow"));
        xmlNewChild (cur_node, NULL, XC ("name"), XC (font));
        xmlNewChild (cur_node, NULL, XC ("size"), XC (buf));
        xmlNewChild (cur_node, NULL, XC ("weight"), XC (weight));
        xmlNewChild (cur_node, NULL, XC ("slant"), XC (style));
    }
    else
    {
        for (count = 0; count < xpathObj->nodesetval->nodeNr; count++)
        {
            node = xpathObj->nodesetval->nodeTab[count];
            place = xmlGetProp (node, XC ("place"));
            if (!xmlStrcmp (place, XC ("ActiveWindow")))
            {
                for (cur_node = node->children; cur_node; cur_node = cur_node->next)
                {
                    if (cur_node->type == XML_ELEMENT_NODE)
                    {
                        if (!xmlStrcmp (cur_node->name, XC ("name"))) xmlNodeSetContent (cur_node, XC (font));
                        if (!xmlStrcmp (cur_node->name, XC ("size"))) xmlNodeSetContent (cur_node, XC (buf));
                        if (!xmlStrcmp (cur_node->name, XC ("weight"))) xmlNodeSetContent (cur_node, XC (weight));
                        if (!xmlStrcmp (cur_node->name, XC ("slant")))  xmlNodeSetContent (cur_node, XC (style));
                    }
                }
            }
            xmlFree (place);
        }
    }
    xmlXPathFreeObject (xpathObj);
    pango_font_description_free (pfd);

    icons = g_strdup_printf ("%s:iconify,max,close", cur_conf.show_labwc_icon ? "icon" : "");
    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titlebar']/*[local-name()='layout']"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titlebar']"), xpathCtx);
        if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        {
            xmlXPathFreeObject (xpathObj);
            xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']"), xpathCtx);
            xmlNewChild (xpathObj->nodesetval->nodeTab[0], NULL, XC ("titlebar"), NULL);
            xmlXPathFreeObject (xpathObj);
            xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='titlebar']"), xpathCtx);
        }
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNewChild (cur_node, NULL, XC ("layout"), XC (icons));
    }
    else
    {
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNodeSetContent (cur_node, XC (icons));
    }
    xmlXPathFreeObject (xpathObj);
    g_free (icons);

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

    cstrb = rgba_to_gdk_color_string (&cur_conf.title_colour[cur_conf.darkmode]);
    cstrf = rgba_to_gdk_color_string (&cur_conf.titletext_colour[cur_conf.darkmode]);

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

void set_labwc_controls (void)
{
    g_signal_handler_block (toggle_icon, id_icon);
    gtk_font_chooser_set_font (GTK_FONT_CHOOSER (font_system), cur_conf.title_font);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_hilite), &cur_conf.title_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_hilitetext), &cur_conf.titletext_colour[cur_conf.darkmode]);
    gtk_switch_set_active (GTK_SWITCH (toggle_icon), cur_conf.show_labwc_icon);
    g_signal_handler_unblock (toggle_icon, id_icon);

    PangoFontDescription *pfdd, *pfdt;
    pfdd = pango_font_description_from_string (cur_conf.desktop_font);
    pfdt = pango_font_description_from_string (cur_conf.title_font);

    gboolean cust = FALSE;
    if (!gdk_rgba_equal (&cur_conf.title_colour[0], &cur_conf.theme_colour[0])) cust = TRUE;
    //if (!gdk_rgba_equal (&cur_conf.title_colour[1], &cur_conf.theme_colour[1])) cust = TRUE;
    if (!gdk_rgba_equal (&cur_conf.titletext_colour[0], &cur_conf.themetext_colour[0])) cust = TRUE;
    //if (!gdk_rgba_equal (&cur_conf.titletext_colour[1], &cur_conf.themetext_colour[1])) cust = TRUE;
    if (!pango_font_description_equal (pfdt, pfdd)) cust = TRUE;

    gtk_switch_set_active (GTK_SWITCH (toggle_cust), cust);

    pango_font_description_free (pfdd);
    pango_font_description_free (pfdt);
}

/*----------------------------------------------------------------------------*/
/* Control handlers                                                           */
/*----------------------------------------------------------------------------*/

static void on_labwc_colour_set (GtkColorChooser *btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.title_colour[cur_conf.darkmode]);

    save_labwc_to_settings ();

    reload_session ();
}

static void on_labwc_textcolour_set (GtkColorChooser *btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.titletext_colour[cur_conf.darkmode]);

    save_labwc_to_settings ();

    reload_session ();
}

static void on_labwc_font_set (GtkFontChooser *btn, gpointer ptr)
{
    const char *font = gtk_font_chooser_get_font (btn);
    if (font) cur_conf.title_font = font;

    save_labwc_settings ();

    reload_session ();
}

static gboolean on_toggle_icon (GtkSwitch *btn, gboolean state, gpointer ptr)
{
    cur_conf.show_labwc_icon = state;

    save_labwc_settings ();

    reload_session ();

    return FALSE;
}

static gboolean on_toggle_cust (GtkSwitch *btn, gboolean state, gpointer ptr)
{
    gtk_widget_set_sensitive (colour_hilite, state);
    gtk_widget_set_sensitive (colour_hilitetext, state);
    gtk_widget_set_sensitive (font_system, state);

    if (!state)
    {
        cur_conf.title_font = cur_conf.desktop_font;
        cur_conf.title_colour[0] = cur_conf.theme_colour[0];
        cur_conf.title_colour[1] = cur_conf.theme_colour[1];
        cur_conf.titletext_colour[0] = cur_conf.themetext_colour[0];
        cur_conf.titletext_colour[1] = cur_conf.themetext_colour[1];

        set_labwc_controls ();

        save_labwc_settings ();
        save_labwc_to_settings ();
        reload_session ();
    }
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Initialisation                                                             */
/*----------------------------------------------------------------------------*/

void load_labwc_tab (GtkBuilder *builder)
{
    load_labwc_settings ();
    load_labwc_to_settings ();

    font_system = (GtkWidget *) gtk_builder_get_object (builder, "fontbutton2");
    g_signal_connect (font_system, "font-set", G_CALLBACK (on_labwc_font_set), NULL);

    colour_hilite = (GtkWidget *) gtk_builder_get_object (builder, "colorbutton7");
    g_signal_connect (colour_hilite, "color-set", G_CALLBACK (on_labwc_colour_set), NULL);

    colour_hilitetext = (GtkWidget *) gtk_builder_get_object (builder, "colorbutton8");
    g_signal_connect (colour_hilitetext, "color-set", G_CALLBACK (on_labwc_textcolour_set), NULL);

    toggle_icon = (GtkWidget *) gtk_builder_get_object (builder, "switch4");
    id_icon = g_signal_connect (toggle_icon, "state-set", G_CALLBACK (on_toggle_icon), NULL);

    toggle_cust = (GtkWidget *) gtk_builder_get_object (builder, "switch5");
    id_cust = g_signal_connect (toggle_cust, "state-set", G_CALLBACK (on_toggle_cust), NULL);
}

/* End of file */
/*----------------------------------------------------------------------------*/
