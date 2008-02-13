/* Murrine theme engine
 * Copyright (C) 2007 Andrea Cimitan
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
 *
 */

#include "murrine_style.h"
#include "murrine_rc_style.h"

#include "animation.h"

static void murrine_rc_style_init (MurrineRcStyle *style);
#ifdef HAVE_ANIMATION
static void murrine_rc_style_finalize (GObject *object);
#endif
static void murrine_rc_style_class_init (MurrineRcStyleClass *klass);
static GtkStyle *murrine_rc_style_create_style (GtkRcStyle *rc_style);
static guint murrine_rc_style_parse (GtkRcStyle  *rc_style,
                                     GtkSettings *settings,
                                     GScanner    *scanner);
static void murrine_rc_style_merge (GtkRcStyle *dest,
                                    GtkRcStyle *src);

static GtkRcStyleClass *parent_class;

GType murrine_type_rc_style = 0;

enum
{
	TOKEN_ANIMATION = G_TOKEN_LAST + 1,
	TOKEN_COLORIZE_SCROLLBAR,
	TOKEN_CONTRAST,
	TOKEN_GLAZESTYLE,
	TOKEN_GRADIENT_STOP_1,
	TOKEN_GRADIENT_STOP_2,
	TOKEN_GRADIENT_STOP_3,
	TOKEN_GRADIENT_STOP_4,
	TOKEN_GRADIENTS,
	TOKEN_HIGHLIGHT_RATIO,
	TOKEN_INNERBORDER_RATIO,
	TOKEN_LISTVIEWHEADERSTYLE,
	TOKEN_LISTVIEWSTYLE,
	TOKEN_MENUBARITEMSTYLE,
	TOKEN_MENUBARSTYLE,
	TOKEN_MENUITEMSTYLE,
	TOKEN_MENUSTYLE,
	TOKEN_RGBA,
	TOKEN_ROUNDNESS,
	TOKEN_SCROLLBAR_COLOR,
	TOKEN_SCROLLBARSTYLE,
	TOKEN_SLIDERSTYLE,
	TOKEN_STEPPERSTYLE,
	TOKEN_STYLE,
	TOKEN_TOOLBARSTYLE,

	TOKEN_CANDIDO,
	TOKEN_CLEARLOOKS,
	TOKEN_MIST,
	TOKEN_MURRINE,
	TOKEN_NODOKA,

	TOKEN_TRUE,
	TOKEN_FALSE,

	/* stuff to ignore */
	TOKEN_HILIGHT_RATIO,
	TOKEN_SQUAREDSTYLE
};

static struct
{
	const gchar *name;
	guint        token;
}
theme_symbols[] =
{
	{ "animation",           TOKEN_ANIMATION },
	{ "colorize_scrollbar",  TOKEN_COLORIZE_SCROLLBAR },
	{ "contrast",            TOKEN_CONTRAST },
	{ "glazestyle",          TOKEN_GLAZESTYLE },
	{ "gradient_stop_1",     TOKEN_GRADIENT_STOP_1 },
	{ "gradient_stop_2",     TOKEN_GRADIENT_STOP_2 },
	{ "gradient_stop_3",     TOKEN_GRADIENT_STOP_3 },
	{ "gradient_stop_4",     TOKEN_GRADIENT_STOP_4 },
	{ "gradients",           TOKEN_GRADIENTS },
	{ "highlight_ratio",     TOKEN_HIGHLIGHT_RATIO },
	{ "innerborder_ratio",   TOKEN_INNERBORDER_RATIO },
	{ "listviewheaderstyle", TOKEN_LISTVIEWHEADERSTYLE },
	{ "listviewstyle",       TOKEN_LISTVIEWSTYLE },
	{ "menubaritemstyle",    TOKEN_MENUBARITEMSTYLE },
	{ "menubarstyle",        TOKEN_MENUBARSTYLE },
	{ "menuitemstyle",       TOKEN_MENUITEMSTYLE },
	{ "menustyle",           TOKEN_MENUSTYLE },
	{ "rgba",                TOKEN_RGBA },
	{ "roundness",           TOKEN_ROUNDNESS },
	{ "scrollbar_color",     TOKEN_SCROLLBAR_COLOR },
	{ "scrollbarstyle",      TOKEN_SCROLLBARSTYLE },
	{ "sliderstyle",         TOKEN_SLIDERSTYLE },
	{ "stepperstyle",        TOKEN_STEPPERSTYLE },
	{ "style",               TOKEN_STYLE },
	{ "toolbarstyle",        TOKEN_TOOLBARSTYLE },

	{ "CANDIDO",             TOKEN_CANDIDO },
	{ "CLEARLOOKS",          TOKEN_CLEARLOOKS },
	{ "MIST",                TOKEN_MIST },
	{ "MURRINE",             TOKEN_MURRINE },
	{ "NODOKA",              TOKEN_NODOKA },

	{ "TRUE",                TOKEN_TRUE },
	{ "FALSE",               TOKEN_FALSE },

	/* stuff to ignore */
	{ "hilight_ratio",       TOKEN_HILIGHT_RATIO },
	{ "squaredstyle",        TOKEN_SQUAREDSTYLE }
};

