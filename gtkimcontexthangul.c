/* ImHangul - Gtk+ 2.0 Input Method Module for Hangul
 * Copyright (C) 2002-2008 Choe Hwanjin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <hangul.h>

#include "gettext.h"
#include "gtkimcontexthangul.h"

enum {
  INPUT_MODE_DIRECT,
  INPUT_MODE_HANGUL,
  INPUT_MODE_HANJA
} IMHangulInputMode;

enum {
  INPUT_MODE_INFO_NONE,
  INPUT_MODE_INFO_ENGLISH,
  INPUT_MODE_INFO_HANGUL
} IMHangulInputModeInfo;

typedef struct _CandidateItem CandidateItem;
typedef struct _StatusWindow  StatusWindow;

struct _Toplevel
{
  int input_mode;
  GtkWidget *widget;
  GtkWidget *status;
  GSList *contexts;
  guint destroy_handler_id;
  guint configure_handler_id;
};

/* Candidate window */
struct _Candidate {
  gchar *key;
  GtkIMContextHangul *hangul_context;
  GtkWidget *window;
  GdkWindow *parent;
  GdkRectangle cursor;
  GtkListStore *store;
  GtkWidget *treeview;
  HanjaList *list;
  int first;
  int n;
  int n_per_page;
  int current;
};

struct _CandidateItem {
    gunichar ch;
    gchar *comment;
};

static size_t ucschar_strlen(const ucschar* s);

static Candidate*  candidate_new             (char *key,
					      int n_per_page,
					      HanjaList *list,
					      GdkWindow *parent,
					      GdkRectangle *area,
					      GtkIMContextHangul *hcontext);
static void        candidate_prev            (Candidate *candidate);
static void        candidate_next            (Candidate *candidate);
static void        candidate_prev_page       (Candidate *candidate);
static void        candidate_next_page       (Candidate *candidate);
static const Hanja* candidate_get_current    (Candidate *candidate);
static const Hanja* candidate_get_nth        (Candidate *candidate, int index);
static void        candidate_delete          (Candidate *candidate);

static void	im_hangul_class_init	     (GtkIMContextHangulClass *klass);
static void	im_hangul_ic_init		     (GtkIMContextHangul *hcontext);
static void	im_hangul_ic_finalize	     (GObject *obj);

static void	im_hangul_ic_reset           (GtkIMContext *context);
static gboolean	im_hangul_ic_slave_filter_keypress (GtkIMContext *context,
					      GdkEventKey  *key);
static gboolean	im_hangul_ic_filter_keypress (GtkIMContext *context,
					      GdkEventKey  *key);

static void	im_hangul_get_preedit_string (GtkIMContext  *ic,
					      gchar	    **str,
					      PangoAttrList **attrs,
					      gint	    *cursor_pos);

static void	im_hangul_ic_focus_in	     (GtkIMContext *context);
static void	im_hangul_ic_focus_out	     (GtkIMContext *context);
static void	im_hangul_ic_set_client_window  (GtkIMContext *context,
					      GdkWindow    *client_window);
static void	im_hangul_ic_set_use_preedit (GtkIMContext *context,
    					      gboolean     use_preedit);
static void	im_hangul_ic_cursor_location (GtkIMContext *context,
    					      GdkRectangle *area);

/* asistant function for hangul composer */
static inline gboolean im_hangul_is_modifier  (guint state);
static inline gboolean im_hangul_is_trigger   (GdkEventKey *key);
static inline gboolean im_hangul_is_backspace (GdkEventKey *key);
static inline void     im_hangul_ic_emit_preedit_changed (GtkIMContextHangul *hcontext);

/* commit functions */
static void     im_hangul_ic_commit_by_slave (GtkIMContext *context,
					      gchar *str, gpointer data);
static void     im_hangul_ic_preedit_start_by_slave (GtkIMContext* context,
						gpointer data);
static void     im_hangul_ic_preedit_end_by_slave (GtkIMContext* context,
						gpointer data);
static void     im_hangul_ic_preedit_changed_by_slave (GtkIMContext* context,
						gpointer data);
static gboolean im_hangul_ic_delete_surrounding_by_slave (GtkIMContext *context,
						gint offset,
						gint n_chars,
						gpointer data);
static gboolean im_hangul_ic_retrieve_surrounding_by_slave (GtkIMContext *context,
						gpointer data);

/* for feedback (preedit attribute) */
static void	im_hangul_preedit_underline  (GtkIMContextHangul *hic,
					      PangoAttrList **attrs,
					      gint start, gint end);
static void	im_hangul_preedit_reverse    (GtkIMContextHangul *hic,
					      PangoAttrList **attrs,
					      gint start, gint end);
static void	im_hangul_preedit_shade      (GtkIMContextHangul *hic,
					      PangoAttrList **attrs,
					      gint start, gint end);
static void	im_hangul_preedit_foreground (GtkIMContextHangul *hic,
					      PangoAttrList **attrs,
					      gint start, gint end);
static void	im_hangul_preedit_background (GtkIMContextHangul *hic,
					      PangoAttrList **attrs,
					      gint start, gint end);
static void	im_hangul_preedit_color      (GtkIMContextHangul *hic,
					      PangoAttrList **attrs,
					      gint start, gint end);
static void	im_hangul_preedit_normal     (GtkIMContextHangul *hic,
					      PangoAttrList **attrs,
					      gint start, gint end);

static char*    im_hangul_get_candidate_string(GtkIMContextHangul *ic);

static void     im_hangul_ic_show_status_window     (GtkIMContextHangul *hcontext);
static void     im_hangul_ic_hide_status_window     (GtkIMContextHangul *hcontext);
static int      im_hangul_ic_get_toplevel_input_mode(GtkIMContextHangul *hcontext);
static void     im_hangul_ic_set_toplevel_input_mode(GtkIMContextHangul *hcontext,
						  int mode);

static Toplevel*  toplevel_new(GtkWidget *toplevel_widget);
static Toplevel*  toplevel_get(GdkWindow *window);
static void       toplevel_append_context(Toplevel *toplevel,
					  GtkIMContextHangul *context);
static void       toplevel_remove_context(Toplevel *toplevel,
					  GtkIMContextHangul *context);
static void       toplevel_delete(Toplevel *toplevel);
static GtkWidget* status_window_new(GtkWidget *parent);

static void popup_candidate_window  (GtkIMContextHangul *hcontext);
static void close_candidate_window  (GtkIMContextHangul *hic);

GType gtk_type_im_context_hangul = 0;

/* static variables for hangul immodule */
static GObjectClass *parent_class;

static GSList          *toplevels = NULL;

static guint		snooper_handler_id = 0;
static GtkIMContext    *current_focused_ic = NULL;

static HanjaTable*      hanja_table = NULL;

/* preferences */
static gboolean		pref_use_capslock = FALSE;
static gboolean		pref_use_status_window = FALSE;
static gboolean		pref_use_dvorak = FALSE;
static gboolean		pref_use_system_keymap = FALSE;
static gboolean		pref_use_preedit_string = TRUE;
static void		(*im_hangul_preedit_attr)(GtkIMContextHangul *hic,
						  PangoAttrList **attrs,
						  gint start,
						  gint end) =
						im_hangul_preedit_foreground;
static GdkColor		pref_fg = { 0, 0xeeee, 0, 0 };
static GdkColor		pref_bg = { 0, 0xFFFF, 0xFFFF, 0xFFFF };

/* scanner */
static const GScannerConfig im_hangul_scanner_config = {
    (
     " \t\r\n"
    )                    /* cset_skip_characters */,
    (
     G_CSET_a_2_z
     "_"
     G_CSET_A_2_Z
    )                    /* cset_identifier_first */,
    (
     G_CSET_a_2_z
     "_"
     G_CSET_A_2_Z
     G_CSET_DIGITS
     G_CSET_LATINS
     G_CSET_LATINC
    )                    /* cset_identifier_nth */,
    ( "#\n" )             /* cpair_comment_single */,

    FALSE                 /* case_sensitive */,

    TRUE                  /* skip_comment_multi */,
    TRUE                  /* skip_comment_single */,
    TRUE                  /* scan_comment_multi */,
    TRUE                  /* scan_identifier */,
    FALSE                 /* scan_identifier_1char */,
    FALSE                 /* scan_identifier_NULL */,
    TRUE                  /* scan_symbols */,
    FALSE                 /* scan_binary */,
    TRUE                  /* scan_octal */,
    TRUE                  /* scan_float */,
    TRUE                  /* scan_hex */,
    FALSE                 /* scan_hex_dollar */,
    TRUE                  /* scan_string_sq */,
    TRUE                  /* scan_string_dq */,
    TRUE                  /* numbers_2_int */,
    FALSE                 /* int_2_float */,
    FALSE                 /* identifier_2_string */,
    TRUE                  /* char_2_token */,
    TRUE                  /* symbol_2_token */,
    FALSE                 /* scope_0_fallback */,
    FALSE                 /* store_int64 */,
};

enum {
    TOKEN_FALSE = G_TOKEN_LAST,
    TOKEN_TRUE,
    TOKEN_ENABLE_STATUS_WINDOW,
    TOKEN_ENABLE_PREEDIT,
    TOKEN_ENABLE_CAPSLOCK,
    TOKEN_ENABLE_DVORAK,
    TOKEN_ENABLE_SYSTEM_KEYMAP,
    TOKEN_PREEDIT_STYLE,
    TOKEN_PREEDIT_STYLE_FG,
    TOKEN_PREEDIT_STYLE_BG
};

