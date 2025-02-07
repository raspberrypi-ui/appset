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
/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

#define MAX_DESKTOPS 9

#define DEFAULT_THEME "PiXflat"
#define DEFAULT_THEME_DARK "PiXnoir"
#define DEFAULT_THEME_L "PiXflat_l"
#define DEFAULT_THEME_DARK_L "PiXnoir_l"
#define TEMP_THEME    "tPiXflat"

#define XC(str) ((xmlChar *) str)

typedef struct {
    const char *desktop_folder;
    const char *desktop_picture;
    const char *desktop_mode;
    GdkRGBA desktop_colour;
    GdkRGBA desktoptext_colour;
    int show_docs;
    int show_trash;
    int show_mnts;
} DesktopConfig;

typedef struct {
    DesktopConfig desktops[MAX_DESKTOPS];
    const char *desktop_font;
    const char *terminal_font;
    GdkRGBA theme_colour[2];
    GdkRGBA themetext_colour[2];
    GdkRGBA bar_colour[2];
    GdkRGBA bartext_colour[2];
    int icon_size;
    int barpos;
    int folder_size;
    int thumb_size;
    int pane_size;
    int sicon_size;
    int tb_icon_size;
    int lo_icon_size;
    int cursor_size;
    int task_width;
    int handle_width;
    int scrollbar_width;
    int monitor;
    int common_bg;
    int darkmode;
} Config;

typedef enum {
    WM_OPENBOX,
    WM_WAYFIRE,
    WM_LABWC } 
wm_type;

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

extern Config cur_conf;
extern wm_type wm;
extern int ndesks;
extern GtkTreeModel *sortmons;

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

extern int vsystem (const char *fmt, ...);
extern char *get_string (char *cmd);
extern char *get_quoted_string (char *cmd);
extern char *rgba_to_gdk_color_string (GdkRGBA *col);
extern const char *session (void);
extern void check_directory (const char *path);
extern void message_ok (char *msg);

/* End of file */
/*----------------------------------------------------------------------------*/