void
murrine_rc_style_register_type (GTypeModule *module)
{
	static const GTypeInfo object_info =
	{
		sizeof (MurrineRcStyleClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) murrine_rc_style_class_init,
		NULL, /* class_finalize */
		NULL, /* class_data */
		sizeof (MurrineRcStyle),
		0, /* n_preallocs */
		(GInstanceInitFunc) murrine_rc_style_init,
		NULL
	};

	murrine_type_rc_style = g_type_module_register_type (module,
	                                                     GTK_TYPE_RC_STYLE,
	                                                     "MurrineRcStyle",
	                                                     &object_info, 0);
}

static void
murrine_rc_style_init (MurrineRcStyle *murrine_rc)
{
	murrine_rc->flags = 0;

	murrine_rc->animation = FALSE;
	murrine_rc->colorize_scrollbar = TRUE;
	murrine_rc->contrast = 1.0;
	murrine_rc->glazestyle = 1;
	murrine_rc->gradient_stop_1 = 1.0;
	murrine_rc->gradient_stop_2 = 1.0;
	murrine_rc->gradient_stop_3 = 1.0;
	murrine_rc->gradient_stop_4 = 1.0;
	murrine_rc->gradients = TRUE;
	murrine_rc->has_scrollbar_color = FALSE;
	murrine_rc->has_gradient_stop = FALSE;
	murrine_rc->highlight_ratio = 1.1;
	murrine_rc->innerborder_ratio = 1.1;
	murrine_rc->listviewheaderstyle = 1;
	murrine_rc->listviewstyle = 0;
	murrine_rc->menubaritemstyle = 0;
	murrine_rc->menubarstyle = 0;
	murrine_rc->menuitemstyle = 1;
	murrine_rc->menustyle = 1;
	murrine_rc->rgba = TRUE;
	murrine_rc->roundness = 1;
	murrine_rc->scrollbarstyle = 0;
	murrine_rc->sliderstyle = 0;
	murrine_rc->stepperstyle = 0;
	murrine_rc->style = MRN_STYLE_MURRINE;
	murrine_rc->toolbarstyle = 0;
}

#ifdef HAVE_ANIMATION
static void
murrine_rc_style_finalize (GObject *object)
{
	/* cleanup all the animation stuff */
	murrine_animation_cleanup ();

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL)
		G_OBJECT_CLASS (parent_class)->finalize(object);
}
#endif


static void
murrine_rc_style_class_init (MurrineRcStyleClass *klass)
{
	GtkRcStyleClass *rc_style_class = GTK_RC_STYLE_CLASS (klass);
#ifdef HAVE_ANIMATION
	GObjectClass    *g_object_class = G_OBJECT_CLASS (klass);
#endif

	parent_class = g_type_class_peek_parent (klass);

	rc_style_class->parse = murrine_rc_style_parse;
	rc_style_class->create_style = murrine_rc_style_create_style;
	rc_style_class->merge = murrine_rc_style_merge;

#ifdef HAVE_ANIMATION
	g_object_class->finalize = murrine_rc_style_finalize;
#endif
}