static const struct {
    char *name;
    guint token;
} symbols[] = {
    { "false", TOKEN_FALSE },
    { "true", TOKEN_TRUE },
    { "off", TOKEN_FALSE },
    { "on", TOKEN_TRUE },
    { "enable_status_window", TOKEN_ENABLE_STATUS_WINDOW },
    { "enable_preedit", TOKEN_ENABLE_PREEDIT },
    { "enable_capslock", TOKEN_ENABLE_CAPSLOCK },
    { "enable_dvorak", TOKEN_ENABLE_DVORAK },
    { "enable_system_keymap", TOKEN_ENABLE_SYSTEM_KEYMAP },
    { "preedit_style", TOKEN_PREEDIT_STYLE },
    { "preedit_style_fg", TOKEN_PREEDIT_STYLE_FG },
    { "preedit_style_bg", TOKEN_PREEDIT_STYLE_BG }
};

static void
set_preedit_style (const char *style)
{
    if (style == NULL) {
	im_hangul_preedit_attr = im_hangul_preedit_foreground;
    } else if (g_ascii_strcasecmp(style, "underline") == 0) {
	im_hangul_preedit_attr = im_hangul_preedit_underline;
    } else if (g_ascii_strcasecmp(style, "reverse") == 0) {
	im_hangul_preedit_attr = im_hangul_preedit_reverse;
    } else if (g_ascii_strcasecmp(style, "shade") == 0) {
	im_hangul_preedit_attr = im_hangul_preedit_shade;
    } else if (g_ascii_strcasecmp(style, "foreground") == 0) {
	im_hangul_preedit_attr = im_hangul_preedit_foreground;
    } else if (g_ascii_strcasecmp(style, "background") == 0) {
	im_hangul_preedit_attr = im_hangul_preedit_background;
    } else if (g_ascii_strcasecmp(style, "color") == 0) {
	im_hangul_preedit_attr = im_hangul_preedit_color;
    } else if (g_ascii_strcasecmp(style, "normal") == 0) {
	im_hangul_preedit_attr = im_hangul_preedit_normal;
    } else {
	im_hangul_preedit_attr = im_hangul_preedit_foreground;
    }
}

void im_hangul_config_parser(void)
{
    int i;
    int fd;
    FILE *file;
    GScanner *scanner;
    const gchar *env_conf_file;
    gchar *conf_file = NULL; 
    guint type;
    GTokenValue value;
    char *str;

    env_conf_file = g_getenv("IM_HANGUL_CONF_FILE");
    if (env_conf_file == NULL) {
	const gchar *homedir = g_get_home_dir();
	if (homedir == NULL)
	    return;

	conf_file = g_build_filename(homedir, ".imhangul.conf", NULL);
    } else {
	conf_file = g_strdup(env_conf_file);
    }

    file = fopen(conf_file, "r");
    g_free(conf_file);
    if (file == NULL)
	return;

    fd = fileno(file);
    scanner = g_scanner_new(&im_hangul_scanner_config);
    g_scanner_input_file(scanner, fd);

    for (i = 0; i < G_N_ELEMENTS (symbols); i++) {
	g_scanner_scope_add_symbol(scanner, 0,
			   symbols[i].name, GINT_TO_POINTER(symbols[i].token));
    }

    do {
	type = g_scanner_get_next_token(scanner);
	if (type == TOKEN_ENABLE_PREEDIT) {
	    type = g_scanner_get_next_token(scanner);
	    if (type == G_TOKEN_EQUAL_SIGN) {
		type = g_scanner_get_next_token(scanner);
		if (type == TOKEN_TRUE) {
		    pref_use_preedit_string = TRUE;
		} else {
		    pref_use_preedit_string = FALSE;
		}
	    }
	} else if (type == TOKEN_ENABLE_STATUS_WINDOW) {
	    type = g_scanner_get_next_token(scanner);
	    if (type == G_TOKEN_EQUAL_SIGN) {
		type = g_scanner_get_next_token(scanner);
		if (type == TOKEN_TRUE) {
		    pref_use_status_window = TRUE;
		} else {
		    pref_use_status_window = FALSE;
		}
	    }
	} else if (type == TOKEN_ENABLE_CAPSLOCK) {
	    type = g_scanner_get_next_token(scanner);
	    if (type == G_TOKEN_EQUAL_SIGN) {
		type = g_scanner_get_next_token(scanner);
		if (type == TOKEN_TRUE) {
		    pref_use_capslock = TRUE;
		} else {
		    pref_use_capslock = FALSE;
		}
	    }
	} else if (type == TOKEN_ENABLE_DVORAK) {
	    type = g_scanner_get_next_token(scanner);
	    if (type == G_TOKEN_EQUAL_SIGN) {
		type = g_scanner_get_next_token(scanner);
		if (type == TOKEN_TRUE) {
		    pref_use_dvorak = TRUE;
		} else {
		    pref_use_dvorak = FALSE;
		}
	    }
	} else if (type == TOKEN_ENABLE_SYSTEM_KEYMAP) {
	    type = g_scanner_get_next_token(scanner);
	    if (type == G_TOKEN_EQUAL_SIGN) {
		type = g_scanner_get_next_token(scanner);
		if (type == TOKEN_TRUE) {
		    pref_use_system_keymap = TRUE;
		} else {
		    pref_use_system_keymap = FALSE;
		}
	    }
	} else if (type == TOKEN_PREEDIT_STYLE) {
	    type = g_scanner_get_next_token(scanner);
	    if (type == G_TOKEN_EQUAL_SIGN) {
		type = g_scanner_get_next_token(scanner);
		if (type == G_TOKEN_IDENTIFIER) {
		    value = g_scanner_cur_value(scanner);
		    str = value.v_identifier;
		    set_preedit_style(str);
		}
	    }
	} else if (type == TOKEN_PREEDIT_STYLE_FG) {
	    type = g_scanner_get_next_token(scanner);
	    if (type == G_TOKEN_EQUAL_SIGN) {
		type = g_scanner_get_next_token(scanner);
		if (type == G_TOKEN_STRING) {
		    value = g_scanner_cur_value(scanner);
		    str = value.v_identifier;
		    gdk_color_parse(str, &pref_fg);
		}
	    }
	} else if (type == TOKEN_PREEDIT_STYLE_BG) {
	    type = g_scanner_get_next_token(scanner);
	    if (type == G_TOKEN_EQUAL_SIGN) {
		type = g_scanner_get_next_token(scanner);
		if (type == G_TOKEN_STRING) {
		    value = g_scanner_cur_value(scanner);
		    str = value.v_identifier;
		    gdk_color_parse(str, &pref_bg);
		}
	    }
	} else {
	    type = g_scanner_get_next_token(scanner);
	    if (type == G_TOKEN_EQUAL_SIGN) {
		type = g_scanner_get_next_token(scanner);
	    }
	}
    } while (!g_scanner_eof(scanner));

    g_scanner_destroy(scanner);

    fclose(file);
}

void
gtk_im_context_hangul_register_type (GTypeModule *type_module)
{
  static const GTypeInfo im_context_hangul_info = {
    sizeof(GtkIMContextHangulClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) im_hangul_class_init,
    NULL,
    NULL,
    sizeof(GtkIMContextHangul),
    0,
    (GInstanceInitFunc) im_hangul_ic_init,
  };

  gtk_type_im_context_hangul =
      g_type_module_register_type (type_module,
				   GTK_TYPE_IM_CONTEXT,
				   "GtkIMContextHangul",
				   &im_context_hangul_info, 0);
}

static void 
im_hangul_class_init (GtkIMContextHangulClass *klass)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS(klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  parent_class = g_type_class_peek_parent (klass);

  im_context_class->set_client_window = im_hangul_ic_set_client_window;
  im_context_class->filter_keypress = im_hangul_ic_slave_filter_keypress;
  im_context_class->reset = im_hangul_ic_reset;
  im_context_class->focus_in = im_hangul_ic_focus_in;
  im_context_class->focus_out = im_hangul_ic_focus_out;
  im_context_class->get_preedit_string = im_hangul_get_preedit_string;
  im_context_class->set_use_preedit = im_hangul_ic_set_use_preedit;
  im_context_class->set_cursor_location = im_hangul_ic_cursor_location;

  gobject_class->finalize = im_hangul_ic_finalize;
}

static void 
im_hangul_ic_init (GtkIMContextHangul *hcontext)
{
  hcontext->slave = gtk_im_context_simple_new();
  hcontext->slave_preedit_started = FALSE;
  g_signal_connect(hcontext->slave, "commit",
		   G_CALLBACK(im_hangul_ic_commit_by_slave), hcontext);
  g_signal_connect(hcontext->slave, "preedit-start",
		   G_CALLBACK(im_hangul_ic_preedit_start_by_slave), hcontext);
  g_signal_connect(hcontext->slave, "preedit-end",
		   G_CALLBACK(im_hangul_ic_preedit_end_by_slave), hcontext);
  g_signal_connect(hcontext->slave, "preedit-changed",
		   G_CALLBACK(im_hangul_ic_preedit_changed_by_slave), hcontext);
  g_signal_connect(hcontext->slave, "delete-surrounding",
		   G_CALLBACK(im_hangul_ic_delete_surrounding_by_slave), hcontext);
  g_signal_connect(hcontext->slave, "retrieve-surrounding",
		   G_CALLBACK(im_hangul_ic_retrieve_surrounding_by_slave), hcontext);

  hcontext->client_window = NULL;
  hcontext->toplevel = NULL;
  hcontext->cursor.x = 0;
  hcontext->cursor.y = 0;
  hcontext->cursor.width = -1;
  hcontext->cursor.height = -1;

  hcontext->hic = hangul_ic_new("2");
  hcontext->preedit = g_string_new(NULL);

  hcontext->candidate = NULL;
  hcontext->candidate_string = NULL;

  /* options */
  hcontext->use_preedit = TRUE;
}

