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

#include "pipanel.h"
#include "desktop.h"
#include "system.h"
#include "defaults.h"

#include "taskbar.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#define MIN_ICON 16
#define MAX_ICON 68

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

/* Controls */
static GtkWidget *colour_bar, *colour_bartext, *rb_top, *rb_bottom, *combo_size;
static GtkWidget *combo_monitor;

/* Handler IDs */
static gulong id_size, id_pos, id_monitor;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static char *wfpanel_file (gboolean global);
static void load_lxpanel_settings (void);
static void load_wfpanel_settings (void);
static void save_lxpanel_settings (void);
static void save_wfpanel_settings (void);
static void on_bar_size_set (GtkComboBox *btn, gpointer ptr);
static void on_bar_pos_set (GtkRadioButton *btn, gpointer ptr);
static void on_bar_loc_set (GtkComboBox *cb, gpointer ptr);
static void on_bar_colour_set (GtkColorChooser *btn, gpointer ptr);
static void on_bar_textcolour_set (GtkColorChooser *btn, gpointer ptr);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Helpers                                                                    */
/*----------------------------------------------------------------------------*/

void reload_panel (void)
{
    if (wm == WM_OPENBOX) vsystem ("lxpanelctl-pi refresh");
}

/*----------------------------------------------------------------------------*/
/* Load / save data                                                           */
/*----------------------------------------------------------------------------*/

char *lxpanel_file (gboolean global)
{
    return g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "lxpanel-pi", "panels/panel", NULL);
}

static char *wfpanel_file (gboolean global)
{
    return g_build_filename (global ? "/etc/xdg" : g_get_user_config_dir (), "wf-panel-pi", "wf-panel-pi.ini", NULL);
}

static void load_lxpanel_settings (void)
{
    char *user_config_file, *cmdbuf, *res;
    int val;

    user_config_file = lxpanel_file (FALSE);
    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        DEFAULT (barpos);
        DEFAULT (icon_size);
        DEFAULT (task_width);
        DEFAULT (monitor);
        g_free (user_config_file);
        return;
    }

    if (!vsystem ("grep -q edge=bottom %s", user_config_file)) cur_conf.barpos = 1;
    else DEFAULT (barpos);

    if (!vsystem ("grep -q monitor=1 %s", user_config_file)) cur_conf.monitor = 1;
    else DEFAULT (monitor);

    cmdbuf = g_strdup_printf ("grep -Po '(?<=iconsize=)[0-9]+' %s", user_config_file);
    res = get_string (cmdbuf);
    if (res[0] && sscanf (res, "%d", &val) == 1) cur_conf.icon_size = val;
    else DEFAULT (icon_size);
    g_free (res);
    g_free (cmdbuf);

    cmdbuf = g_strdup_printf ("grep -Po '(?<=MaxTaskWidth=)[0-9]+' %s", user_config_file);
    res = get_string (cmdbuf);
    if (res[0] && sscanf (res, "%d", &val) == 1) cur_conf.task_width = val;
    else DEFAULT (task_width);
    g_free (res);
    g_free (cmdbuf);

    g_free (user_config_file);
}

static void load_wfpanel_settings (void)
{
    char *user_config_file, *ret;
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
        if (err == NULL && ret && !strcmp (ret, "bottom")) cur_conf.barpos = 1;
        else DEFAULT (barpos);
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "panel", "icon_size", &err);
        if (err == NULL && val >= 16 && val <= 48) cur_conf.icon_size = val + 4;
        else DEFAULT (icon_size);

        err = NULL;
        val = g_key_file_get_integer (kf, "panel", "window-list_max_width", &err);
        if (err == NULL) cur_conf.task_width = val;
        else DEFAULT (task_width);

        err = NULL;
        ret = g_key_file_get_string (kf, "panel", "monitor", &err);
        DEFAULT (monitor);
        if (err == NULL && ret)
        {
            for (val = 0; val < ndesks; val++)
            {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                char *buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), val);