static guint
theme_parse_boolean (GtkSettings *settings,
                     GScanner     *scanner,
                     gboolean *retval)
{
	guint token;

	/* Skip 'ANIMATION' */
	token = g_scanner_get_next_token(scanner);

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
		return G_TOKEN_EQUAL_SIGN;

	token = g_scanner_get_next_token(scanner);
	if (token == TOKEN_TRUE)
		*retval = TRUE;
	else if (token == TOKEN_FALSE)
		*retval = FALSE;
	else
		return TOKEN_TRUE;

	return G_TOKEN_NONE;
}

static guint
theme_parse_color (GtkSettings  *settings,
                   GScanner     *scanner,
                   GdkColor     *color)
{
	guint token;

	/* Skip 'blah_color' */
	token = g_scanner_get_next_token(scanner);

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
		return G_TOKEN_EQUAL_SIGN;

	return gtk_rc_parse_color (scanner, color);
}

static guint
theme_parse_ratio (GtkSettings  *settings,
                   GScanner     *scanner,
                   double       *ratio)
{
	guint token;

	/* Skip 'ratio' */
	token = g_scanner_get_next_token(scanner);

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
		return G_TOKEN_EQUAL_SIGN;

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_FLOAT)
		return G_TOKEN_FLOAT;

	*ratio = scanner->value.v_float;

	return G_TOKEN_NONE;
}

static guint
theme_parse_int (GtkSettings  *settings,
                 GScanner     *scanner,
                 guint8       *style)
{
	guint token;

	/* Skip '*style' */
	token = g_scanner_get_next_token(scanner);

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
	return G_TOKEN_EQUAL_SIGN;

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_INT)
		return G_TOKEN_INT;

	*style = scanner->value.v_int;

	return G_TOKEN_NONE;
}

