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
static GtkWidget *colour_hilite, *colour_hilitetext, *font_system;

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
    char *user_config_file, *font, *size, *weight, *slant;
    int val;
    
    // set a default font here?

    user_config_file = labwc_file ();
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
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='font']", xpathCtx);
    xmlNode *node;
    for (val = 0; val < 2; val++)
    {
        node = xpathObj->nodesetval->nodeTab[val];
        if (node)
        {
            xmlChar *place = xmlGetProp (node, XC ("place"));
            if (!xmlStrcmp (place, XC ("ActiveWindow")))
            {
                for (xmlNode *cur = node->children; cur != NULL; cur = cur->next)
                {
                    if (!xmlStrcmp (cur->name, XC ("name"))) font = (char *) xmlNodeGetContent (cur);
                    if (!xmlStrcmp (cur->name, XC ("size"))) size = (char *) xmlNodeGetContent (cur);
                    if (!xmlStrcmp (cur->name, XC ("weight"))) weight = (char *) xmlNodeGetContent (cur);
                    if (!xmlStrcmp (cur->name, XC ("slant"))) slant = (char *) xmlNodeGetContent (cur);
                }
                cur_conf.title_font = g_strdup_printf ("%s %s %s %s", font, weight, slant, size);
            }
            g_free (place);
        }
    }

    g_free (font);
    g_free (size);
    g_free (weight);
    g_free (slant);  

    // cleanup XML
    xmlXPathFreeObject (xpathObj);
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
    char *user_config_file, *font, *cptr;
    int count, size;
    const gchar *weight = NULL, *style = NULL;
    char buf[10];

    xmlDocPtr xDoc;
    xmlNodePtr root, cur_node, node;
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;

    if (wm == WM_LABWC) user_config_file = labwc_file ();
    else return;

    check_directory (user_config_file);

    // set the font description variables for XML from the font name
    PangoFontDescription *pfd = pango_font_description_from_string (cur_conf.title_font);
    size = pango_font_description_get_size (pfd) / (pango_font_description_get_size_is_absolute (pfd) ? 1 : PANGO_SCALE);
    PangoWeight pweight = pango_font_description_get_weight (pfd);
    PangoStyle pstyle = pango_font_description_get_style (pfd);

    if (wm == WM_OPENBOX)
    {
        // openbox only recognises bold and italic - anything else is ignored
        if (pweight == PANGO_WEIGHT_BOLD) weight = "Bold";
        else weight = "Normal";

        if (pstyle == PANGO_STYLE_ITALIC) style = "Italic";
        else style = "Normal";
    }
    else
    {
        // labwc (now) recognises all weights and styles
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
    }
    pango_font_description_unset_fields (pfd, PANGO_FONT_MASK_WEIGHT);
    pango_font_description_unset_fields (pfd, PANGO_FONT_MASK_STYLE);
    pango_font_description_unset_fields (pfd, PANGO_FONT_MASK_STRETCH);

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

        g_free (cstrf);
        g_free (cstrb);
        g_free (user_config_file);
        return;
    }

    // amend entries already in file, or add if not present
    LABWC_THEME_UPDATE ("window.active.title.bg.color", cstrb);
    LABWC_THEME_UPDATE ("window.active.label.text.color", cstrf);
    LABWC_THEME_UPDATE ("window.active.button.unpressed.image.color", cstrf);

    g_free (cstrf);
    g_free (cstrb);
    g_free (user_config_file);
}

/*----------------------------------------------------------------------------*/
/* Set controls to match data                                                 */
/*----------------------------------------------------------------------------*/

void set_labwc_controls (void)
{
    gtk_font_chooser_set_font (GTK_FONT_CHOOSER (font_system), cur_conf.title_font);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_hilite), &cur_conf.title_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_hilitetext), &cur_conf.titletext_colour[cur_conf.darkmode]);
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
}

/* End of file */
/*----------------------------------------------------------------------------*/