#pragma GCC diagnostic pop
                if (!g_strcmp0 (buf, ret)) cur_conf.monitor = val;
                g_free (buf);
            }
        }
    }
    else
    {
        DEFAULT (barpos);
        DEFAULT (icon_size);
        DEFAULT (task_width);
        DEFAULT (monitor);
    }
    g_key_file_free (kf);
    g_free (user_config_file);

    user_config_file = wfpanel_file (FALSE);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // get data from the key file
        err = NULL;
        ret = g_key_file_get_string (kf, "panel", "position", &err);
        if (err == NULL && ret)
        {
            if (!strcmp (ret, "bottom")) cur_conf.barpos = 1;
            else cur_conf.barpos = 0;
        }
        g_free (ret);

        err = NULL;
        val = g_key_file_get_integer (kf, "panel", "icon_size", &err);
        if (err == NULL && val >= 16 && val <= 48) cur_conf.icon_size = val + 4;

        err = NULL;
        val = g_key_file_get_integer (kf, "panel", "window-list_max_width", &err);
        if (err == NULL) cur_conf.task_width = val;

        err = NULL;
        ret = g_key_file_get_string (kf, "panel", "monitor", &err);
        if (err == NULL && ret)
        {
            for (val = 0; val < ndesks; val++)
            {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                char *buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), val);
#pragma GCC diagnostic pop
                if (!g_strcmp0 (buf, ret)) cur_conf.monitor = val;
                g_free (buf);
            }
        }
    }
    g_key_file_free (kf);
    g_free (user_config_file);
}

static void save_lxpanel_settings (void)
{
    char *user_config_file;

    user_config_file = lxpanel_file (FALSE);

    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        // need a local copy to take the changes
        check_directory (user_config_file);
        vsystem ("cp /etc/xdg/lxpanel-pi/panels/panel %s", user_config_file);
    }

    // use sed to write
    if (cur_conf.icon_size <= MAX_ICON && cur_conf.icon_size >= MIN_ICON)
    {
        vsystem ("sed -i s/iconsize=.*/iconsize=%d/g %s", cur_conf.icon_size, user_config_file);
        vsystem ("sed -i s/height=.*/height=%d/g %s", cur_conf.icon_size, user_config_file);
    }
    vsystem ("sed -i s/edge=.*/edge=%s/g %s", cur_conf.barpos ? "bottom" : "top", user_config_file);
    vsystem ("sed -i s/MaxTaskWidth=.*/MaxTaskWidth=%d/g %s", cur_conf.task_width, user_config_file);
    vsystem ("sed -i s/monitor=.*/monitor=%d/g %s", cur_conf.monitor, user_config_file);

    g_free (user_config_file);
}

static void save_wfpanel_settings (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = wfpanel_file (FALSE);
    check_directory (user_config_file);

    // process wfpanel config data
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_string (kf, "panel", "position", cur_conf.barpos ? "bottom" : "top");
    g_key_file_set_integer (kf, "panel", "icon_size", cur_conf.icon_size - 4);
    g_key_file_set_integer (kf, "panel", "window-list_max_width", cur_conf.task_width);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    char *buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (gdk_display_get_default ()), cur_conf.monitor);
#pragma GCC diagnostic pop
    g_key_file_set_string (kf, "panel", "monitor", buf);
    g_free (buf);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

void save_panel_settings (void)
{
    if (wm == WM_OPENBOX) save_lxpanel_settings ();
    else save_wfpanel_settings ();
}

/*----------------------------------------------------------------------------*/
/* Set controls to match data                                                 */
/*----------------------------------------------------------------------------*/

void set_taskbar_controls (void)
{
    GtkTreeIter iter;
    int val;

    g_signal_handler_block (combo_size, id_size);
    g_signal_handler_block (rb_top, id_pos);
    g_signal_handler_block (combo_monitor, id_monitor);

    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_bar), &cur_conf.bar_colour[cur_conf.darkmode]);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colour_bartext), &cur_conf.bartext_colour[cur_conf.darkmode]);

    if (cur_conf.icon_size <= 20) gtk_combo_box_set_active (GTK_COMBO_BOX (combo_size), 3);
    else if (cur_conf.icon_size <= 28) gtk_combo_box_set_active (GTK_COMBO_BOX (combo_size), 2);
    else if (cur_conf.icon_size <= 36) gtk_combo_box_set_active (GTK_COMBO_BOX (combo_size), 1);
    else gtk_combo_box_set_active (GTK_COMBO_BOX (combo_size), 0);

    if (cur_conf.barpos) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_bottom), TRUE);
    else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb_top), TRUE);

    if (ndesks > 1)
    {
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sortmons), &iter);
        gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &val, -1);
        while (val != cur_conf.monitor)
        {
            gtk_tree_model_iter_next (GTK_TREE_MODEL (sortmons), &iter);
            gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &val, -1);
        }
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_monitor), &iter);
    }

    g_signal_handler_unblock (rb_top, id_pos);
    g_signal_handler_unblock (combo_monitor, id_monitor);
    g_signal_handler_unblock (combo_size, id_size);
}

