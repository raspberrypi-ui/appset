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

#define MAX_ICON 36
#define MIN_ICON 16

#define DEFAULT_SES "LXDE-pi"

/* Shell commands to reload data */
#define RELOAD_LXPANEL "lxpanelctl refresh"
#define RELOAD_OPENBOX "openbox --reconfigure"
#define RELOAD_PCMANFM "pcmanfm --reconfigure"
#define RELOAD_LXSESSION "lxsession -r"

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
static int tb_icon_size, orig_tb_icon_size;
static int lo_icon_size, orig_lo_icon_size;

/* Flag to indicate whether lxsession is version 4.9 or later, in which case no need to refresh manually */

static char needs_refresh;

/* Controls */
static GObject *hcol, *htcol, *font, *dcol, *dtcol, *dmod, *dpic, *barh, *bcol, *btcol, *rb1, *rb2, *rb3, *rb4, *rb5, *cb1, *cb2, *cb3;

static void backup_values (void);
static int restore_values (void);
static void check_themes (void);
static void load_lxsession_settings (void);
static void load_pcman_settings (void);
static void load_lxpanel_settings (void);
static void load_obpix_settings (void);
static void load_lxterm_settings (void);
static void load_libreoffice_settings (void);
static void save_lxpanel_settings (void);
static void save_gtk3_settings (void);
static void save_obpix_settings (void);
static void save_lxsession_settings (void);
static void save_pcman_settings (void);
static void save_obconf_settings (void);
static void save_lxterm_settings (void);
static void save_libreoffice_settings (void);
static void set_openbox_theme (const char *theme);
static void set_lxsession_theme (const char *theme);
static void on_menu_size_set (GtkRadioButton* btn, gpointer ptr);
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
static void on_set_defaults (GtkButton* btn, gpointer ptr);

static void read_version (char *package, int *maj, int *min, int *sub)
{
    char buf[512];
    FILE *fp;

    *maj = *min = *sub = 0;
    sprintf (buf, "dpkg -s %s | grep Version", package);
    fp = popen (buf, "r");
    if (fp == NULL) return;

    while (fgets (buf, sizeof (buf) - 1, fp) != NULL)
        sscanf (buf, "Version: %d.%d.%d.", maj, min, sub);

    pclose (fp);
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
    orig_tb_icon_size = tb_icon_size;
    orig_terminal_font = terminal_font;
    orig_lo_icon_size = lo_icon_size;
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
    return ret;
}

/* Functions to load required values from user config files */

static void check_themes (void)
{
    const char *session_name, *ret;
    char *user_config_file, *cptr, *nptr, *fname;
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
        if (err == NULL) orig_lxsession_theme = ret;
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
        xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']", xpathCtx);

        // find relevant node and read value
        for (count = 0; count < xpathObj->nodesetval->nodeNr; count++)
        {
            xmlNode *node = xpathObj->nodesetval->nodeTab[count];
            xmlAttr *attr = node->properties;
            xmlNode *cur_node = NULL;
            for (cur_node = node->children; cur_node; cur_node = cur_node->next)
            {
                if (cur_node->type == XML_ELEMENT_NODE)
                {
                    if (!strcmp (cur_node->name, "name"))
                        orig_openbox_theme = xmlNodeGetContent (cur_node);
                }
            }
        }

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
        if (needs_refresh) system (RELOAD_LXSESSION);
    }
    if (strcmp ("PiX", orig_openbox_theme))
    {
        set_openbox_theme ("PiX");
        system (RELOAD_OPENBOX);
    }
}

