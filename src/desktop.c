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

#include "pipanel.h"

#include "desktop.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

/* Controls */
static GtkWidget *dcol, *dtcol, *dmod, *dpic, *dfold, *cbdesk, *cb1, *cb2, *cb3, *cb4;

/* Handler IDs */
static gulong dmid, tdid, ttid, tmid, dfid, cbid, cdid;

/* Currently-selected desktop */
static int desktop_n;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void atk_label (GtkWidget *widget, GtkLabel *label);
static void load_pcman_settings (int desktop);
static void load_pcman_g_settings (void);
static void on_desktop_changed (GtkComboBox* cb, gpointer ptr);
static void on_toggle_desktop (GtkCheckButton* btn, gpointer ptr);
static void on_desktop_mode_set (GtkComboBox* btn, gpointer ptr);
static void on_desktop_picture_set (GtkFileChooser* btn, gpointer ptr);
static void on_desktop_colour_set (GtkColorChooser* btn, gpointer ptr);
static void on_desktoptext_colour_set (GtkColorChooser* btn, gpointer ptr);
static void on_desktop_folder_set (GtkFileChooser* btn, gpointer ptr);
static void on_toggle_docs (GtkCheckButton* btn, gpointer ptr);
static void on_toggle_trash (GtkCheckButton* btn, gpointer ptr);
static void on_toggle_mnts (GtkCheckButton* btn, gpointer ptr);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

void reload_pcmanfm (void)
{
    vsystem ("pcmanfm --reconfigure");
}

/* Create a labelled-by relationship between a widget and a label */

static void atk_label (GtkWidget *widget, GtkLabel *label)
{
    AtkObject *atk_widget, *atk_label;
    AtkRelationSet *relation_set;
    AtkRelation *relation;
    AtkObject *targets[1];

    atk_widget = gtk_widget_get_accessible (widget);
    atk_label = gtk_widget_get_accessible (GTK_WIDGET (label));
    relation_set = atk_object_ref_relation_set (atk_widget);
    targets[0] = atk_label;
    relation = atk_relation_new (targets, 1, ATK_RELATION_LABELLED_BY);
    atk_relation_set_add (relation_set, relation);
    g_object_unref (G_OBJECT (relation));
}

/*----------------------------------------------------------------------------*/
/* Load / save data                                                           */
/*----------------------------------------------------------------------------*/

char *pcmanfm_file (gboolean global, int desktop, gboolean write)
{
    char *fname, *buf;
    if (desktop < 0 || desktop > MAX_DESKTOPS) return NULL;
    if (cur_conf.common_bg)
    {
        fname = g_strdup_printf ("desktop-items-0.conf");
        buf = g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), fname, NULL);
        g_free (fname);
        return buf;
    }

    if (wm != WM_OPENBOX)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), desktop);
#pragma GCC diagnostic pop
        fname = g_strdup_printf ("desktop-items-%s.conf", buf);
        g_free (buf);
        buf = g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), fname, NULL);
        g_free (fname);
        if (write || access (buf, F_OK) == 0) return buf;
        else g_free (buf);
        if (global && desktop > 1) desktop = 1; // only 2 numbered global desktop files
    }

    fname = g_strdup_printf ("desktop-items-%u.conf", desktop);
    buf = g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), fname, NULL);
    g_free (fname);
    return buf;
}

char *pcmanfm_g_file (gboolean global)
{
    return g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "pcmanfm", session (), "pcmanfm.conf", NULL);
}