/*----------------------------------------------------------------------------*/
/* Control handlers                                                           */
/*----------------------------------------------------------------------------*/

static void on_bar_size_set (GtkComboBox *btn, gpointer ptr)
{
    gint val = gtk_combo_box_get_active (btn);
    switch (val)
    {
        case 0 :    cur_conf.icon_size = 52;
                    cur_conf.task_width = 300;
                    break;
        case 1 :    cur_conf.icon_size = 36;
                    cur_conf.task_width = 200;
                    break;
        case 2 :    cur_conf.icon_size = 28;
                    cur_conf.task_width = 200;
                    break;
        case 3 :    cur_conf.icon_size = 20;
                    cur_conf.task_width = 150;
                    break;
    }

    save_panel_settings ();
    if (wm != WM_OPENBOX) reload_desktop ();
    reload_panel ();
}

static void on_bar_pos_set (GtkRadioButton *btn, gpointer ptr)
{
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btn))) cur_conf.barpos = 0;
    else cur_conf.barpos = 1;

    save_panel_settings ();
    if (wm != WM_OPENBOX) reload_desktop ();
    reload_panel ();
}

static void on_bar_loc_set (GtkComboBox *cb, gpointer ptr)
{
    GtkTreeIter iter;

    gtk_combo_box_get_active_iter (cb, &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (sortmons), &iter, 0, &cur_conf.monitor, -1);

    save_panel_settings ();
    if (wm != WM_OPENBOX) reload_desktop ();
    reload_panel ();
}

static void on_bar_colour_set (GtkColorChooser *btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.bar_colour[cur_conf.darkmode]);
    set_theme (TEMP_THEME);
    save_gtk3_settings ();
    reload_theme (FALSE);
}

static void on_bar_textcolour_set (GtkColorChooser *btn, gpointer ptr)
{
    gtk_color_chooser_get_rgba (btn, &cur_conf.bartext_colour[cur_conf.darkmode]);
    set_theme (TEMP_THEME);
    save_gtk3_settings ();
    reload_theme (FALSE);
}

/*----------------------------------------------------------------------------*/
/* Initialisation                                                             */
/*----------------------------------------------------------------------------*/

void load_taskbar_tab (GtkBuilder *builder)
{
    if (wm != WM_OPENBOX) load_wfpanel_settings ();
    else load_lxpanel_settings ();

    colour_bar = (GtkWidget *) gtk_builder_get_object (builder, "colorbutton3");
    g_signal_connect (colour_bar, "color-set", G_CALLBACK (on_bar_colour_set), NULL);

    colour_bartext = (GtkWidget *) gtk_builder_get_object (builder, "colorbutton4");
    g_signal_connect (colour_bartext, "color-set", G_CALLBACK (on_bar_textcolour_set), NULL);

    rb_top = (GtkWidget *) gtk_builder_get_object (builder, "radiobutton1");
    rb_bottom = (GtkWidget *) gtk_builder_get_object (builder, "radiobutton2");
    id_pos = g_signal_connect (rb_top, "toggled", G_CALLBACK (on_bar_pos_set), NULL);

    combo_monitor = (GtkWidget *) gtk_builder_get_object (builder, "cb_barmon");
    id_monitor = g_signal_connect (combo_monitor, "changed", G_CALLBACK (on_bar_loc_set), NULL);

    combo_size = (GtkWidget *) gtk_builder_get_object (builder, "comboboxtext2");
    id_size = g_signal_connect (combo_size, "changed", G_CALLBACK (on_bar_size_set), NULL);

    if (ndesks > 1)
    {
        GtkCellRenderer *rend = gtk_cell_renderer_text_new ();

        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_monitor), rend, FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo_monitor), rend, "text", 1);
        gtk_combo_box_set_model (GTK_COMBO_BOX (combo_monitor), GTK_TREE_MODEL (sortmons));

        gtk_widget_show_all (GTK_WIDGET (gtk_builder_get_object (builder, "hbox25")));
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "hbox25")));
    }
}

/* End of file */
/*----------------------------------------------------------------------------*/