static void load_lxsession_settings (void)
{
    const char *session_name, *ret;
    char *user_config_file, *cptr, *nptr;
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
        desktop_font = "<not set>";
        return;
    }

    // get data from the key file
    err = NULL;
    ret = g_key_file_get_string (kf, "GTK", "sGtk/FontName", &err);
    if (err == NULL) desktop_font = ret;
    else desktop_font = "<not set>";

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
            nptr = strtok (NULL, ":\n");
        }
    }
    else
    {
        gdk_color_parse ("#000000", &bartext_colour);
        gdk_color_parse ("#EDECEB", &bar_colour);
    }

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

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_pcman_settings (void)
{
    const char *session_name, *ret;
    char *user_config_file;
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

    err = NULL;
    ret = g_key_file_get_string (kf, "*", "desktop_fg", &err);
    if (err == NULL)
    {
        if (!gdk_color_parse (ret, &desktoptext_colour))
            gdk_color_parse ("#000000", &desktoptext_colour);
    }
    else gdk_color_parse ("#000000", &desktoptext_colour);

    err = NULL;
    ret = g_key_file_get_string (kf, "*", "wallpaper", &err);
    if (err == NULL && ret) desktop_picture = ret;
    else desktop_picture = "<not set>";

    err = NULL;
    ret = g_key_file_get_string (kf, "*", "wallpaper_mode", &err);
    if (err == NULL && ret) desktop_mode = ret;
    else desktop_mode = "color";

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

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_lxpanel_settings (void)
{
    const char *session_name;
    const char * const *sys_dirs;
    char *user_config_file;
    char linbuf[256], posbuf[16];
    FILE *fp;
    int val;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxpanel/", session_name, "/panels/panel", NULL);

    // open the file
    fp = g_fopen (user_config_file, "rb");
    g_free (user_config_file);
    if (!fp) 
    {
        // set defaults if not read from file
        icon_size = MAX_ICON;
        barpos = 0;
        return;
    }
    
    // read data from the file
    barpos = 0;
    while (1)
    {
        if (!fgets (linbuf, 256, fp)) break;
        if (sscanf (linbuf, "%*[ \t]iconsize=%d", &val) == 1) icon_size = val;
        if (sscanf (linbuf, "%*[ \t]edge=%s", posbuf) == 1)
        {
            if (!strcmp (posbuf, "bottom")) barpos = 1;
        }
    }
    fclose (fp);
}

static void load_obpix_settings (void)
{
    char *user_config_file;
    char linbuf[256], colstr[16];
    FILE *fp;

    // open the file
    user_config_file = g_build_filename (g_get_home_dir (), ".themes/PiX/openbox-3/themerc", NULL);
    fp = g_fopen (user_config_file, "rb");
    g_free (user_config_file);
    if (!fp) return;

    // set defaults in case read fails
    gdk_color_parse ("#4D98F5", &theme_colour);
    gdk_color_parse ("#FFFFFF", &themetext_colour);

    // read data from the file
    while (1)
    {
        if (!fgets (linbuf, 256, fp)) break;
        if (sscanf (linbuf, "window.active.title.bg.color: %s", colstr) == 1)
        {
            if (!gdk_color_parse (colstr, &theme_colour))
                gdk_color_parse ("#4D98F5", &theme_colour);
        }
        if (sscanf (linbuf, "window.active.label.text.color: %s", colstr) == 1)
        {
            if (!gdk_color_parse (colstr, &themetext_colour))
                gdk_color_parse ("#FFFFFF", &themetext_colour);
        }
    }
    fclose (fp);
}

static void load_lxterm_settings (void)
{
    const char *ret;
    char *user_config_file;
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
    if (err == NULL) terminal_font = ret;
    else terminal_font = "Monospace 10";

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
        lo_icon_size = 2;
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

/* Functions to save settings back to relevant files */

static void save_lxpanel_settings (void)
{
    const char *session_name;
    char *user_config_file;
    char cmdbuf[256];

    // sanity check
    if (icon_size > MAX_ICON || icon_size < MIN_ICON) return;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxpanel/", session_name, "/panels/panel", NULL);

    // use sed to write
    sprintf (cmdbuf, "sed -i s/iconsize=../iconsize=%d/g %s", icon_size, user_config_file);
    system (cmdbuf);
    sprintf (cmdbuf, "sed -i s/height=../height=%d/g %s", icon_size, user_config_file);
    system (cmdbuf);
    sprintf (cmdbuf, "sed -i s/edge=.*/edge=%s/g %s", barpos ? "bottom" : "top", user_config_file);
    system (cmdbuf);

    g_free (user_config_file);
}

static void save_obpix_settings (void)
{
    char *user_config_file, *cstr;
    char cmdbuf[256];

    // construct the file path
    user_config_file = g_build_filename (g_get_home_dir (), ".themes/PiX/openbox-3/themerc", NULL);

    // convert colour to string and use sed to write
    cstr = gdk_color_to_string (&theme_colour);
    sprintf (cmdbuf, "sed -i s/'window.active.title.bg.color: #......'/'window.active.title.bg.color: #%c%c%c%c%c%c'/g %s",
        cstr[1], cstr[2], cstr[5], cstr[6], cstr[9], cstr[10], user_config_file);
    system (cmdbuf);

    cstr = gdk_color_to_string (&themetext_colour);
    sprintf (cmdbuf, "sed -i s/'window.active.label.text.color: #......'/'window.active.label.text.color: #%c%c%c%c%c%c'/g %s",
        cstr[1], cstr[2], cstr[5], cstr[6], cstr[9], cstr[10], user_config_file);
    system (cmdbuf);

    g_free (cstr);
    g_free (user_config_file);
}

static void save_gtk3_settings (void)
{
    char *user_config_file, *cstr;
    char cmdbuf[256];

    // construct the file path
    user_config_file = g_build_filename (g_get_home_dir (), ".config/gtk-3.0/gtk.css", NULL);

    // convert colour to string and use sed to write
    cstr = gdk_color_to_string (&theme_colour);
    sprintf (cmdbuf, "sed -i s/'theme_selected_bg_color #......'/'theme_selected_bg_color #%c%c%c%c%c%c'/g %s",
        cstr[1], cstr[2], cstr[5], cstr[6], cstr[9], cstr[10], user_config_file);
    system (cmdbuf);

    cstr = gdk_color_to_string (&themetext_colour);
    sprintf (cmdbuf, "sed -i s/'theme_selected_fg_color #......'/'theme_selected_fg_color #%c%c%c%c%c%c'/g %s",
        cstr[1], cstr[2], cstr[5], cstr[6], cstr[9], cstr[10], user_config_file);
    system (cmdbuf);

    // write the current font to the file
    sprintf (cmdbuf, "sed -i s/'font:[^;]*'/'font:\t%s'/g %s", desktop_font, user_config_file);
    system (cmdbuf);
    
    // for stretch, this needs to be separate family and size entries
    char *c, *font;
    const gchar *size = NULL, *bold = NULL, *italic = NULL;

    // set the font description variables for XML from the font name
    font = g_strdup (desktop_font);
    while ((c = strrchr (font, ' ')))
    {
        if (!bold && !italic && !size && atoi (c + 1))
            size = c + 1;
        else if (!bold && !italic && !g_ascii_strcasecmp (c + 1, "italic"))
            italic = c + 1;
        else if (!bold && !g_ascii_strcasecmp (c + 1, "bold"))
            bold = c + 1;
        else break;
        *c = '\0';
    }
    if (!bold) bold = "Normal";
    if (!italic) italic = "Normal";
        
    sprintf (cmdbuf, "sed -i s/'font-family:[^;]*'/'font-family:\t%s'/g %s", font, user_config_file);
    system (cmdbuf);
    sprintf (cmdbuf, "sed -i s/'font-size:[^;]*'/'font-size:\t%spt'/g %s", size, user_config_file);
    system (cmdbuf);
    sprintf (cmdbuf, "sed -i s/'font-style:[^;]*'/'font-style:\t%s'/g %s", italic, user_config_file);
    system (cmdbuf);
    sprintf (cmdbuf, "sed -i s/'font-weight:[^;]*'/'font-weight:\t%s'/g %s", bold, user_config_file);
    system (cmdbuf);

    g_free (cstr);
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
        g_key_file_free (kf);
        g_free (user_config_file);
        return;
    }
    g_key_file_set_integer (kf, "ui", "big_icon_size", folder_size);
    g_key_file_set_integer (kf, "ui", "thumbnail_size", thumb_size);
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (str);
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_lxterm_settings (void)
{
    char *user_config_file, *str;
    char colbuf[32];
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
        write (handle, str, len);
        close (handle);

        g_free (str);
        g_key_file_free (kf);

        // copy the temp file to the correct place with sudo
        sprintf (buffer, "sudo -A cp %s %s", tfname, GREETER_CONFIG_FILE);
        system (buffer);
    }
}

static void save_obconf_settings (void)
{
    const char *session_name;
    char *user_config_file, *c, *font, *fname;
    int count;
    const gchar *size = NULL, *bold = NULL, *italic = NULL;

    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    fname = g_strconcat (g_ascii_strdown (session_name, -1), "-rc.xml", NULL);
    user_config_file = g_build_filename (g_get_user_config_dir (), "openbox/", fname, NULL);
    g_free (fname);

    // set the font description variables for XML from the font name
    font = g_strdup (desktop_font);
    while ((c = strrchr (font, ' ')))
    {
        if (!bold && !italic && !size && atoi (c + 1))
            size = c + 1;
        else if (!bold && !italic && !g_ascii_strcasecmp (c + 1, "italic"))
            italic = c + 1;
        else if (!bold && !g_ascii_strcasecmp (c + 1, "bold"))
            bold = c + 1;
        else break;
        *c = '\0';
    }
    if (!bold) bold = "Normal";
    if (!italic) italic = "Normal";

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    xmlDocPtr xDoc = xmlParseFile (user_config_file);
    if (xDoc == NULL)
    {
        g_free (font);
        g_free (user_config_file);
        return;
    }

    xmlXPathContextPtr xpathCtx = xmlXPathNewContext (xDoc);
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression ((xmlChar *) "/*[local-name()='openbox_config']/*[local-name()='theme']/*[local-name()='font']", xpathCtx);

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
                if (!strcmp (cur_node->name, "name")) xmlNodeSetContent (cur_node, font);
                if (!strcmp (cur_node->name, "size")) xmlNodeSetContent (cur_node, size);
                if (!strcmp (cur_node->name, "weight")) xmlNodeSetContent (cur_node, bold);
                if (!strcmp (cur_node->name, "slant"))  xmlNodeSetContent (cur_node, italic);
            }
        }
    }

    // cleanup XML
    xmlXPathFreeObject (xpathObj);
    xmlXPathFreeContext (xpathCtx);
    xmlSaveFile (user_config_file, xDoc);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

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

        g_free (user_config_file);
        user_config_file = g_build_filename (g_get_user_config_dir (), "libreoffice/", NULL);
        if (stat (user_config_file, &attr) == -1) mkdir (user_config_file, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        g_free (user_config_file);
        user_config_file = g_build_filename (g_get_user_config_dir (), "libreoffice/4/", NULL);
        if (stat (user_config_file, &attr) == -1) mkdir (user_config_file, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        g_free (user_config_file);
        user_config_file = g_build_filename (g_get_user_config_dir (), "libreoffice/4/user/", NULL);
        if (stat (user_config_file, &attr) == -1) mkdir (user_config_file, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        g_free (user_config_file);
        user_config_file = g_build_filename (g_get_user_config_dir (), "libreoffice/4/user/registrymodifications.xcu", NULL);

        // create XML doc
        FILE *fp = fopen (user_config_file, "wb");
        if (fp)
        {
            fprintf (fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
            fprintf (fp, "<oor:items xmlns:oor=\"http://openoffice.org/2001/registry\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n");
            fprintf (fp, "<item oor:path=\"/org.openoffice.Office.Common/Misc\"><prop oor:name=\"SymbolSet\" oor:op=\"fuse\"><value>%d</value></prop></item>\n", lo_icon_size);
            fprintf (fp, "</oor:items>\n");
            fclose (fp);
        }
        g_free (user_config_file);
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

static void on_menu_size_set (GtkRadioButton* btn, gpointer ptr)
{
    // only respond to the button which is now active
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) return;

    // find out which button in the group is active
    GSList *group = gtk_radio_button_get_group (btn);
    GtkRadioButton *tbtn;
    int nbtn = 0;
    while (group)
    {
        tbtn = group->data;
        group = group->next;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (tbtn))) break;
        nbtn++;
    }
    switch (nbtn)
    {
        case 0: icon_size = 20;
                break;
        case 1: icon_size = 28;
                break;
        case 2: icon_size = 36;
                break;
    }
    save_lxpanel_settings ();
    system (RELOAD_LXPANEL);
}

static void on_theme_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &theme_colour);
    save_lxsession_settings ();
    save_obpix_settings ();
    save_gtk3_settings ();
    if (needs_refresh) system (RELOAD_LXSESSION);
    system (RELOAD_OPENBOX);
    system (RELOAD_PCMANFM);
}