static void
im_hangul_ic_finalize (GObject *object)
{
  GtkIMContextHangul *hic = GTK_IM_CONTEXT_HANGUL(object);

  if (hic->toplevel != NULL)
    toplevel_remove_context(hic->toplevel, hic);

  hangul_ic_delete(hic->hic);
  g_string_free(hic->preedit, TRUE);

  gtk_im_context_reset(hic->slave);
  g_signal_handlers_disconnect_by_func(hic->slave,
				       im_hangul_ic_commit_by_slave,
				       object);
  g_object_unref(G_OBJECT(hic->slave));
  hic->slave = NULL;

  G_OBJECT_CLASS(parent_class)->finalize (object);
  if ((GObject*)current_focused_ic == object)
    current_focused_ic = NULL;
}

static void
im_hangul_ic_set_client_window (GtkIMContext *context,
			     GdkWindow *client_window)
{
  GtkIMContextHangul *hcontext;

  g_return_if_fail (context != NULL);
  g_return_if_fail (GTK_IS_IM_CONTEXT_HANGUL (context));

  hcontext = GTK_IM_CONTEXT_HANGUL(context);

  if (hcontext->client_window == client_window)
    return;

  if (hcontext->toplevel != NULL)
    toplevel_remove_context(hcontext->toplevel, hcontext);

  if (client_window == NULL) {
    hcontext->client_window = NULL;
    hcontext->toplevel = NULL;
    return;
  }

  hcontext->client_window = client_window;
  hcontext->toplevel = toplevel_get (client_window);
  toplevel_append_context(hcontext->toplevel, hcontext);
}

GtkIMContext *
gtk_im_context_hangul_new (void)
{
  return GTK_IM_CONTEXT (g_object_new (GTK_TYPE_IM_CONTEXT_HANGUL, NULL));
}

void
gtk_im_context_hangul_select_keyboard(GtkIMContextHangul *hcontext,
			              const char *keyboard)
{
    g_return_if_fail (hcontext);

    hangul_ic_select_keyboard(hcontext->hic, keyboard);
}

static void
im_hangul_set_input_mode_info_for_screen (GdkScreen *screen, int state)
{
  if (screen != NULL) {
    GdkWindow *root_window = gdk_screen_get_root_window(screen);
    long data = state;
    gdk_property_change (root_window,
			 gdk_atom_intern ("_HANGUL_INPUT_MODE", FALSE),
			 gdk_atom_intern ("INTEGER", FALSE),
			 32, GDK_PROP_MODE_REPLACE,
			 (const guchar *)&data, 1);
  }
}

static void
im_hangul_set_input_mode_info (GdkWindow *window, int state)
{
  if (window != NULL) {
    GdkScreen *screen = gdk_drawable_get_screen(window);
    im_hangul_set_input_mode_info_for_screen (screen, state);
  }
}

static void
im_hangul_set_input_mode(GtkIMContextHangul *hcontext, int mode)
{
  switch (mode) {
    case INPUT_MODE_DIRECT:
      im_hangul_set_input_mode_info (hcontext->client_window,
				     INPUT_MODE_INFO_ENGLISH);
      im_hangul_ic_hide_status_window(hcontext);
      g_signal_emit_by_name (hcontext, "preedit_end");
      break;
    case INPUT_MODE_HANGUL:
      im_hangul_set_input_mode_info (hcontext->client_window,
				     INPUT_MODE_INFO_HANGUL);
      im_hangul_ic_show_status_window(hcontext);
      g_signal_emit_by_name (hcontext, "preedit_start");
      break;
  }
  im_hangul_ic_set_toplevel_input_mode(hcontext, mode);
}

static void
im_hangul_preedit_underline (GtkIMContextHangul *hic,
			     PangoAttrList **attrs, gint start, gint end)
{
    PangoAttribute *attr;

    *attrs = pango_attr_list_new ();
    attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
    attr->start_index = start;
    attr->end_index = end;
    pango_attr_list_insert (*attrs, attr);
}

static void
im_hangul_preedit_reverse (GtkIMContextHangul *hic,
			   PangoAttrList **attrs, gint start, gint end)
{
    static GdkColor default_base = { 0, 0xffff, 0xffff, 0xffff };
    static GdkColor default_text = { 0, 0, 0, 0 };

    PangoAttribute *attr;
    GtkWidget *widget = NULL;
    GdkColor *fg = &default_base;
    GdkColor *bg = &default_text;

    gdk_window_get_user_data(hic->client_window, (gpointer)&widget);
    if (widget != NULL) {
	GtkStyle *style = gtk_widget_get_style(widget);
	fg = &style->base[GTK_STATE_NORMAL];
	bg = &style->text[GTK_STATE_NORMAL];
    }

    *attrs = pango_attr_list_new ();
    attr = pango_attr_foreground_new (fg->red, fg->green, fg->blue);
    attr->start_index = start;
    attr->end_index = end;
    pango_attr_list_insert (*attrs, attr);

    attr = pango_attr_background_new (bg->red, bg->green, bg->blue);
    attr->start_index = start;
    attr->end_index = end;
    pango_attr_list_insert (*attrs, attr);
}

static void
im_hangul_preedit_shade (GtkIMContextHangul *hic,
			   PangoAttrList **attrs, gint start, gint end)
{
    static const GdkColor default_text = { 0, 0, 0, 0 };
    static const GdkColor default_base = { 0, 0xffff * 90 / 100,
					      0xffff * 90 / 100,
					      0xffff * 90 / 100 };

    PangoAttribute *attr;
    GtkWidget *widget = NULL;
    GdkColor fg = default_text;
    GdkColor bg = default_base;

    gdk_window_get_user_data(hic->client_window, (gpointer)&widget);
    if (widget != NULL) {
	GtkStyle *style = gtk_widget_get_style(widget);
	if (style != NULL) {
	    fg.red   = style->text[GTK_STATE_NORMAL].red;
	    fg.green = style->text[GTK_STATE_NORMAL].green;
	    fg.blue  = style->text[GTK_STATE_NORMAL].blue;
	    bg.red   = (style->base[GTK_STATE_NORMAL].red   * 90 +
			style->text[GTK_STATE_NORMAL].red   * 10) / 100;
	    bg.green = (style->base[GTK_STATE_NORMAL].green * 90 +
			style->text[GTK_STATE_NORMAL].green * 10) / 100;
	    bg.blue  = (style->base[GTK_STATE_NORMAL].blue  * 90 +
			style->text[GTK_STATE_NORMAL].blue  * 10) / 100;
	}
    }

    *attrs = pango_attr_list_new ();
    attr = pango_attr_foreground_new (fg.red, fg.green, fg.blue);
    attr->start_index = start;
    attr->end_index = end;
    pango_attr_list_insert (*attrs, attr);

    attr = pango_attr_background_new (bg.red, bg.green, bg.blue);
    attr->start_index = start;
    attr->end_index = end;
    pango_attr_list_insert (*attrs, attr);
}

static void
im_hangul_preedit_foreground (GtkIMContextHangul *hic,
			      PangoAttrList **attrs, gint start, gint end)
{
  PangoAttribute *attr;

  *attrs = pango_attr_list_new ();
  attr = pango_attr_foreground_new (pref_fg.red, pref_fg.green, pref_fg.blue);
  attr->start_index = start;
  attr->end_index = end;
  pango_attr_list_insert (*attrs, attr);
}

static void
im_hangul_preedit_background (GtkIMContextHangul *hic,
			      PangoAttrList **attrs, gint start, gint end)
{
  PangoAttribute *attr;

  *attrs = pango_attr_list_new ();
  attr = pango_attr_background_new (pref_bg.red, pref_bg.green, pref_bg.blue);
  attr->start_index = start;
  attr->end_index = end;
  pango_attr_list_insert (*attrs, attr);
}

static void
im_hangul_preedit_color (GtkIMContextHangul *hic,
		         PangoAttrList **attrs, gint start, gint end)
{
  PangoAttribute *attr;

  *attrs = pango_attr_list_new ();
  attr = pango_attr_foreground_new (pref_fg.red, pref_fg.green, pref_fg.blue);
  attr->start_index = start;
  attr->end_index = end;
  pango_attr_list_insert (*attrs, attr);

  attr = pango_attr_background_new (pref_bg.red, pref_bg.green, pref_bg.blue);
  attr->start_index = start;
  attr->end_index = end;
  pango_attr_list_insert (*attrs, attr);
}

static void
im_hangul_preedit_normal (GtkIMContextHangul *hic,
			  PangoAttrList **attrs, gint start, gint end)
{
  /* we do nothing */
  *attrs = pango_attr_list_new ();
}

static void
im_hangul_get_preedit_string (GtkIMContext *context, gchar **str,
			      PangoAttrList **attrs,
			      gint *cursor_pos)
{
    int len;
    GtkIMContextHangul *ic;

    g_return_if_fail (context != NULL);

    ic = GTK_IM_CONTEXT_HANGUL(context);

    if (ic->slave_preedit_started) {
	gtk_im_context_get_preedit_string(ic->slave, str, attrs, cursor_pos); 
    } else {
	len = g_utf8_strlen(ic->preedit->str, -1);
	if (attrs)
	    im_hangul_preedit_attr(ic, attrs, 0, ic->preedit->len);

	if (cursor_pos)
	    *cursor_pos = len;

	if (str)
	    *str = g_strdup(ic->preedit->str);
    }
}