static void load_pcman_settings (int desktop)
{
    char *user_config_file, *ret;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from file to a key file
    user_config_file = pcmanfm_file (FALSE, desktop, FALSE);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_bg", &err);
        if (err == NULL)
        {
            if (!gdk_rgba_parse (&cur_conf.desktops[desktop].desktop_colour, ret))
                DEFAULT (desktops[desktop].desktop_colour);
        }
        else DEFAULT (desktops[desktop].desktop_colour);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "desktop_fg", &err);
        if (err == NULL)
        {
            if (!gdk_rgba_parse (&cur_conf.desktops[desktop].desktoptext_colour, ret))
                DEFAULT (desktops[desktop].desktoptext_colour);
        }
        else DEFAULT (desktops[desktop].desktoptext_colour);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper", &err);
        if (err == NULL && ret) cur_conf.desktops[desktop].desktop_picture = g_strdup (ret);
        else DEFAULT (desktops[desktop].desktop_picture);
        g_free (ret);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "wallpaper_mode", &err);
        if (err == NULL && ret) cur_conf.desktops[desktop].desktop_mode = g_strdup (ret);
        else DEFAULT (desktops[desktop].desktop_mode);
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_documents", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.desktops[desktop].show_docs = val;
        else DEFAULT (desktops[desktop].show_docs);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_trash", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.desktops[desktop].show_trash = val;
        else DEFAULT (desktops[desktop].show_trash);

        err = NULL;
        val = g_key_file_get_integer (kf, "*", "show_mounts", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.desktops[desktop].show_mnts = val;
        else DEFAULT (desktops[desktop].show_mnts);

        err = NULL;
        ret = g_key_file_get_string (kf, "*", "folder", &err);
        if (err == NULL && ret) cur_conf.desktops[desktop].desktop_folder = g_strdup (ret);
        else DEFAULT (desktops[desktop].desktop_folder);
        g_free (ret);
    }
    else
    {
        DEFAULT (desktops[desktop].desktop_colour);
        DEFAULT (desktops[desktop].desktoptext_colour);
        DEFAULT (desktops[desktop].desktop_picture);
        DEFAULT (desktops[desktop].desktop_mode);
        DEFAULT (desktops[desktop].show_docs);
        DEFAULT (desktops[desktop].show_trash);
        DEFAULT (desktops[desktop].show_mnts);
        DEFAULT (desktops[desktop].desktop_folder);
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void load_pcman_g_settings (void)
{
    char *user_config_file;
    GKeyFile *kf;
    GError *err;
    gint val;

    // read in data from file to a key file
    user_config_file = pcmanfm_g_file (FALSE);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        val = g_key_file_get_integer (kf, "ui", "common_bg", &err);
        if (err == NULL && val >= 0 && val <= 1) cur_conf.common_bg = val;
        else DEFAULT (common_bg);
    }
    else
    {
        DEFAULT (common_bg);
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

void save_pcman_settings (int desktop)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = pcmanfm_file (FALSE, desktop, TRUE);
    check_directory (user_config_file);

    // process pcmanfm config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    str = rgba_to_gdk_color_string (&cur_conf.desktops[desktop].desktop_colour);
    g_key_file_set_string (kf, "*", "desktop_bg", str);
    g_key_file_set_string (kf, "*", "desktop_shadow", str);
    g_free (str);

    str = rgba_to_gdk_color_string (&cur_conf.desktops[desktop].desktoptext_colour);
    g_key_file_set_string (kf, "*", "desktop_fg", str);
    g_free (str);

    g_key_file_set_string (kf, "*", "desktop_font", cur_conf.desktop_font);
    g_key_file_set_string (kf, "*", "wallpaper", cur_conf.desktops[desktop].desktop_picture);
    g_key_file_set_string (kf, "*", "wallpaper_mode", cur_conf.desktops[desktop].desktop_mode);
    g_key_file_set_integer (kf, "*", "show_documents", cur_conf.desktops[desktop].show_docs);
    g_key_file_set_integer (kf, "*", "show_trash", cur_conf.desktops[desktop].show_trash);
    g_key_file_set_integer (kf, "*", "show_mounts", cur_conf.desktops[desktop].show_mnts);
    g_key_file_set_string (kf, "*", "folder", cur_conf.desktops[desktop].desktop_folder);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

void save_pcman_g_settings (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = pcmanfm_g_file (FALSE);
    check_directory (user_config_file);

    // process pcmanfm config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_integer (kf, "ui", "common_bg", cur_conf.common_bg);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

/*----------------------------------------------------------------------------*/
/* Set controls to match data                                                 */
/*----------------------------------------------------------------------------*/

void set_desktop_controls (void)
{
    GtkTreeIter iter;
    int val;

    g_signal_handler_block (dmod, dmid);
    g_signal_handler_block (cb1, tdid);
    g_signal_handler_block (cb2, ttid);
    g_signal_handler_block (cb3, tmid);
    g_signal_handler_block (dfold, dfid);
    g_signal_handler_block (cb4, cbid);
    g_signal_handler_block (cbdesk, cdid);
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb4), cur_conf.common_bg);
    if (ndesks > 1)
    {
        if (cur_conf.common_bg)
        {
            desktop_n = 0;
            gtk_combo_box_set_active (GTK_COMBO_BOX (cbdesk), -1);
            gtk_widget_set_sensitive (GTK_WIDGET (cbdesk), FALSE);
        }
        else
        {
            gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sortmons), &iter);
            gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &val, -1);
            while (val != desktop_n)
            {
                gtk_tree_model_iter_next (GTK_TREE_MODEL (sortmons), &iter);
                gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &val, -1);
            }
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cbdesk), &iter);
            gtk_widget_set_sensitive (GTK_WIDGET (cbdesk), TRUE);
        }
    }

    gtk_widget_set_sensitive (GTK_WIDGET (dpic), TRUE);
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dpic), cur_conf.desktops[desktop_n].desktop_picture);
    if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "center")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 1);
    else if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "fit")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 2);
    else if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "crop")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 3);
    else if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "stretch")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 4);
    else if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "tile")) gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 5);
    else
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (dmod), 0);
        gtk_widget_set_sensitive (GTK_WIDGET (dpic), FALSE);
    }
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dcol), &cur_conf.desktops[desktop_n].desktop_colour);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dtcol), &cur_conf.desktops[desktop_n].desktoptext_colour);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb1), cur_conf.desktops[desktop_n].show_docs);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb2), cur_conf.desktops[desktop_n].show_trash);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb3), cur_conf.desktops[desktop_n].show_mnts);
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dfold), cur_conf.desktops[desktop_n].desktop_folder);

    g_signal_handler_unblock (cb4, cbid);
    g_signal_handler_unblock (cbdesk, cdid);
    g_signal_handler_unblock (dmod, dmid);
    g_signal_handler_unblock (cb1, tdid);
    g_signal_handler_unblock (cb2, ttid);
    g_signal_handler_unblock (cb3, tmid);
    g_signal_handler_unblock (dfold, dfid);
}