static void on_themetext_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &themetext_colour);
    save_lxsession_settings ();
    save_obpix_settings ();
    save_gtk3_settings ();
    if (needs_refresh) system (RELOAD_LXSESSION);
    system (RELOAD_OPENBOX);
    system (RELOAD_PCMANFM);
}

static void on_bar_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &bar_colour);
    save_lxsession_settings ();
    if (needs_refresh) system (RELOAD_LXSESSION);
    system (RELOAD_PCMANFM);
}

static void on_bartext_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &bartext_colour);
    save_lxsession_settings ();
    if (needs_refresh) system (RELOAD_LXSESSION);
    system (RELOAD_PCMANFM);
}

static void on_desktop_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &desktop_colour);
    save_pcman_settings ();
    system (RELOAD_PCMANFM);
}

static void on_desktoptext_colour_set (GtkColorButton* btn, gpointer ptr)
{
    gtk_color_button_get_color (btn, &desktoptext_colour);
    save_pcman_settings ();
    system (RELOAD_PCMANFM);
}

static void on_desktop_picture_set (GtkFileChooser* btn, gpointer ptr)
{
    char *picture = gtk_file_chooser_get_filename (btn);
    if (picture) desktop_picture = picture;
    save_pcman_settings ();
    system (RELOAD_PCMANFM);
}