static void
im_hangul_ic_focus_in (GtkIMContext *context)
{
  int input_mode;
  GtkIMContextHangul *hcontext;

  g_return_if_fail (context != NULL);

  hcontext = GTK_IM_CONTEXT_HANGUL(context);
  input_mode = im_hangul_ic_get_toplevel_input_mode(hcontext);
  im_hangul_set_input_mode(hcontext, input_mode);

  current_focused_ic = context;
}

static void
im_hangul_ic_set_preedit(GtkIMContextHangul* hic, const ucschar* preedit)
{
    int i;
    char* old;

    old = g_strdup(hic->preedit->str);

    g_string_assign(hic->preedit, "");
    if (preedit != NULL) {
	for (i = 0; preedit[i] != 0; i++) {
	    g_string_append_unichar(hic->preedit, preedit[i]);
	}
    }

    // preedit string이 바뀌지 않았는데도 preedit changed signal을 너무 자주
    // 보내게 되면 오작동하는 프로그램이 있을 수 있다.
    // GtkHtml 같은 것은 backspace키를 처리하는 과정에서도 reset을 부르는데
    // 여기서 매번 preedit changed signal을 보내면 오작동한다.
    if (strcmp(hic->preedit->str, old) != 0)
	im_hangul_ic_emit_preedit_changed(hic);

    g_free(old);
}

static inline void
im_hangul_ic_emit_preedit_changed (GtkIMContextHangul *hcontext)
{
  if (hcontext->use_preedit)
    g_signal_emit_by_name (hcontext, "preedit_changed");
}

static void
im_hangul_ic_focus_out (GtkIMContext *context)
{
  GtkIMContextHangul *hcontext;

  g_return_if_fail (context != NULL);

  im_hangul_ic_reset(context);

  hcontext = GTK_IM_CONTEXT_HANGUL(context);
  im_hangul_ic_hide_status_window (hcontext);
  im_hangul_set_input_mode_info (hcontext->client_window, INPUT_MODE_INFO_NONE);
  if (current_focused_ic == context)
    current_focused_ic = NULL;
}

static void
im_hangul_ic_set_use_preedit (GtkIMContext *context, gboolean use_preedit)
{
  GtkIMContextHangul *hcontext;

  g_return_if_fail (context != NULL);

  hcontext = GTK_IM_CONTEXT_HANGUL(context);
  hcontext->use_preedit = use_preedit;
}

static void
im_hangul_ic_cursor_location (GtkIMContext *context, GdkRectangle *area)
{
  GtkIMContextHangul *hcontext;

  g_return_if_fail (context != NULL);

  hcontext = GTK_IM_CONTEXT_HANGUL(context);
  hcontext->cursor = *area;
}

static inline gboolean
im_hangul_is_modifier (guint state)
{
  return ((state & GDK_CONTROL_MASK) || (state & GDK_MOD1_MASK));
}

static inline gboolean
im_hangul_is_trigger (GdkEventKey *key)
{
  return ( key->keyval == GDK_Hangul || 
	   key->keyval == GDK_Alt_R ||
	  (key->keyval == GDK_space && (key->state & GDK_SHIFT_MASK)));
}

static inline gboolean
im_hangul_is_candidate (GdkEventKey *key)
{
  return (key->keyval == GDK_Hangul_Hanja ||
	  key->keyval == GDK_F9 ||
          key->keyval == GDK_Control_R);
}

static inline gboolean
im_hangul_is_backspace (GdkEventKey *key)
{
  return (key->keyval == GDK_BackSpace);
}

static void
im_hangul_ic_reset (GtkIMContext *context)
{
    const ucschar* preedit;
    const ucschar* flush;
    GtkIMContextHangul *hic = GTK_IM_CONTEXT_HANGUL (context);

    flush = hangul_ic_flush(hic->hic);

    preedit = hangul_ic_get_preedit_string(hic->hic);
    im_hangul_ic_set_preedit(hic, preedit);

    if (flush[0] != 0) {
	char* str = g_ucs4_to_utf8(flush, -1, NULL, NULL, NULL);
	g_signal_emit_by_name(hic, "commit", str);
	g_free(str);
    }
}

static gboolean
im_hangul_handle_direct_mode (GtkIMContextHangul *hcontext,
			      GdkEventKey *key)
{
    if (im_hangul_is_trigger (key)) {
	im_hangul_ic_reset(GTK_IM_CONTEXT(hcontext));
	im_hangul_set_input_mode(hcontext, INPUT_MODE_HANGUL);
	return TRUE;
    }
    return FALSE;
}

static void
im_hangul_ic_commit_by_slave (GtkIMContext *context, gchar *str, gpointer data)
{
  g_signal_emit_by_name (GTK_IM_CONTEXT_HANGUL(data), "commit", str);
}

static void
im_hangul_ic_preedit_start_by_slave (GtkIMContext *context, gpointer data)
{
    GtkIMContextHangul *ic = GTK_IM_CONTEXT_HANGUL (data);

    ic->slave_preedit_started = TRUE;
    g_signal_emit_by_name (ic, "preedit-start");
}

static void
im_hangul_ic_preedit_end_by_slave (GtkIMContext *context, gpointer data)
{
    GtkIMContextHangul *ic = GTK_IM_CONTEXT_HANGUL (data);

    ic->slave_preedit_started = FALSE;
    g_signal_emit_by_name (ic, "preedit-end");
}

static void
im_hangul_ic_preedit_changed_by_slave (GtkIMContext *context, gpointer data)
{
    g_signal_emit_by_name (GTK_IM_CONTEXT_HANGUL(data), "preedit-changed");
}

static gboolean
im_hangul_ic_delete_surrounding_by_slave (GtkIMContext *context,
					  gint offset,
					  gint n_chars,
					  gpointer data)
{
    gboolean ret = FALSE;
    g_signal_emit_by_name (GTK_IM_CONTEXT_HANGUL(data), "delete-surrounding",
			     offset, n_chars, &ret);
    return ret;
}

static gboolean
im_hangul_ic_retrieve_surrounding_by_slave (GtkIMContext *context, gpointer data)
{
    gboolean ret = FALSE;
    g_signal_emit_by_name (GTK_IM_CONTEXT_HANGUL(data), "retrieve-surrounding",
			     &ret);
    return ret;
}

/* this is a very dangerous function:
 * safe only when GDKKEYSYMS's value is enumarated  */
static guint
im_hangul_dvorak_to_qwerty (guint code)
{
  /* maybe safe if we use switch statement */
  static guint table[] = {
    GDK_exclam,			/* GDK_exclam */
    GDK_Q,			/* GDK_quotedbl */
    GDK_numbersign,		/* GDK_numbersign */
    GDK_dollar,			/* GDK_dollar */
    GDK_percent,		/* GDK_percent */
    GDK_ampersand,		/* GDK_ampersand */
    GDK_q,			/* GDK_apostrophe */
    GDK_parenleft,		/* GDK_parenleft */
    GDK_parenright,		/* GDK_parenright */
    GDK_asterisk,		/* GDK_asterisk */
    GDK_braceright,		/* GDK_plus */
    GDK_w,			/* GDK_comma */
    GDK_apostrophe,		/* GDK_minus */
    GDK_e,			/* GDK_period */
    GDK_bracketleft,		/* GDK_slash */
    GDK_0,			/* GDK_0 */
    GDK_1,			/* GDK_1 */
    GDK_2,			/* GDK_2 */
    GDK_3,			/* GDK_3 */
    GDK_4,			/* GDK_4 */
    GDK_5,			/* GDK_5 */
    GDK_6,			/* GDK_6 */
    GDK_7,			/* GDK_7 */
    GDK_8,			/* GDK_8 */
    GDK_9,			/* GDK_9 */
    GDK_Z,			/* GDK_colon */
    GDK_z,			/* GDK_semicolon */
    GDK_W,			/* GDK_less */
    GDK_bracketright,		/* GDK_qual */
    GDK_E,			/* GDK_greater */
    GDK_braceleft,		/* GDK_question */
    GDK_at,			/* GDK_at */
    GDK_A,			/* GDK_A */
    GDK_N,			/* GDK_B */
    GDK_I,			/* GDK_C */
    GDK_H,			/* GDK_D */
    GDK_D,			/* GDK_E */
    GDK_Y,			/* GDK_F */
    GDK_U,			/* GDK_G */
    GDK_J,			/* GDK_H */
    GDK_G,			/* GDK_I */
    GDK_C,			/* GDK_J */
    GDK_V,			/* GDK_K */
    GDK_P,			/* GDK_L */
    GDK_M,			/* GDK_M */
    GDK_L,			/* GDK_N */
    GDK_S,			/* GDK_O */
    GDK_R,			/* GDK_P */
    GDK_X,			/* GDK_Q */
    GDK_O,			/* GDK_R */
    GDK_colon,			/* GDK_S */
    GDK_K,			/* GDK_T */
    GDK_F,			/* GDK_U */
    GDK_greater,		/* GDK_V */
    GDK_less,			/* GDK_W */
    GDK_B,			/* GDK_X */
    GDK_T,			/* GDK_Y */
    GDK_question,		/* GDK_Z */
    GDK_minus,			/* GDK_bracketleft */
    GDK_backslash,		/* GDK_backslash */
    GDK_equal,			/* GDK_bracketright */
    GDK_asciicircum,		/* GDK_asciicircum */
    GDK_quotedbl,		/* GDK_underscore */
    GDK_grave,			/* GDK_grave */
    GDK_a,			/* GDK_a */
    GDK_n,			/* GDK_b */
    GDK_i,			/* GDK_c */
    GDK_h,			/* GDK_d */
    GDK_d,			/* GDK_e */
    GDK_y,			/* GDK_f */
    GDK_u,			/* GDK_g */
    GDK_j,			/* GDK_h */
    GDK_g,			/* GDK_i */
    GDK_c,			/* GDK_j */
    GDK_v,			/* GDK_k */
    GDK_p,			/* GDK_l */
    GDK_m,			/* GDK_m */
    GDK_l,			/* GDK_n */
    GDK_s,			/* GDK_o */
    GDK_r,			/* GDK_p */
    GDK_x,			/* GDK_q */
    GDK_o,			/* GDK_r */
    GDK_semicolon,		/* GDK_s */
    GDK_k,			/* GDK_t */
    GDK_f,			/* GDK_u */
    GDK_period,			/* GDK_v */
    GDK_comma,			/* GDK_w */
    GDK_b,			/* GDK_x */
    GDK_t,			/* GDK_y */
    GDK_slash,			/* GDK_z */
    GDK_underscore,		/* GDK_braceleft */
    GDK_bar,			/* GDK_bar */
    GDK_plus,			/* GDK_braceright */
    GDK_asciitilde,		/* GDK_asciitilde */
  };

  if (code < GDK_exclam || code > GDK_asciitilde)
    return code;
  return table[code - GDK_exclam];
}