static guint
theme_parse_style (GtkSettings   *settings,
                   GScanner      *scanner,
                   MurrineStyles *style)
{
	guint token;

	g_assert (MRN_NUM_STYLES == MRN_STYLE_CLEARLOOKS + 1); /* so that people don't forget ;-) */

	/* Skip 'style' */
	token = g_scanner_get_next_token (scanner);

	token = g_scanner_get_next_token (scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
	   return G_TOKEN_EQUAL_SIGN;

	token = g_scanner_get_next_token (scanner);

	switch (token)
	{
		case TOKEN_MURRINE:
		   *style = MRN_STYLE_MURRINE;
		   break;
		case TOKEN_NODOKA:
		   *style = MRN_STYLE_NODOKA;
		   break;
		case TOKEN_MIST:
		   *style = MRN_STYLE_MIST;
		   break;
		case TOKEN_CANDIDO:
		   *style = MRN_STYLE_CANDIDO;
		   break;
		case TOKEN_CLEARLOOKS:
		   *style = MRN_STYLE_CLEARLOOKS;
		   break;
		default:
		   return TOKEN_MURRINE;
	}

	return G_TOKEN_NONE;
}

static guint
murrine_gtk2_rc_parse_dummy (GtkSettings      *settings,
                             GScanner         *scanner,
                             gchar            *name)
{
	guint token;

	/* Skip option */
	token = g_scanner_get_next_token (scanner);

	/* print a warning. Isn't there a way to get the string from the scanner? */
	g_scanner_warn (scanner, "Murrine configuration option \"%s\" is not supported and will be ignored.", name);

	/* equal sign */
	token = g_scanner_get_next_token (scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
		return G_TOKEN_EQUAL_SIGN;

	/* eat whatever comes next */
	token = g_scanner_get_next_token (scanner);

	return G_TOKEN_NONE;
}

static guint
murrine_rc_style_parse (GtkRcStyle *rc_style,
                        GtkSettings  *settings,
                        GScanner   *scanner)
{
	static GQuark scope_id = 0;
	MurrineRcStyle *murrine_style = MURRINE_RC_STYLE (rc_style);

	guint old_scope;
	guint token;
	guint i;

	/* Set up a new scope in this scanner. */

	if (!scope_id)
		scope_id = g_quark_from_string("murrine_theme_engine");

	/* If we bail out due to errors, we *don't* reset the scope, so the
	* error messaging code can make sense of our tokens.
	*/
	old_scope = g_scanner_set_scope(scanner, scope_id);

	/* Now check if we already added our symbols to this scope
	* (in some previous call to murrine_rc_style_parse for the
	* same scanner.
	*/

	if (!g_scanner_lookup_symbol(scanner, theme_symbols[0].name))
	{
		g_scanner_freeze_symbol_table(scanner);
		for (i = 0; i < G_N_ELEMENTS (theme_symbols); i++)
			g_scanner_scope_add_symbol(scanner, scope_id, theme_symbols[i].name, GINT_TO_POINTER(theme_symbols[i].token));
		g_scanner_thaw_symbol_table(scanner);
	}

	/* We're ready to go, now parse the top level */
	token = g_scanner_peek_next_token(scanner);
	while (token != G_TOKEN_RIGHT_CURLY)
	{
		switch (token)
		{
			case TOKEN_ANIMATION:
				token = theme_parse_boolean (settings, scanner, &murrine_style->animation);
				murrine_style->flags |= MRN_FLAG_ANIMATION;
				break;
			case TOKEN_COLORIZE_SCROLLBAR:
				token = theme_parse_boolean (settings, scanner, &murrine_style->colorize_scrollbar);
				murrine_style->flags |= MRN_FLAG_COLORIZE_SCROLLBAR;
				break;
			case TOKEN_CONTRAST:
				token = theme_parse_ratio (settings, scanner, &murrine_style->contrast);
				murrine_style->flags |= MRN_FLAG_CONTRAST;
				break;
			case TOKEN_GLAZESTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->glazestyle);
				murrine_style->flags |= MRN_FLAG_GLAZESTYLE;
				break;
			case TOKEN_GRADIENT_STOP_1:
				token = theme_parse_ratio (settings, scanner, &murrine_style->gradient_stop_1);
				murrine_style->flags |= MRN_FLAG_GRADIENT_STOP_1;
				murrine_style->has_gradient_stop = TRUE;
				break;
			case TOKEN_GRADIENT_STOP_2:
				token = theme_parse_ratio (settings, scanner, &murrine_style->gradient_stop_2);
				murrine_style->flags |= MRN_FLAG_GRADIENT_STOP_2;
				murrine_style->has_gradient_stop = TRUE;
				break;
			case TOKEN_GRADIENT_STOP_3:
				token = theme_parse_ratio (settings, scanner, &murrine_style->gradient_stop_3);
				murrine_style->flags |= MRN_FLAG_GRADIENT_STOP_3;
				murrine_style->has_gradient_stop = TRUE;
				break;
			case TOKEN_GRADIENT_STOP_4:
				token = theme_parse_ratio (settings, scanner, &murrine_style->gradient_stop_4);
				murrine_style->flags |= MRN_FLAG_GRADIENT_STOP_4;
				murrine_style->has_gradient_stop = TRUE;
				break;
			case TOKEN_GRADIENTS:
				token = theme_parse_boolean (settings, scanner, &murrine_style->gradients);
				murrine_style->flags |= MRN_FLAG_GRADIENTS;
				break;
			case TOKEN_HIGHLIGHT_RATIO:
				token = theme_parse_ratio (settings, scanner, &murrine_style->highlight_ratio);
				murrine_style->flags |= MRN_FLAG_HIGHLIGHT_RATIO;
				break;
			case TOKEN_INNERBORDER_RATIO:
				token = theme_parse_ratio (settings, scanner, &murrine_style->innerborder_ratio);
				murrine_style->flags |= MRN_FLAG_INNERBORDER_RATIO;
				break;
			case TOKEN_LISTVIEWHEADERSTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->listviewheaderstyle);
				murrine_style->flags |= MRN_FLAG_LISTVIEWHEADERSTYLE;
				break;
			case TOKEN_LISTVIEWSTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->listviewstyle);
				murrine_style->flags |= MRN_FLAG_LISTVIEWSTYLE;
				break;
			case TOKEN_MENUBARITEMSTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->menubaritemstyle);
				murrine_style->flags |= MRN_FLAG_MENUBARITEMSTYLE;
				break;
			case TOKEN_MENUBARSTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->menubarstyle);
				murrine_style->flags |= MRN_FLAG_MENUBARSTYLE;
				break;
			case TOKEN_MENUITEMSTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->menuitemstyle);
				murrine_style->flags |= MRN_FLAG_MENUITEMSTYLE;
				break;
			case TOKEN_MENUSTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->menustyle);
				murrine_style->flags |= MRN_FLAG_MENUSTYLE;
				break;
			case TOKEN_RGBA:
				token = theme_parse_boolean (settings, scanner, &murrine_style->rgba);
				murrine_style->flags |= MRN_FLAG_RGBA;
				break;
			case TOKEN_ROUNDNESS:
				token = theme_parse_int (settings, scanner, &murrine_style->roundness);
				murrine_style->flags |= MRN_FLAG_ROUNDNESS;
				break;
			case TOKEN_SCROLLBAR_COLOR:
				token = theme_parse_color (settings, scanner, &murrine_style->scrollbar_color);
				murrine_style->flags |= MRN_FLAG_SCROLLBAR_COLOR;
				murrine_style->has_scrollbar_color = TRUE;
				break;
			case TOKEN_SCROLLBARSTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->scrollbarstyle);
				murrine_style->flags |= MRN_FLAG_SCROLLBARSTYLE;
				break;
			case TOKEN_SLIDERSTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->sliderstyle);
				murrine_style->flags |= MRN_FLAG_SLIDERSTYLE;
				break;
			case TOKEN_STEPPERSTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->stepperstyle);
				murrine_style->flags |= MRN_FLAG_STEPPERSTYLE;
				break;
			case TOKEN_STYLE:
				token = theme_parse_style (settings, scanner, &murrine_style->style);
				murrine_style->flags |= MRN_FLAG_STYLE;
				break;
			case TOKEN_TOOLBARSTYLE:
				token = theme_parse_int (settings, scanner, &murrine_style->toolbarstyle);
				murrine_style->flags |= MRN_FLAG_TOOLBARSTYLE;
				break;

			/* stuff to ignore */
			case TOKEN_HILIGHT_RATIO:
				g_scanner_warn (scanner, "Murrine configuration option \"hilight_ratio\" will be deprecated in future releases. Please update this theme to get rid of this warning.", "hilight_ratio");
				double hilight_ratio;
				token = theme_parse_ratio (settings, scanner, &hilight_ratio);
				murrine_style->highlight_ratio = hilight_ratio/0.909090;
				murrine_style->flags |= MRN_FLAG_HIGHLIGHT_RATIO;
				break;
			case TOKEN_SQUAREDSTYLE:
				token = murrine_gtk2_rc_parse_dummy (settings, scanner, "squaredstyle");
				break;

			default:
				g_scanner_get_next_token(scanner);
				token = G_TOKEN_RIGHT_CURLY;
				break;
		}

		if (token != G_TOKEN_NONE)
			return token;

		token = g_scanner_peek_next_token(scanner);
	}

	g_scanner_get_next_token(scanner);

	g_scanner_set_scope(scanner, old_scope);

	return G_TOKEN_NONE;
}