static void on_desktop_font_set (GtkFontButton* btn, gpointer ptr)
{
    const char *font = gtk_font_button_get_font_name (btn);
    if (font) desktop_font = font;
    save_lxsession_settings ();
    save_pcman_settings ();
    save_obconf_settings ();
    save_gtk3_settings ();
    if (needs_refresh) system (RELOAD_LXSESSION);
    system (RELOAD_LXPANEL);
    system (RELOAD_OPENBOX);
    system (RELOAD_PCMANFM);
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
    system (RELOAD_PCMANFM);
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
    system (RELOAD_LXPANEL);
}

static void on_toggle_docs (GtkCheckButton* btn, gpointer ptr)
{
    show_docs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings ();
    system (RELOAD_PCMANFM);
}

static void on_toggle_trash (GtkCheckButton* btn, gpointer ptr)
{
    show_trash = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings ();
    system (RELOAD_PCMANFM);
}

static void on_toggle_mnts (GtkCheckButton* btn, gpointer ptr)
{
    show_mnts = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings ();
    system (RELOAD_PCMANFM);
}

static void on_set_defaults (GtkButton* btn, gpointer ptr)
{
    if (* (int *) ptr == 1)
    {
        desktop_font = "Piboto Light 12";
        terminal_font = "Monospace 10";
        icon_size = 36;
        folder_size = 48;
        thumb_size = 128;
        tb_icon_size = 24;
        lo_icon_size = 2;
    }
    else
    {
        desktop_font = "Piboto Light 8";
        terminal_font = "Monospace 8";
        icon_size = 20;
        folder_size = 32;
        thumb_size = 64;
        tb_icon_size = 16;
        lo_icon_size = 0;
    }
    gtk_font_button_set_font_name (GTK_FONT_BUTTON (font), desktop_font);
    desktop_picture = "/usr/share/rpd-wallpaper/road.jpg";
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dpic), desktop_picture);
    desktop_mode = "crop";
    gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 3);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb3), TRUE);
    gdk_color_parse ("#4D98F5", &theme_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (hcol), &theme_colour);
    gdk_color_parse ("#D6D3DE", &desktop_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (dcol), &desktop_colour);
    gdk_color_parse ("#E8E8E8", &desktoptext_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (dtcol), &desktoptext_colour);
    gdk_color_parse ("#000000", &bartext_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (btcol), &bartext_colour);
    gdk_color_parse ("#EDECEB", &bar_colour);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (bcol), &bar_colour);
    gdk_color_parse ("#FFFFFF", &themetext_colour);
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
    save_obpix_settings ();
    save_gtk3_settings ();
    save_lxpanel_settings ();
    save_lxterm_settings ();
    save_libreoffice_settings ();
    if (needs_refresh) system (RELOAD_LXSESSION);
    system (RELOAD_LXPANEL);
    system (RELOAD_OPENBOX);
    system (RELOAD_PCMANFM);
}