static const guint keymap[][2] = {
    { GDK_1,             GDK_exclam         },  /* 10 */
    { GDK_2,             GDK_at             },  /* 11 */
    { GDK_3,             GDK_numbersign     },  /* 12 */
    { GDK_4,             GDK_dollar         },  /* 13 */
    { GDK_5,             GDK_percent        },  /* 14 */
    { GDK_6,             GDK_asciicircum    },  /* 15 */
    { GDK_7,             GDK_ampersand      },  /* 16 */
    { GDK_8,             GDK_asterisk       },  /* 17 */
    { GDK_9,             GDK_parenleft      },  /* 18 */
    { GDK_0,             GDK_parenright     },  /* 19 */
    { GDK_minus,         GDK_underscore     },  /* 20 */
    { GDK_equal,         GDK_plus           },  /* 21 */
    { GDK_BackSpace,     GDK_BackSpace      },  /* 22 */
    { GDK_Tab,           GDK_Tab            },  /* 23 */
    { GDK_q,             GDK_Q              },  /* 24 */
    { GDK_w,             GDK_W              },  /* 25 */
    { GDK_e,             GDK_E              },  /* 26 */
    { GDK_r,             GDK_R              },  /* 27 */
    { GDK_t,             GDK_T              },  /* 28 */
    { GDK_y,             GDK_Y              },  /* 29 */
    { GDK_u,             GDK_U              },  /* 30 */
    { GDK_i,             GDK_I              },  /* 31 */
    { GDK_o,             GDK_O              },  /* 32 */
    { GDK_p,             GDK_P              },  /* 33 */
    { GDK_bracketleft,   GDK_braceleft      },  /* 34 */
    { GDK_bracketright,  GDK_braceright     },  /* 35 */
    { GDK_Return,        GDK_Return         },  /* 36 */
    { GDK_Control_L,     GDK_Control_L      },  /* 37 */
    { GDK_a,             GDK_A              },  /* 38 */
    { GDK_s,             GDK_S              },  /* 39 */
    { GDK_d,             GDK_D              },  /* 40 */
    { GDK_f,             GDK_F              },  /* 41 */
    { GDK_g,             GDK_G              },  /* 42 */
    { GDK_h,             GDK_H              },  /* 43 */
    { GDK_j,             GDK_J              },  /* 44 */
    { GDK_k,             GDK_K              },  /* 45 */
    { GDK_l,             GDK_L              },  /* 46 */
    { GDK_semicolon,     GDK_colon          },  /* 47 */
    { GDK_apostrophe,    GDK_quotedbl       },  /* 48 */
    { GDK_grave,         GDK_asciitilde     },  /* 49 */
    { GDK_Shift_L,       GDK_Shift_L        },  /* 50 */
    { GDK_backslash,     GDK_bar            },  /* 51 */
    { GDK_z,             GDK_Z              },  /* 52 */
    { GDK_x,             GDK_X              },  /* 53 */
    { GDK_c,             GDK_C              },  /* 54 */
    { GDK_v,             GDK_V              },  /* 55 */
    { GDK_b,             GDK_B              },  /* 56 */
    { GDK_n,             GDK_N              },  /* 57 */
    { GDK_m,             GDK_M              },  /* 58 */
    { GDK_comma,         GDK_less           },  /* 59 */
    { GDK_period,        GDK_greater        },  /* 60 */
    { GDK_slash,         GDK_question       },  /* 61 */
};

/* 한글 입력기는 각 키의 위치에 따라서 입력되는 자모가 결정되어 있다. 
 * 그래서 키보드를 드보락이라든가, 유럽언어로 바꾸게 되면 각 키가 생성하는
 * 라틴문자가 qwerty와 달라지게 된다. 그 상태에서 keyval을 그대로 사용하면
 * 키의 영문자가 그 위치에 대한 정보를 가지지 못한 상태가 되므로 libhangul의 
 * 조합 함수를 사용할 수 없게 된다.
 * 그래서 GDK의 hardware_keycode값에서 keyval로 변환하는 내장 테이블을 사용하여
 * 어떤 경우에든 US qwerty 자판인 것과 같은 변환을 해줌으로써 사용자가 설정한
 * 자판 정보에 관계없이, 한글 입력이 제대로 되도록 한다.
 * 이렇게 고치게 되면, capslock 처리라던가, dvorak을 위한 처리가 따로 필요 없다.
 * 단 키보드 하드웨어가 달라서 hardware_keycode(scancode)가 다른 값이 나오는 
 * 키보드의 경우에는 한글 입력에 문제가 발생하게 될 가능성이 있다.
 * 그런 경우에는 예전과 같은 방식으로 동작하도록 system_keymap 옵션을 
 * 켜도록 한다.
 */
static gunichar
im_hangul_get_keyval(GtkIMContextHangul *hcontext,
		     guint	       keycode,
		     guint	       keyval,
		     guint	       state)
{
    /* hangul jamo keysym */
    if (keyval >= 0x01001100 && keyval <= 0x010011ff)
	return keyval & 0x0000ffff;

    if (pref_use_system_keymap) {
	/* treat for dvorak */
	if (pref_use_dvorak)
	    keyval = im_hangul_dvorak_to_qwerty (keyval);

	if (keyval >= GDK_exclam && keyval <= GDK_asciitilde) {
	    /* treat capslock, as capslock is not on */
	    if (state & GDK_LOCK_MASK) {
		if (state & GDK_SHIFT_MASK) {
		    if (keyval >= GDK_a && keyval <= GDK_z)
			keyval -= (GDK_a - GDK_A);
		} else {
		    if (keyval >= GDK_A && keyval <= GDK_Z)
			keyval += (GDK_a - GDK_A);
		}
	    }
	}
    } else {
	/* keycode가 10에서 61 범위에 있으면 내장 keymap을 이용해서 변환한다. */
	if (keycode >= 10 && keycode <= 61) {
	    if (state & GDK_SHIFT_MASK) {
		keyval = keymap[keycode - 10][1];
	    } else {
		keyval = keymap[keycode - 10][0];
	    }
	}
    }

    return keyval;
}

static void
im_hangul_candidate_commit(GtkIMContextHangul *ic,
			   const char* match_key,
			   const Hanja* hanja)
{
    const char* key;
    const char* value;

    key = hanja_get_key(hanja);
    value = hanja_get_value(hanja);
    if (value != NULL) {
	ucschar* candidate_str = (gunichar*)ic->candidate_string->data;
	int candidate_str_len = ic->candidate_string->len;
	int len_to_delete = g_utf8_strlen(key, -1);

	// 먼저 hangul_ic의 preedit string을 제거한다.
	if (!hangul_ic_is_empty(ic->hic)) {
	    const ucschar* preedit;
	    int preedit_len;
	    preedit = hangul_ic_get_preedit_string(ic->hic);
	    preedit_len = ucschar_strlen(preedit);

	    // 여기서 preedit가 자모 스트링이라면 preedit_len을 바로 빼면 
	    // 안되고, NFC normalize 한 스트링으로 해야 하는데
	    // 편의상 hangul_ic는 한번에 한 음절만 가지고 있다고 보고
	    // 1만 빼서 계산 한다.
	    len_to_delete -= 1;
	    // candidate_str에는 preedit 조차도 자모 스트링으로 들어 있을 수 
	    // 있으므로 preedit_len을 뺀다.
	    candidate_str_len -= preedit_len;
	    hangul_ic_reset(ic->hic);
	    im_hangul_ic_set_preedit(ic, NULL);
	}

	// candidate string은 자모스트링일 수도 있으므로 
	// 주의한다.
	if (len_to_delete > 0) {
	    int len = 0;
	    ucschar* end = candidate_str + candidate_str_len;
	    const ucschar* p = end;

	    // 끝에서부터 한음절씩 빼본다.
	    while (len_to_delete > 0) {
		p = hangul_syllable_iterator_prev(p, candidate_str);
		len_to_delete--;
	    }
	    len = end - p;
	    gtk_im_context_delete_surrounding(GTK_IM_CONTEXT(ic), -len, len);
	}

	g_signal_emit_by_name(ic, "commit", value);
	close_candidate_window(ic);
    }
}