/*----------------------------------------------------------------------------*/
/* Control handlers                                                           */
/*----------------------------------------------------------------------------*/

static void on_desktop_changed (GtkComboBox* cb, gpointer ptr)
{
    GtkTreeIter iter;

    gtk_combo_box_get_active_iter (cb, &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &desktop_n, -1);

    set_desktop_controls ();
}

static void on_toggle_desktop (GtkCheckButton* btn, gpointer ptr)
{
    int i;

    desktop_n = 0;
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn)))
    {
        cur_conf.common_bg = 1;
        g_signal_handler_block (cbdesk, cdid);
        gtk_combo_box_set_active (GTK_COMBO_BOX (cbdesk), -1);
        g_signal_handler_unblock (cbdesk, cdid);
        gtk_widget_set_sensitive (GTK_WIDGET (cbdesk), FALSE);
    }
    else
    {
        cur_conf.common_bg = 0;
        gtk_combo_box_set_active (GTK_COMBO_BOX (cbdesk), 0);
        gtk_widget_set_sensitive (GTK_WIDGET (cbdesk), TRUE);
        for (i = 0; i < ndesks; i++) load_pcman_settings (i);
    }
    set_desktop_controls ();
    save_pcman_g_settings ();
    save_pcman_settings (0);
    reload_pcmanfm ();
}