/* The dialog... */

int main (int argc, char *argv[])
{
    GtkBuilder *builder;
    GObject *item;
    GtkWidget *dlg;
    int maj, min, sub;
    int flag1 = 1, flag2 = 2;

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    // check to see if lxsession will auto-refresh - version 0.4.9 or later
    read_version ("lxsession", &maj, &min, &sub);
    if (min >= 5) needs_refresh = 0;
    else if (min == 4 && sub == 9) needs_refresh = 0;
    else needs_refresh = 1;

    // load data from config files
    check_themes ();
    load_lxsession_settings ();
    load_obpix_settings ();
    load_pcman_settings ();
    load_lxpanel_settings ();
    load_lxterm_settings ();
    load_libreoffice_settings ();
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

    item = gtk_builder_get_object (builder, "button3");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), &flag1);
    item = gtk_builder_get_object (builder, "button4");
    g_signal_connect (item, "clicked", G_CALLBACK (on_set_defaults), &flag2);

    rb1 = gtk_builder_get_object (builder, "radiobutton1");
    g_signal_connect (rb1, "toggled", G_CALLBACK (on_bar_pos_set), NULL);
    rb2 = gtk_builder_get_object (builder, "radiobutton2");
    g_signal_connect (rb2, "toggled", G_CALLBACK (on_bar_pos_set), NULL);
    if (barpos) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb2), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb1), TRUE);

    rb3 = gtk_builder_get_object (builder, "radiobutton3");
    g_signal_connect (rb3, "toggled", G_CALLBACK (on_menu_size_set), NULL);
    rb4 = gtk_builder_get_object (builder, "radiobutton4");
    g_signal_connect (rb4, "toggled", G_CALLBACK (on_menu_size_set), NULL);
    rb5 = gtk_builder_get_object (builder, "radiobutton5");
    g_signal_connect (rb5, "toggled", G_CALLBACK (on_menu_size_set), NULL);
    if (icon_size <= 20) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb5), TRUE);
    else if (icon_size <= 28) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb4), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb3), TRUE);

    cb1 = gtk_builder_get_object (builder, "checkbutton1");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb1), show_docs);
    g_signal_connect (cb1, "toggled", G_CALLBACK (on_toggle_docs), NULL);
    cb2 = gtk_builder_get_object (builder, "checkbutton2");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb2), show_trash);
    g_signal_connect (cb2, "toggled", G_CALLBACK (on_toggle_trash), NULL);
    cb3 = gtk_builder_get_object (builder, "checkbutton3");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb3), show_mnts);
    g_signal_connect (cb3, "toggled", G_CALLBACK (on_toggle_mnts), NULL);

    g_object_unref (builder);

    if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_CANCEL)
    {
        if (restore_values ())
        {
            save_lxsession_settings ();
            save_pcman_settings ();
            save_obconf_settings ();
            save_obpix_settings ();
            save_gtk3_settings ();
            save_lxpanel_settings ();
            save_lxterm_settings ();
            save_libreoffice_settings ();
            if (needs_refresh) system (RELOAD_LXSESSION);
            system (RELOAD_LXPANEL);
            system (RELOAD_OPENBOX);
            system (RELOAD_PCMANFM);
        }
    }
    else save_greeter_settings ();
    gtk_widget_destroy (dlg);

    return 0;
}