static gboolean
im_hangul_cadidate_filter_keypress (GtkIMContextHangul *hcontext,
				    GdkEventKey *key)
{
  const Hanja* hanja = NULL;

  switch (key->keyval)
    {
      case GDK_Return:
      case GDK_KP_Enter:
	hanja = candidate_get_current(hcontext->candidate);
	break;
      case GDK_Left:
      case GDK_h:
      case GDK_Page_Up:
	candidate_prev_page(hcontext->candidate);
	break;
      case GDK_Right:
      case GDK_l:
      case GDK_Page_Down:
	candidate_next_page(hcontext->candidate);
	break;
      case GDK_Up:
      case GDK_k:
      case GDK_BackSpace:
      case GDK_KP_Subtract:
	candidate_prev(hcontext->candidate);
	break;
      case GDK_Down:
      case GDK_j:
      case GDK_space:
      case GDK_KP_Add:
      case GDK_KP_Tab:
	candidate_next(hcontext->candidate);
	break;
      case GDK_Escape:
	close_candidate_window(hcontext);
	break;
      case GDK_0:
	hanja = candidate_get_nth(hcontext->candidate, 9);
	break;
      case GDK_1:
      case GDK_2:
      case GDK_3:
      case GDK_4:
      case GDK_5:
      case GDK_6:
      case GDK_7:
      case GDK_8:
      case GDK_9:
	hanja = candidate_get_nth(hcontext->candidate, key->keyval - GDK_1);
	break;
      default:
	break;
    }

  if (hanja != NULL)
      im_hangul_candidate_commit(hcontext, hcontext->candidate->key, hanja);

  return TRUE;
}

static gboolean
im_hangul_ic_slave_filter_keypress (GtkIMContext *context, GdkEventKey *key)
{
  GtkIMContextHangul *hcontext;

  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  hcontext = GTK_IM_CONTEXT_HANGUL(context);
  return gtk_im_context_filter_keypress(hcontext->slave, key);
}

/* use hangul composer */
static gboolean
im_hangul_ic_filter_keypress (GtkIMContext *context, GdkEventKey *key)
{
  int keyval;
  bool res;
  const ucschar* commit;
  const ucschar* preedit;
  GtkIMContextHangul *hcontext;

  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  hcontext = GTK_IM_CONTEXT_HANGUL(context);

  /* process GtkIMContextSimple first if it has preedit string */
  if (hcontext->slave_preedit_started) {
    return FALSE;
  }

  /* ignore key release */
  if (key->type == GDK_KEY_RELEASE)
    return FALSE;

  /* we silently ignore shift keys */
  if (key->keyval == GDK_Shift_L || key->keyval == GDK_Shift_R)
    return FALSE;

  /* candidate window mode */
  if (hcontext->candidate != NULL)
    return im_hangul_cadidate_filter_keypress (hcontext, key);

  /* on capslock, we use Hangul Jamo */
  if (pref_use_capslock) {
      if (key->state & GDK_LOCK_MASK)
	  hangul_ic_set_output_mode(hcontext->hic, HANGUL_OUTPUT_JAMO);
      else
	  hangul_ic_set_output_mode(hcontext->hic, HANGUL_OUTPUT_SYLLABLE);
  }

  /* handle direct mode */
  if (im_hangul_ic_get_toplevel_input_mode(hcontext) == INPUT_MODE_DIRECT)
    return im_hangul_handle_direct_mode (hcontext, key);

  /* handle Escape key: automaticaly change to direct mode */
  if (key->keyval == GDK_Escape)
    {
      im_hangul_ic_reset(context);
      im_hangul_set_input_mode(hcontext, INPUT_MODE_DIRECT);
      return FALSE;
    }

  /* modifiler key */
  if (im_hangul_is_modifier (key->state))
    {
      im_hangul_ic_reset(context);
      return FALSE;
    }

  /* hanja key */
  if (im_hangul_is_candidate(key))
    {
      popup_candidate_window (hcontext);
      return TRUE;
    }

  /* trigger key: mode change to direct mode */
  if (im_hangul_is_trigger(key)) {
      im_hangul_ic_reset(context);
      im_hangul_set_input_mode(hcontext, INPUT_MODE_DIRECT);
      return TRUE;
  }

  /* backspace */
  if (im_hangul_is_backspace(key)) {
      res = hangul_ic_backspace(hcontext->hic);
      if (res) {
	  preedit = hangul_ic_get_preedit_string(hcontext->hic);
	  im_hangul_ic_set_preedit(hcontext, preedit);
      }
      return res;
  }

  /* process */
  keyval = im_hangul_get_keyval(hcontext,
			     key->hardware_keycode, key->keyval, key->state);
  res = hangul_ic_process(hcontext->hic, keyval);

  commit = hangul_ic_get_commit_string(hcontext->hic);
  if (commit[0] != 0) {
      char* str = g_ucs4_to_utf8(commit, -1, NULL, NULL, NULL);
      /* 몇몇 어플리케이션에서 입력기 관련 구현에 버그가 있어서
       * commit하기 전에 preedit string을 빈 스트링으로 만들지 
       * 않으면 오작동하는 경우가 있다. 이 문제를 피하기 위해서
       * commit하기 전에 preedit string을 빈 스트링으로 만든다. */
      im_hangul_ic_set_preedit(hcontext, NULL);
      g_signal_emit_by_name (hcontext, "commit", str);
      g_free(str);
  }

  preedit = hangul_ic_get_preedit_string(hcontext->hic);
  im_hangul_ic_set_preedit(hcontext, preedit);

  return res;
}

/* status window */
static gboolean
status_window_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  gdk_draw_rectangle (widget->window,
		      widget->style->fg_gc[GTK_STATE_NORMAL],
		      FALSE,
		      0, 0,
		      widget->allocation.width-1, widget->allocation.height-1);

  return FALSE;
}

static gboolean
status_window_configure	(GtkWidget *widget,
			 GdkEventConfigure *event,
			 Toplevel *toplevel)
{
  GdkRectangle rect;
  GtkRequisition requisition;
  gint y;

  if (toplevel == NULL || toplevel->status == NULL)
    return FALSE;

  gdk_window_get_frame_extents (widget->window, &rect);
  gtk_widget_size_request (toplevel->status, &requisition);

  if (rect.y + rect.height + requisition.height < gdk_screen_height ())
    y = rect.y + rect.height;
  else
    y = gdk_screen_height () - requisition.height;

  gtk_window_move (GTK_WINDOW(toplevel->status), rect.x, y);
  return FALSE;
}

static GtkWidget*
status_window_new(GtkWidget *parent)
{
  GtkWidget *window;
  GtkWidget *frame;
  GtkWidget *label;

  if (parent == NULL)
    return NULL;

  window = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_container_set_border_width (GTK_CONTAINER(window), 1);
  /* gtk_window_set_decorated (GTK_WINDOW(window), FALSE); */
  gtk_widget_set_name (window, "imhangul_status");
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_widget_set_app_paintable (window, TRUE);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER(window), frame);

  /* hangul status window label */
  label = gtk_label_new (_("hangul")); 
  gtk_container_add (GTK_CONTAINER(frame), label);
  gtk_widget_show (label);

  g_signal_connect (G_OBJECT(window), "expose-event",
		   G_CALLBACK(status_window_expose_event), NULL);

  return window;
}

static void
im_hangul_ic_show_status_window (GtkIMContextHangul *hcontext)
{
  g_return_if_fail (hcontext != NULL);

  if (pref_use_status_window && hcontext->toplevel != NULL) {
    if (hcontext->toplevel->status == NULL) {
      hcontext->toplevel->status =
	status_window_new(hcontext->toplevel->widget);
	status_window_configure (hcontext->toplevel->widget,
				 NULL,
				 hcontext->toplevel);
    }
    gtk_widget_show (hcontext->toplevel->status);
  }
}

static void
im_hangul_ic_hide_status_window (GtkIMContextHangul *hcontext)
{
  g_return_if_fail (hcontext != NULL);

  if (hcontext->toplevel != NULL && hcontext->toplevel->status != NULL) {
    gtk_widget_hide (hcontext->toplevel->status);
  }
}

static GtkWidget *
get_toplevel_widget (GdkWindow *window)
{
  GtkWidget *gtk_toplevel;
  gpointer ptr;

  if (window == NULL)
    return NULL;

  gdk_window_get_user_data (window, &ptr);
  memcpy(&gtk_toplevel, &ptr, sizeof(gtk_toplevel));
  if (gtk_toplevel != NULL)
    gtk_toplevel = gtk_widget_get_toplevel(GTK_WIDGET(gtk_toplevel));

  return gtk_toplevel;
}

static void
toplevel_destroy(Toplevel *toplevel)
{
  if (toplevel != NULL) {
    toplevel_delete(toplevel);
    toplevels = g_slist_remove(toplevels, toplevel);
  }
}

static Toplevel *
toplevel_new(GtkWidget *toplevel_widget)
{
  Toplevel *toplevel = NULL;

  toplevel = g_new(Toplevel, 1);
  toplevel->input_mode = INPUT_MODE_DIRECT;
  toplevel->widget = toplevel_widget;
  toplevel->status = NULL;
  toplevel->contexts = NULL;
  toplevel->destroy_handler_id = 
	    g_signal_connect_swapped (G_OBJECT(toplevel->widget), "destroy",
			     G_CALLBACK(toplevel_destroy), toplevel);
  toplevel->configure_handler_id = 
	    g_signal_connect (G_OBJECT(toplevel->widget), "configure-event",
			     G_CALLBACK(status_window_configure),
			     toplevel);

  g_object_set_data(G_OBJECT(toplevel_widget),
		     "gtk-imhangul-toplevel-info", toplevel);
  return toplevel;
}