static void on_desktop_mode_set (GtkComboBox* btn, gpointer ptr)
{
    gint val = gtk_combo_box_get_active (btn);
    switch (val)
    {
        case 0 :    cur_conf.desktops[desktop_n].desktop_mode = "color";
                    break;
        case 1 :    cur_conf.desktops[desktop_n].desktop_mode = "center";
                    break;
        case 2 :    cur_conf.desktops[desktop_n].desktop_mode = "fit";
                    break;
        case 3 :    cur_conf.desktops[desktop_n].desktop_mode = "crop";
                    break;
        case 4 :    cur_conf.desktops[desktop_n].desktop_mode = "stretch";
                    break;
        case 5 :    cur_conf.desktops[desktop_n].desktop_mode = "tile";
                    break;
    }

    if (!strcmp (cur_conf.desktops[desktop_n].desktop_mode, "color")) gtk_widget_set_sensitive (GTK_WIDGET (dpic), FALSE);
    else gtk_widget_set_sensitive (GTK_WIDGET (dpic), TRUE);
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_desktop_picture_set (GtkFileChooser* btn, gpointer ptr)
{
    char *picture = gtk_file_chooser_get_filename (btn);
    if (picture) cur_conf.desktops[desktop_n].desktop_picture = picture;
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_desktop_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.desktops[desktop_n].desktop_colour);
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_desktoptext_colour_set (GtkColorChooser* btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.desktops[desktop_n].desktoptext_colour);
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_desktop_folder_set (GtkFileChooser* btn, gpointer ptr)
{
    char *folder = gtk_file_chooser_get_filename (btn);
    if (folder)
    {
        if (g_strcmp0 (cur_conf.desktops[desktop_n].desktop_folder, folder))
        {
            cur_conf.desktops[desktop_n].desktop_folder = folder;
            save_pcman_settings (desktop_n);
            reload_pcmanfm ();
        }
    }
}

static void on_toggle_docs (GtkCheckButton* btn, gpointer ptr)
{
    cur_conf.desktops[desktop_n].show_docs = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_toggle_trash (GtkCheckButton* btn, gpointer ptr)
{
    cur_conf.desktops[desktop_n].show_trash = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

static void on_toggle_mnts (GtkCheckButton* btn, gpointer ptr)
{
    cur_conf.desktops[desktop_n].show_mnts = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn));
    save_pcman_settings (desktop_n);
    reload_pcmanfm ();
}

/*----------------------------------------------------------------------------*/
/* Initialisation                                                             */
/*----------------------------------------------------------------------------*/

void load_desktop_tab (GtkBuilder *builder)
{
    GtkWidget *wid;
    GtkLabel *lbl;
    GList *children, *child;
    int i;

    load_pcman_g_settings ();
    for (i = 0; i < ndesks; i++)
        load_pcman_settings (i);
    
    desktop_n = 0;

    dcol = (GtkWidget *) gtk_builder_get_object (builder, "colorbutton2");
    g_signal_connect (dcol, "color-set", G_CALLBACK (on_desktop_colour_set), NULL);

    dtcol = (GtkWidget *) gtk_builder_get_object (builder, "colorbutton6");
    g_signal_connect (dtcol, "color-set", G_CALLBACK (on_desktoptext_colour_set), NULL);

    dpic = (GtkWidget *) gtk_builder_get_object (builder, "filechooserbutton1");
    g_signal_connect (dpic, "file-set", G_CALLBACK (on_desktop_picture_set), NULL);

    // add accessibility label to button child of file chooser
    lbl = GTK_LABEL (gtk_builder_get_object (builder, "label12"));
    children = gtk_container_get_children (GTK_CONTAINER (dpic));
    child = children;
    do
    {
        wid = GTK_WIDGET (child->data);
        if (GTK_IS_BUTTON (wid))
        {
            atk_label (wid, lbl);
            gtk_widget_set_tooltip_text (wid, gtk_widget_get_tooltip_text (GTK_WIDGET (dpic)));
        }
    } while ((child = g_list_next (child)) != NULL);
    g_list_free (children);

    dmod = (GtkWidget *) gtk_builder_get_object (builder, "comboboxtext1");
    dmid = g_signal_connect (dmod, "changed", G_CALLBACK (on_desktop_mode_set), NULL);

    cb1 = (GtkWidget *) gtk_builder_get_object (builder, "checkbutton1");
    tdid = g_signal_connect (cb1, "toggled", G_CALLBACK (on_toggle_docs), NULL);

    cb2 = (GtkWidget *) gtk_builder_get_object (builder, "checkbutton2");
    ttid = g_signal_connect (cb2, "toggled", G_CALLBACK (on_toggle_trash), NULL);

    cb3 = (GtkWidget *) gtk_builder_get_object (builder, "checkbutton3");
    tmid = g_signal_connect (cb3, "toggled", G_CALLBACK (on_toggle_mnts), NULL);

    dfold = (GtkWidget *) gtk_builder_get_object (builder, "filechooserbutton4");
    dfid = g_signal_connect (dfold, "selection-changed", G_CALLBACK (on_desktop_folder_set), NULL);

    cb4 = (GtkWidget *) gtk_builder_get_object (builder, "checkbutton4");
    cbid = g_signal_connect (cb4, "toggled", G_CALLBACK (on_toggle_desktop), NULL);

    cbdesk = (GtkWidget *) gtk_builder_get_object (builder, "cb_desktop");
    cdid = g_signal_connect (cbdesk, "changed", G_CALLBACK (on_desktop_changed), NULL);

    // add accessibility label to combo box child of file chooser (yes, I know the previous one attached to a button...)
    lbl = GTK_LABEL (gtk_builder_get_object (builder, "label16"));
    children = gtk_container_get_children (GTK_CONTAINER (dfold));
    child = children;
    do
    {
        wid = GTK_WIDGET (child->data);
        if (GTK_IS_COMBO_BOX (wid))
        {
            atk_label (wid, lbl);
            gtk_widget_set_tooltip_text (wid, gtk_widget_get_tooltip_text (GTK_WIDGET (dfold)));
        }
    } while ((child = g_list_next (child)) != NULL);
    g_list_free (children);

    if (ndesks > 1)
    {
        GtkCellRenderer *rend = gtk_cell_renderer_text_new ();

        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbdesk), rend, FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (cbdesk), rend, "text", 1);
        gtk_combo_box_set_model (GTK_COMBO_BOX (cbdesk), GTK_TREE_MODEL (sortmons));

        gtk_widget_show (GTK_WIDGET (cb4));
        if (cur_conf.common_bg == 0 && st_tab == 1) desktop_n = 1;
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox10")));
    }

}

/* End of file */
/*----------------------------------------------------------------------------*/