static void
murrine_rc_style_merge (GtkRcStyle *dest,
                        GtkRcStyle *src)
{
	MurrineRcStyle *dest_w, *src_w;
	MurrineRcFlags flags;

	parent_class->merge (dest, src);

	if (!MURRINE_IS_RC_STYLE (src))
		return;

	src_w = MURRINE_RC_STYLE (src);
	dest_w = MURRINE_RC_STYLE (dest);

	flags = (~dest_w->flags) & src_w->flags;

	if (flags & MRN_FLAG_ANIMATION)
		dest_w->animation = src_w->animation;
	if (flags & MRN_FLAG_COLORIZE_SCROLLBAR)
		dest_w->colorize_scrollbar = src_w->colorize_scrollbar;
	if (flags & MRN_FLAG_CONTRAST)
		dest_w->contrast = src_w->contrast;
	if (flags & MRN_FLAG_GLAZESTYLE)
		dest_w->glazestyle = src_w->glazestyle;
	if (flags & MRN_FLAG_GRADIENT_STOP_1)
	{
		dest_w->has_gradient_stop = TRUE;
		dest_w->gradient_stop_1 = src_w->gradient_stop_1;
	}
	if (flags & MRN_FLAG_GRADIENT_STOP_2)
	{
		dest_w->has_gradient_stop = TRUE;
		dest_w->gradient_stop_2 = src_w->gradient_stop_2;
	}
	if (flags & MRN_FLAG_GRADIENT_STOP_3)
	{
		dest_w->has_gradient_stop = TRUE;
		dest_w->gradient_stop_3 = src_w->gradient_stop_3;
	}
	if (flags & MRN_FLAG_GRADIENT_STOP_4)
	{
		dest_w->has_gradient_stop = TRUE;
		dest_w->gradient_stop_4 = src_w->gradient_stop_4;
	}
	if (flags & MRN_FLAG_GRADIENTS)
		dest_w->gradients = src_w->gradients;
	if (flags & MRN_FLAG_HIGHLIGHT_RATIO)
		dest_w->highlight_ratio = src_w->highlight_ratio;
	if (flags & MRN_FLAG_INNERBORDER_RATIO)
		dest_w->innerborder_ratio = src_w->innerborder_ratio;
	if (flags & MRN_FLAG_LISTVIEWHEADERSTYLE)
		dest_w->listviewheaderstyle = src_w->listviewheaderstyle;
	if (flags & MRN_FLAG_LISTVIEWSTYLE)
		dest_w->listviewstyle = src_w->listviewstyle;
	if (flags & MRN_FLAG_MENUBARITEMSTYLE)
		dest_w->menubaritemstyle = src_w->menubaritemstyle;
	if (flags & MRN_FLAG_MENUBARSTYLE)
		dest_w->menubarstyle = src_w->menubarstyle;
	if (flags & MRN_FLAG_MENUITEMSTYLE)
		dest_w->menuitemstyle = src_w->menuitemstyle;
	if (flags & MRN_FLAG_MENUSTYLE)
		dest_w->menustyle = src_w->menustyle;
	if (flags & MRN_FLAG_RGBA)
		dest_w->rgba = src_w->rgba;
	if (flags & MRN_FLAG_ROUNDNESS)
		dest_w->roundness = src_w->roundness;
	if (flags & MRN_FLAG_SCROLLBAR_COLOR)
	{
		dest_w->has_scrollbar_color = TRUE;
		dest_w->scrollbar_color = src_w->scrollbar_color;
	}
	if (flags & MRN_FLAG_SCROLLBARSTYLE)
		dest_w->scrollbarstyle = src_w->scrollbarstyle;
	if (flags & MRN_FLAG_SLIDERSTYLE)
		dest_w->sliderstyle = src_w->sliderstyle;
	if (flags & MRN_FLAG_STEPPERSTYLE)
		dest_w->stepperstyle = src_w->stepperstyle;
	if (flags & MRN_FLAG_STYLE)
		dest_w->style = src_w->style;
	if (flags & MRN_FLAG_TOOLBARSTYLE)
		dest_w->toolbarstyle = src_w->toolbarstyle;

	dest_w->flags |= src_w->flags;
}

/* Create an empty style suitable to this RC style
 */
static GtkStyle *
murrine_rc_style_create_style (GtkRcStyle *rc_style)
{
	return GTK_STYLE (g_object_new (MURRINE_TYPE_STYLE, NULL));
}