static Toplevel *
toplevel_get(GdkWindow *window)
{
  Toplevel *toplevel = NULL;
  GtkWidget *toplevel_widget;

  toplevel_widget = get_toplevel_widget (window);
  if (toplevel_widget == NULL) {
    return NULL;
  }

  toplevel = g_object_get_data(G_OBJECT(toplevel_widget),
			       "gtk-imhangul-toplevel-info");
  if (toplevel == NULL) {
    toplevel = toplevel_new(toplevel_widget);
    toplevels = g_slist_prepend(toplevels, toplevel);
  }

  return toplevel;
}

static void
toplevel_remove_context(Toplevel *toplevel, GtkIMContextHangul *context)
{
  if (toplevel == NULL || context == NULL)
    return;

  toplevel->contexts = g_slist_remove(toplevel->contexts, context);
}

static void
toplevel_append_context(Toplevel *toplevel, GtkIMContextHangul *context)
{
  if (toplevel == NULL || context == NULL)
    return;

  toplevel->contexts = g_slist_prepend(toplevel->contexts, context);
}

static void
toplevel_delete(Toplevel *toplevel)
{
  if (toplevel != NULL) {
    if (toplevel->status != NULL) {
      gtk_widget_destroy(toplevel->status);
    }
    if (toplevel->contexts != NULL) {
      GSList *item = toplevel->contexts;
      while (item != NULL) {
	GtkIMContextHangul *context = (GtkIMContextHangul *)(item->data);
	context->toplevel = NULL;
	item = g_slist_next(item);
      }
      g_slist_free(toplevel->contexts);
    }
    g_signal_handler_disconnect (toplevel->widget,
				 toplevel->configure_handler_id);
    g_signal_handler_disconnect (toplevel->widget,
				 toplevel->destroy_handler_id);
    g_object_set_data (G_OBJECT(toplevel->widget),
		       "gtk-imhangul-toplevel-info", NULL);
    g_free(toplevel);
  }
}

static int
im_hangul_ic_get_toplevel_input_mode(GtkIMContextHangul *hcontext)
{
  if (hcontext->toplevel == NULL)
    return INPUT_MODE_DIRECT;
  return hcontext->toplevel->input_mode;
}

static void
im_hangul_ic_set_toplevel_input_mode(GtkIMContextHangul *hcontext, int mode)
{
  if (hcontext->toplevel != NULL)
    hcontext->toplevel->input_mode = mode;
}

/*
 * candidate selection window
 */
static char*
im_hangul_get_candidate_string(GtkIMContextHangul *ic)
{
    int n;
    gboolean res;
    gchar* text = NULL;
    gint cursor_index = 0;
    gunichar buf[20] = { 0, };
    char* str = NULL;

    n = G_N_ELEMENTS(buf);
    if (!hangul_ic_is_empty(ic->hic)) {
	const ucschar* preedit;
	int i, preedit_len;

	preedit = hangul_ic_get_preedit_string(ic->hic);
	preedit_len = ucschar_strlen(preedit);

	n -= preedit_len;
	for (i = 0; i < preedit_len; i++) {
	    buf[n + i] = preedit[i];
	}
    }

    res = gtk_im_context_get_surrounding(GTK_IM_CONTEXT(ic),
					 &text, &cursor_index);
    if (res && text != NULL) {
	gchar* p;

	p = g_utf8_find_prev_char(text, text + cursor_index);
	while (n > 0 && p != NULL) {
	    if (*p == ' ')
		break;

	    buf[n - 1] = g_utf8_get_char(p);

	    p = g_utf8_find_prev_char(text, p);
	    n--;
	}

	g_free(text);
    }

    if (n < G_N_ELEMENTS(buf)) {
	char* utf8;
	int len = G_N_ELEMENTS(buf) - n;
	if (ic->candidate_string == NULL) {
	    ic->candidate_string = g_array_sized_new(FALSE, FALSE,
						     sizeof(gunichar), len);
	} else if (ic->candidate_string->len > 0) {
	    g_array_set_size(ic->candidate_string, 0);
	}

	g_array_insert_vals(ic->candidate_string, 0, buf + n, len);
	utf8 = g_ucs4_to_utf8((const gunichar*)ic->candidate_string->data,
			      len, NULL, NULL, NULL);
	str = g_utf8_normalize(utf8, -1, G_NORMALIZE_DEFAULT_COMPOSE);
	g_free(utf8);
    }

    return str;
}

static void
popup_candidate_window (GtkIMContextHangul *hcontext)
{
  char* key;
  HanjaList* list;

  if (hcontext->candidate != NULL)
    {
      close_candidate_window(hcontext);
    }

  if (hanja_table == NULL)
      hanja_table = hanja_table_load(NULL);

  key = im_hangul_get_candidate_string(hcontext);
  list = hanja_table_match_suffix(hanja_table, key);
  if (list != NULL) {
      hcontext->candidate = candidate_new (key,
					   9,
					   list,
					   hcontext->client_window,
					   &hcontext->cursor,
					   hcontext);
  }
  g_free(key);
}

static void
close_candidate_window (GtkIMContextHangul *hic)
{
    if (hic->candidate_string != NULL && hic->candidate_string->len > 0)
	g_array_set_size(hic->candidate_string, 0);
    candidate_delete(hic->candidate);
    hic->candidate = NULL;
}

static gint
im_hangul_key_snooper(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  if (current_focused_ic != NULL) {
    /* Some keys like return, tab, ':' is usually used for auto completion or
     * commiting some changes. Some application programmers make the program
     * catch the key before the im module getting the key and check that it is 
     * return or tab or so. Then the program get the string from the text entry
     * or textview, so there is no chance for im module to commit the current
     * string. So in this case, we catch it first and process the filter
     * function of the input context. Then mostly imhangul will work fine,
     * I think :) */
    return im_hangul_ic_filter_keypress(current_focused_ic, event);
  }

  return FALSE;
}

void
im_hangul_init(void)
{
  im_hangul_config_parser();

  /* install gtk key snooper
   * this is work around code for the problem:
   *   http://bugzilla.gnome.org/show_bug.cgi?id=62948
   * I finally decided to install key snooper and catch the keys before the
   * widget getting it. */
  snooper_handler_id = gtk_key_snooper_install(im_hangul_key_snooper, NULL);
}

void
im_hangul_finalize (void)
{
  GSList *item;

  /* remove gtk key snooper */
  if (snooper_handler_id > 0) {
    gtk_key_snooper_remove(snooper_handler_id);
    snooper_handler_id = 0;
  }

  /* remove toplevel info */
  for (item = toplevels; item != NULL; item = g_slist_next(item)) {
    toplevel_delete((Toplevel*)item->data);
  }
  g_slist_free(toplevels);
  toplevels = NULL;
}

/* candidate window */
enum {
  COLUMN_INDEX,
  COLUMN_KEY,
  COLUMN_VALUE,
  COLUMN_COMMENT,
  NO_OF_COLUMNS
};

static void
candidate_on_row_activated(GtkWidget *widget,
			   GtkTreePath *path,
			   GtkTreeViewColumn *column,
			   Candidate *candidate)
{
  if (path != NULL)
    {
      int *indices;
      const Hanja* hanja;
      GtkIMContextHangul *hcontext = candidate->hangul_context;

      indices = gtk_tree_path_get_indices(path);
      candidate->current = candidate->first + indices[0];
      hanja = candidate_get_current(candidate);
      im_hangul_candidate_commit(hcontext, candidate->key, hanja);
    }
}

static void
candidate_on_cursor_changed(GtkWidget *widget,
			    Candidate *candidate)
{
  GtkTreePath *path;

  gtk_tree_view_get_cursor(GTK_TREE_VIEW(widget), &path, NULL);
  if (path != NULL)
    {
      int *indices;
      indices = gtk_tree_path_get_indices(path);
      candidate->current = candidate->first + indices[0];
      gtk_tree_path_free(path);
    }
}

static gboolean
candidate_on_scroll(GtkWidget *widget,
		    GdkEventScroll *event,
		    gpointer data)
{
  Candidate *candidate;

  if (data == NULL)
    return FALSE;

  candidate = (Candidate*)data;
  switch (event->direction) {
    case GDK_SCROLL_UP:
      candidate_prev_page(candidate);
      break;
    case GDK_SCROLL_DOWN:
      candidate_next_page(candidate);
      break;
    default:
      return FALSE;
  }
  return TRUE;
}

static gboolean
candidate_on_key_press(GtkWidget *widget,
		       GdkEventKey *event,
		       gpointer data)
{
  Candidate *candidate;
  const Hanja* hanja = NULL;

  if (data == NULL)
    return FALSE;

  candidate = (Candidate*)data;
  switch (event->keyval) {
    case GDK_Return:
    case GDK_KP_Enter:
      hanja = candidate_get_current(candidate);
      break;
    case GDK_Left:
    case GDK_h:
    case GDK_Page_Up:
      candidate_prev_page(candidate);
      break;
    case GDK_Right:
    case GDK_l:
    case GDK_Page_Down:
      candidate_next_page(candidate);
      break;
    case GDK_Up:
    case GDK_k:
    case GDK_BackSpace:
    case GDK_KP_Subtract:
      candidate_prev(candidate);
      break;
    case GDK_Down:
    case GDK_j:
    case GDK_space:
    case GDK_KP_Add:
    case GDK_KP_Tab:
      candidate_next(candidate);
      break;
    case GDK_Escape:
      close_candidate_window(candidate->hangul_context);
      break;
    case GDK_0:
      hanja = candidate_get_nth(candidate, 9);
      break;
    case GDK_1:
    case GDK_2:
    case GDK_3:
    case GDK_4:
    case GDK_5:
    case GDK_6:
    case GDK_7:
    case GDK_8:
    case GDK_9:
      hanja = candidate_get_nth(candidate, event->keyval - GDK_1);
      break;
    default:
      return FALSE;
  }

  if (hanja != NULL)
    im_hangul_candidate_commit(candidate->hangul_context,
			       candidate->key, hanja);
  return TRUE;
}

static void
candidate_on_expose (GtkWidget *widget,
		     GdkEventExpose *event,
		     gpointer data)
{
  GtkStyle *style;
  GtkAllocation alloc;

  style = gtk_widget_get_style(widget);
  alloc = GTK_WIDGET(widget)->allocation;
  gdk_draw_rectangle(widget->window, style->black_gc,
		     FALSE,
		     0, 0, alloc.width - 1, alloc.height - 1);
}

static void
candidate_update_cursor(Candidate *candidate)
{
  GtkTreePath *path;

  if (candidate->treeview == NULL)
    return;

  path = gtk_tree_path_new_from_indices(candidate->current - candidate->first,                                          -1);
  gtk_tree_view_set_cursor(GTK_TREE_VIEW(candidate->treeview),
			   path, NULL, FALSE);
  gtk_tree_path_free(path);
}

static void
candidate_set_window_position (Candidate *candidate)
{
    gint width = 0, height = 0;
    gint absx = 0, absy = 0;
    gint root_w, root_h, cand_w, cand_h;
    GtkRequisition requisition;

    if (candidate->parent == NULL)
      return;

    gdk_window_get_origin (GDK_WINDOW(candidate->parent), &absx, &absy);
    gdk_drawable_get_size (GDK_DRAWABLE(candidate->parent), &width, &height);

    root_w = gdk_screen_width();
    root_h = gdk_screen_height();

    gtk_widget_size_request(GTK_WIDGET(candidate->window), &requisition);
    cand_w = requisition.width;
    cand_h = requisition.height;

    absx += candidate->cursor.x;
    absy += (candidate->cursor.height < 0)? 
	    height : candidate->cursor.y + candidate->cursor.height;

    if (absy + cand_h > root_h)
      absy = root_h - cand_h;
    if (absx + cand_w > root_w)
      absx = root_w - cand_w;
    gtk_window_move(GTK_WINDOW(candidate->window), absx, absy);
}

static void
candidate_update_list(Candidate *candidate)
{
  int i;
  GtkTreeIter iter;

  gtk_list_store_clear(candidate->store);
  for (i = 0;
       i < candidate->n_per_page && candidate->first + i < candidate->n;
       i++)
    {
      const char* value;
      const char* comment;
      const Hanja* hanja;
      
      hanja = hanja_list_get_nth(candidate->list, candidate->first + i);
      value = hanja_get_value(hanja);
      comment = hanja_get_comment(hanja);

      gtk_list_store_append(candidate->store, &iter);
      gtk_list_store_set(candidate->store, &iter,
	      COLUMN_INDEX, (i + 1) % 10,
	      COLUMN_VALUE, value,
	      COLUMN_COMMENT, comment,
	      -1);
    }
  candidate_set_window_position (candidate);
}

static void
candidate_on_realize(GtkWidget* widget, gpointer data)
{
    gtk_widget_modify_fg  (widget, GTK_STATE_ACTIVE,
		    &widget->style->fg[GTK_STATE_SELECTED]);
    gtk_widget_modify_bg  (widget, GTK_STATE_ACTIVE,
		    &widget->style->bg[GTK_STATE_SELECTED]);
    gtk_widget_modify_text(widget, GTK_STATE_ACTIVE,
		    &widget->style->text[GTK_STATE_SELECTED]);
    gtk_widget_modify_base(widget, GTK_STATE_ACTIVE,
		    &widget->style->base[GTK_STATE_SELECTED]);
}

static void
candidate_create_window(Candidate *candidate)
{
  GtkWidget *frame;
  GtkWidget *treeview;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  candidate->window = gtk_window_new(GTK_WINDOW_POPUP);

  candidate_update_list(candidate);

  frame = gtk_frame_new(candidate->key);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
  gtk_container_add(GTK_CONTAINER(candidate->window), frame);

  treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(candidate->store));
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
  gtk_widget_set_name(GTK_WIDGET(treeview), "imhangul_candidate");
  gtk_container_add(GTK_CONTAINER(frame), treeview);
  candidate->treeview = treeview;
  g_object_unref(candidate->store);

  /* number column */
  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes("No",
						    renderer,
						    "text", COLUMN_INDEX,
						    NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

  /* character column */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "scale", 2.0, NULL);
  column = gtk_tree_view_column_new_with_attributes("Character",
						    renderer,
						    "text", COLUMN_VALUE,
						    NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

  /* comment column */
  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes("Comment",
						    renderer,
						    "text", COLUMN_COMMENT,
						    NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

  candidate_update_cursor(candidate);

  /* 테마에 따라서 active 색상이 normal 색상과 같은 것들이 있다.
   * 그런 경우에 active 상태의 row가 표시가 안되므로 
   * focus를 받지 못하는 candidat window에서는 구분이 안된다.
   * 이를 피하기 위해서 widget의 스타일을 수정해서 
   * activate와 select를 같은 색으로 그려주도록 한다. */
  g_signal_connect_after(G_OBJECT(treeview), "realize",
                         G_CALLBACK(candidate_on_realize), NULL);
  g_signal_connect(G_OBJECT(treeview), "row-activated",
		   G_CALLBACK(candidate_on_row_activated), candidate);
  g_signal_connect(G_OBJECT(treeview), "cursor-changed",
		   G_CALLBACK(candidate_on_cursor_changed), candidate);
  g_signal_connect(G_OBJECT(candidate->window), "scroll-event",
		   G_CALLBACK(candidate_on_scroll), candidate);
  g_signal_connect(G_OBJECT(candidate->window), "key-press-event",
		   G_CALLBACK(candidate_on_key_press), candidate);
  g_signal_connect_after(G_OBJECT(candidate->window), "expose-event",
			 G_CALLBACK(candidate_on_expose), candidate);
  g_signal_connect_swapped(G_OBJECT(candidate->window), "realize",
			   G_CALLBACK(candidate_set_window_position),
			   candidate);

  gtk_widget_show_all(candidate->window);
  gtk_grab_add(candidate->window);
}

static Candidate*
candidate_new(char *key,
	      int n_per_page,
	      HanjaList *list,
	      GdkWindow *parent,
	      GdkRectangle *area,
	      GtkIMContextHangul *hcontext)
{
  Candidate *candidate;

  candidate = (Candidate*)g_malloc(sizeof(Candidate));
  candidate->key = g_strdup(key);
  candidate->first = 0;
  candidate->current = 0;
  candidate->n_per_page = n_per_page;
  candidate->list = list;
  candidate->n = hanja_list_get_size(list);
  candidate->parent = parent;
  candidate->cursor = *area;
  candidate->store = NULL;
  candidate->treeview = NULL;
  candidate->hangul_context = hcontext;

  if (n_per_page == 0)
    candidate->n_per_page = candidate->n;

  candidate->store = gtk_list_store_new(NO_OF_COLUMNS,
		    G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  candidate_create_window(candidate);

  return candidate;
}

static void
candidate_prev(Candidate *candidate)
{
  if (candidate == NULL)
    return;

  if (candidate->current > 0)
    candidate->current--;

  if (candidate->current < candidate->first)
    {
      candidate->first -= candidate->n_per_page;
      candidate_update_list(candidate);
    }
  candidate_update_cursor(candidate);
}

static void
candidate_next(Candidate *candidate)
{
  if (candidate == NULL)
    return;

  if (candidate->current < candidate->n - 1)
    candidate->current++;

  if (candidate->current >= candidate->first + candidate->n_per_page)
    {
      candidate->first += candidate->n_per_page;
      candidate_update_list(candidate);
    }
  candidate_update_cursor(candidate);
}

static void
candidate_prev_page(Candidate *candidate)
{
  if (candidate == NULL)
    return;

  if (candidate->first - candidate->n_per_page >= 0)
    {
      candidate->current -= candidate->n_per_page;
      if (candidate->current < 0)
	candidate->current = 0;
      candidate->first -= candidate->n_per_page;
      candidate_update_list(candidate);
    }
  candidate_update_cursor(candidate);
}

static void
candidate_next_page(Candidate *candidate)
{
  if (candidate == NULL)
    return;

  if (candidate->first + candidate->n_per_page < candidate->n)
    {
      candidate->current += candidate->n_per_page;
      if (candidate->current > candidate->n - 1)
	candidate->current = candidate->n - 1;
      candidate->first += candidate->n_per_page;
      candidate_update_list(candidate);
    }
  candidate_update_cursor(candidate);
}

static const Hanja*
candidate_get_current(Candidate *candidate)
{
  if (candidate == NULL)
    return 0;

  return hanja_list_get_nth(candidate->list, candidate->current);
}

static const Hanja*
candidate_get_nth(Candidate *candidate, int index_)
{
  if (candidate == NULL)
    return 0;

  index_ += candidate->first;
  if (index_ < 0 || index_ >= candidate->n)
    return 0;

  return hanja_list_get_nth(candidate->list, index_);
}

static void
candidate_delete(Candidate *candidate)
{
  if (candidate == NULL)
    return;

  gtk_grab_remove(candidate->window);
  gtk_widget_destroy(candidate->window);
  hanja_list_delete(candidate->list);
  g_free(candidate->key);
  g_free(candidate);
}

static size_t
ucschar_strlen(const ucschar* s)
{
    const ucschar* p = s;
    while (*p != 0)
	p++;
    return p - s;
}

/* vim: set cindent sw=4 sts=4 ts=8 : */
