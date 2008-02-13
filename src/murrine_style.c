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

#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>

#include "murrine_style.h"
#include "murrine_rc_style.h"
#include "murrine_draw.h"
#include "support.h"
#include "cairo-support.h"

/* #define DEBUG 1 */

#define SCALE_SIZE 5

#define DETAIL(xx) ((detail) && (!strcmp(xx, detail)))
#define COMPARE_COLORS(a,b) (a.red == b.red && a.green == b.green && a.blue == b.blue)

#define DRAW_ARGS GtkStyle      *style, \
                  GdkWindow     *window, \
                  GtkStateType  state_type, \
                  GtkShadowType shadow_type, \
                  GdkRectangle  *area, \
                  GtkWidget     *widget, \
                  const gchar   *detail, \
                  gint          x, \
                  gint          y, \
                  gint          width, \
                  gint          height

#define CHECK_ARGS \
	g_return_if_fail (window != NULL); \
	g_return_if_fail (style != NULL);

#define SANITIZE_SIZE \
	g_return_if_fail (width  >= -1); \
	g_return_if_fail (height >= -1); \
	\
	if ((width == -1) && (height == -1)) \
		gdk_drawable_get_size (window, &width, &height); \
	else if (width == -1) \
		gdk_drawable_get_size (window, &width, NULL); \
	else if (height == -1) \
		gdk_drawable_get_size (window, NULL, &height);

#ifdef HAVE_ANIMATION
#include "animation.h"
#endif

#define STYLE_FUNCTION(function) (murrine_style_class->style_functions[params.drawstyle].function)

static MurrineStyleClass *murrine_style_class;
static GtkStyleClass *murrine_parent_class;

static cairo_t *
murrine_begin_paint (GdkDrawable *window, GdkRectangle *area)
{
	cairo_t *cr;

	g_return_val_if_fail (window != NULL, NULL);

	cr = (cairo_t*) gdk_cairo_create (window);
	cairo_set_line_width (cr, 1.0);

	if (area)
	{
		cairo_rectangle (cr, area->x, area->y, area->width, area->height);
		cairo_clip_preserve (cr);
		cairo_new_path (cr);
	}

	return cr;
}

static
boolean murrine_widget_is_rgba (GtkWidget *widget)
{
	GdkScreen *screen = gtk_widget_get_screen (widget);
	boolean use_rgba = FALSE;

	if (gdk_screen_is_composited(screen) && gdk_screen_get_rgba_colormap (screen))
		use_rgba = (gtk_widget_get_colormap (widget) ==
		            gdk_screen_get_rgba_colormap (screen));

	return use_rgba;
}

static void
murrine_set_widget_parameters (const GtkWidget  *widget,
                               const GtkStyle   *style,
                               GtkStateType     state_type,
                               WidgetParameters *params)
{
	if (widget && MRN_IS_ENTRY (widget))
		state_type = GTK_WIDGET_STATE (widget);

	MurrineStyle *murrine_style = MURRINE_STYLE (style);

	params->active     = (state_type == GTK_STATE_ACTIVE);
	params->prelight   = (state_type == GTK_STATE_PRELIGHT);
	params->disabled   = (state_type == GTK_STATE_INSENSITIVE);
	params->state_type = (MurrineStateType)state_type;
	params->corners    = MRN_CORNER_ALL;
	params->ltr        = murrine_widget_is_ltr ((GtkWidget*)widget);
	params->focus      = widget && GTK_WIDGET_HAS_FOCUS (widget);
	params->is_default = widget && GTK_WIDGET_HAS_DEFAULT (widget);

	if (!params->active && widget && MRN_IS_TOGGLE_BUTTON (widget))
		params->active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	params->xthickness = style->xthickness;
	params->ythickness = style->ythickness;

	params->innerborder_ratio = murrine_style->innerborder_ratio;
	params->glazestyle        = murrine_style->glazestyle;
	params->roundness         = murrine_style->roundness;
	params->highlight_ratio   = murrine_style->highlight_ratio;

	MurrineGradients mrn_gradient;
	mrn_gradient.has_gradient_stop = murrine_style->has_gradient_stop;
	mrn_gradient.gradient_stop_1 = murrine_style->gradient_stop_1;
	mrn_gradient.gradient_stop_2 = murrine_style->gradient_stop_2;
	mrn_gradient.gradient_stop_3 = murrine_style->gradient_stop_3;
	mrn_gradient.gradient_stop_4 = murrine_style->gradient_stop_4;
	mrn_gradient.gradients = murrine_style->gradients;
	mrn_gradient.use_rgba = (murrine_widget_is_rgba ((GtkWidget*) widget) && murrine_style->rgba);
	mrn_gradient.rgba_opacity = GRADIENT_OPACITY;

	MurrineDrawStyles drawstyle = MRN_DRAW_STYLE_MURRINE;
	if (mrn_gradient.use_rgba)
	{
		//params->highlight_ratio = get_increased_ratio (params->highlight_ratio, 1.4);
		drawstyle = MRN_DRAW_STYLE_RGBA;
	}
	params->mrn_gradient = mrn_gradient;
	params->drawstyle = drawstyle;
	params->style_functions = &(murrine_style_class->style_functions[drawstyle]);

	/* I want to avoid to have to do this. I need it for GtkEntry, unless I
	   find out why it doesn't behave the way I expect it to. */
	params->parentbg = MURRINE_STYLE (style)->colors.bg[state_type];
	murrine_get_parent_bg (widget, &params->parentbg);
}

static void
murrine_style_draw_flat_box (DRAW_ARGS)
{
	//printf( "draw_flat_box: %s %s\n", detail, G_OBJECT_TYPE_NAME (widget));
	if (detail &&
	    state_type == GTK_STATE_SELECTED && (
	    !strncmp ("cell_even", detail, 9) ||
	    !strncmp ("cell_odd", detail, 8)))
	{
		MurrineStyle  *murrine_style = MURRINE_STYLE (style);
		MurrineColors *colors = &murrine_style->colors;
		cairo_t       *cr;

		CHECK_ARGS
		SANITIZE_SIZE

		cr = murrine_begin_paint (window, area);

		WidgetParameters params;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		STYLE_FUNCTION(draw_selected_cell) (cr, colors, &params, x, y, width, height);

		cairo_destroy (cr);
	}
	else if (DETAIL ("tooltip"))
	{
		MurrineStyle  *murrine_style = MURRINE_STYLE (style);
		MurrineColors *colors = &murrine_style->colors;
		cairo_t       *cr;

		CHECK_ARGS
		SANITIZE_SIZE

		cr = murrine_begin_paint (window, area);

		WidgetParameters params;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		STYLE_FUNCTION(draw_tooltip) (cr, colors, &params, x, y, width, height);

		cairo_destroy (cr);
	}
	else
	{
		if (DETAIL ("base") || DETAIL ("eventbox") || DETAIL ("entry_bg") || DETAIL ("trough"))
		{
			MurrineStyle  *murrine_style = MURRINE_STYLE (style);
			MurrineColors *colors = &murrine_style->colors;
			cairo_t       *cr;
			boolean use_rgba = FALSE;

			CHECK_ARGS
			SANITIZE_SIZE

			use_rgba = (murrine_widget_is_rgba (widget) && murrine_style->rgba);

			if (!use_rgba)
			{
				murrine_parent_class->draw_flat_box (style, window, state_type,
				                                     shadow_type,
				                                     area, widget, detail,
				                                     x, y, width, height);
			}
			else
			{
				cr = (cairo_t*) gdk_cairo_create (window);

				if (DETAIL ("entry_bg"))
				{
					/* Draw (erase) the background */
					cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
					cairo_paint (cr);
					cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

					murrine_set_color_rgba (cr, &colors->base[state_type], ENTRY_OPACITY);
					cairo_rectangle (cr, 0, 0, width, height);
					cairo_fill (cr);
				}
				else if (DETAIL ("eventbox") || DETAIL ("trough"))
				{
					/* Draw (erase) the background */
					cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
					cairo_paint (cr);
					cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

					murrine_set_color_rgba (cr, &colors->bg[0], WINDOW_OPACITY);
					cairo_rectangle (cr, 0, 0, width, height);
					cairo_fill (cr);
				}
				else
				{
					/* Draw (erase) the background */
					cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
					cairo_paint (cr);
					cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

					cairo_pattern_t *pattern;

					pattern = cairo_pattern_create_linear (0, 0, width, 0);
					cairo_pattern_add_color_stop_rgba (pattern, 0.0, colors->bg[0].r,
					                                                 colors->bg[0].g,
					                                                 colors->bg[0].b, WINDOW_OPACITY);
					cairo_pattern_add_color_stop_rgba (pattern, 0.5, colors->bg[0].r,
					                                                 colors->bg[0].g,
					                                                 colors->bg[0].b, 0.8);
					cairo_pattern_add_color_stop_rgba (pattern, 1.0, colors->bg[0].r,
					                                                 colors->bg[0].g,
					                                                 colors->bg[0].b, WINDOW_OPACITY);
					cairo_set_source (cr, pattern);
					cairo_rectangle  (cr, 0, 0, width, height);
					cairo_fill       (cr);
					cairo_pattern_destroy (pattern);
				}

				cairo_destroy (cr);
			}
		}
		else
		{
			// printf( "draw_flat_box: %s %s\n", detail, G_OBJECT_TYPE_NAME (widget));
			murrine_parent_class->draw_flat_box (style, window, state_type,
			                                     shadow_type,
			                                     area, widget, detail,
			                                     x, y, width, height);
		}
	}

	/* Dotted listview */
	if (detail && (!strncmp ("cell_even", detail, 9) || !strncmp ("cell_odd", detail, 8)))
	{
		MurrineStyle  *murrine_style = MURRINE_STYLE (style);
		if (murrine_style->listviewstyle > 0)
		{
			MurrineColors *colors = &murrine_style->colors;
			cairo_t       *cr;

			CHECK_ARGS
			SANITIZE_SIZE

			cr = murrine_begin_paint (window, area);

			cairo_translate (cr, x, y);
			int i;
			int pos = 1;
			if (murrine_style->listviewheaderstyle != 1)
				pos = 2;

			murrine_set_color_rgba (cr, &colors->text[GTK_STATE_NORMAL], 0.42);
			for (i = 2; i < height; i+=4)
			{
				cairo_rectangle (cr, -pos, i, 1, 1);
				cairo_fill (cr);
			}

			cairo_destroy (cr);
		}
	}
}

static void
murrine_style_draw_shadow (DRAW_ARGS)
{
	MurrineStyle  *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;
	cairo_t       *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = murrine_begin_paint (window, area);

	if (DETAIL ("entry") && !(widget && widget->parent && MRN_IS_TREE_VIEW (widget->parent)))
	{
		WidgetParameters params;

		murrine_set_widget_parameters (widget, style, state_type, &params);
		params.roundness = murrine_style->roundness;

		if (widget && (MRN_IS_COMBO (widget->parent) ||
		               MRN_IS_COMBO_BOX_ENTRY(widget->parent) ||
		               MRN_IS_SPIN_BUTTON (widget)))
		{
			width += style->xthickness;
			if (!params.ltr)
				x -= style->xthickness;

			if (params.ltr)
				params.corners = MRN_CORNER_TOPLEFT | MRN_CORNER_BOTTOMLEFT;
			else
				params.corners = MRN_CORNER_TOPRIGHT | MRN_CORNER_BOTTOMRIGHT;
		}

		STYLE_FUNCTION(draw_entry) (cr, &murrine_style->colors, &params,
		                    x, y, width, height);
	}
	else if (DETAIL ("frame") && widget && MRN_IS_STATUSBAR (widget->parent))
	{
		WidgetParameters params;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		if (!params.mrn_gradient.use_rgba)
		{
			gtk_style_apply_default_background (style, window, TRUE, state_type,
			                                    area, x, y, width, height);
		}

		STYLE_FUNCTION(draw_statusbar) (cr, colors, &params,
		                                x, y, width, height);
	}
	else if (DETAIL ("frame"))
	{
		WidgetParameters params;
		FrameParameters  frame;

		frame.shadow  = shadow_type;
		frame.gap_x   = -1; /* No gap will be drawn */
		frame.border  = &colors->shade[4];

		murrine_set_widget_parameters (widget, style, state_type, &params);
		params.corners = MRN_CORNER_NONE;

		if (widget && !g_str_equal ("XfcePanelWindow", gtk_widget_get_name (gtk_widget_get_toplevel (widget))))
			STYLE_FUNCTION(draw_frame) (cr, colors, &params, &frame, x, y, width, height);
	}
	else if (DETAIL ("scrolled_window") || DETAIL ("viewport") || detail == NULL)
	{
		MurrineRGB *border = (MurrineRGB*)&colors->shade[5];
		cairo_rectangle (cr, x+0.5, y+0.5, width-1, height-1);
		murrine_set_color_rgb (cr, border);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);
	}
	else
	{
		WidgetParameters params;
		FrameParameters frame;

		frame.shadow = shadow_type;
		frame.gap_x  = -1;
		frame.border = &colors->shade[4];

		murrine_set_widget_parameters (widget, style, state_type, &params);

		params.corners = MRN_CORNER_ALL;
		if (params.roundness < 2)
			params.corners = MRN_CORNER_NONE;

		STYLE_FUNCTION(draw_frame) (cr, colors, &params, &frame, x, y, width, height);
	}

	cairo_destroy (cr);
}

static void
murrine_style_draw_box_gap (DRAW_ARGS,
                            GtkPositionType gap_side,
                            gint            gap_x,
                            gint            gap_width)
{
	MurrineStyle  *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;
	cairo_t       *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = murrine_begin_paint (window, area);

	if (DETAIL ("notebook"))
	{
		WidgetParameters params;
		FrameParameters  frame;
		gboolean start, end;

		frame.shadow    = shadow_type;
		frame.gap_side  = gap_side;
		frame.gap_x     = gap_x;
		frame.gap_width = gap_width;
		frame.border    = &colors->shade[5];

		murrine_set_widget_parameters (widget, style, state_type, &params);

		murrine_get_notebook_tab_position (widget, &start, &end);

		params.corners = MRN_CORNER_ALL;
		switch (gap_side)
		{
			case GTK_POS_TOP:
				if (murrine_widget_is_ltr (widget))
				{
					if (start)
						params.corners ^= MRN_CORNER_TOPLEFT;
					if (end)
						params.corners ^= MRN_CORNER_TOPRIGHT;
				}
				else
				{
					if (start)
						params.corners ^= MRN_CORNER_TOPRIGHT;
					if (end)
						params.corners ^= MRN_CORNER_TOPLEFT;
				}
				break;
			case GTK_POS_BOTTOM:
				if (murrine_widget_is_ltr (widget))
				{
					if (start)
						params.corners ^= MRN_CORNER_BOTTOMLEFT;
					if (end)
						params.corners ^= MRN_CORNER_BOTTOMRIGHT;
				}
				else
				{
					if (start)
						params.corners ^= MRN_CORNER_BOTTOMRIGHT;
					if (end)
						params.corners ^= MRN_CORNER_BOTTOMLEFT;
				}
				break;
			case GTK_POS_LEFT:
				if (start)
					params.corners ^= MRN_CORNER_TOPLEFT;
				if (end)
					params.corners ^= MRN_CORNER_BOTTOMLEFT;
				break;
			case GTK_POS_RIGHT:
				if (start)
					params.corners ^= MRN_CORNER_TOPRIGHT;
				if (end)
					params.corners ^= MRN_CORNER_BOTTOMRIGHT;
				break;
		}
		if (params.roundness < 2)
			params.corners = MRN_CORNER_NONE;

		/* Fill the background with bg[NORMAL] */
		murrine_rounded_rectangle (cr, x, y, width, height, params.roundness, params.corners);
		if (!params.mrn_gradient.use_rgba)
			murrine_set_color_rgb (cr, &colors->bg[0]);
		else
			murrine_set_color_rgba (cr, &colors->bg[0], NOTEBOOK_OPACITY);
		cairo_fill(cr);

		STYLE_FUNCTION(draw_frame) (cr, colors, &params, &frame,
		                            x, y, width, height);
	}
	else
	{
		murrine_parent_class->draw_box_gap (style, window, state_type, shadow_type,
		                                    area, widget, detail,
		                                    x, y, width, height,
		                                    gap_side, gap_x, gap_width);
	}

	cairo_destroy (cr);
}

static void
murrine_style_draw_extension (DRAW_ARGS, GtkPositionType gap_side)
{
	MurrineStyle  *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;
	cairo_t       *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = murrine_begin_paint (window, area);

	if (DETAIL ("tab"))
	{
		WidgetParameters params;
		TabParameters    tab;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		tab.gap_side = (MurrineGapSide)gap_side;
		switch (gap_side)
		{
			case MRN_GAP_TOP:
				params.corners = MRN_CORNER_BOTTOMLEFT | MRN_CORNER_BOTTOMRIGHT;
				break;
			case MRN_GAP_BOTTOM:
				params.corners = MRN_CORNER_TOPLEFT | MRN_CORNER_TOPRIGHT;
				break;
			case MRN_GAP_LEFT:
				params.corners = MRN_CORNER_TOPRIGHT | MRN_CORNER_BOTTOMRIGHT;
				break;
			case MRN_GAP_RIGHT:
				params.corners = MRN_CORNER_TOPLEFT | MRN_CORNER_BOTTOMLEFT;
				break;
		}

		STYLE_FUNCTION(draw_tab) (cr, colors, &params, &tab, x, y, width, height);
	}
	else
	{
		murrine_parent_class->draw_extension (style, window, state_type, shadow_type, area,
		                                      widget, detail, x, y, width, height, gap_side);
	}

	cairo_destroy (cr);
}

static void
murrine_style_draw_handle (DRAW_ARGS, GtkOrientation orientation)
{
	MurrineStyle  *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;
	cairo_t       *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = murrine_begin_paint (window, area);

	if (DETAIL ("handlebox"))
	{
		WidgetParameters params;
		HandleParameters handle;

		handle.type = MRN_HANDLE_TOOLBAR;
		handle.horizontal = (orientation == GTK_ORIENTATION_HORIZONTAL);

		murrine_set_widget_parameters (widget, style, state_type, &params);

		STYLE_FUNCTION(draw_handle) (cr, colors, &params, &handle, x, y, width, height);
	}
	else if (DETAIL ("paned"))
	{
		WidgetParameters params;
		HandleParameters handle;

		handle.type = MRN_HANDLE_SPLITTER;
		handle.horizontal = (orientation == GTK_ORIENTATION_HORIZONTAL);

		murrine_set_widget_parameters (widget, style, state_type, &params);

		STYLE_FUNCTION(draw_handle) (cr, colors, &params, &handle, x, y, width, height);
	}
	else
	{
		WidgetParameters params;
		HandleParameters handle;

		handle.type = MRN_HANDLE_TOOLBAR;
		handle.horizontal = (orientation == GTK_ORIENTATION_HORIZONTAL);

		murrine_set_widget_parameters (widget, style, state_type, &params);

		STYLE_FUNCTION(draw_handle) (cr, colors, &params, &handle, x, y, width, height);
	}

	cairo_destroy (cr);
}

static void
murrine_style_draw_box (DRAW_ARGS)
{
	MurrineStyle *murrine_style = MURRINE_STYLE (style);
	const MurrineColors *colors = &murrine_style->colors;
	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = murrine_begin_paint (window, area);

	if ((width == -1) && (height == -1))
		gdk_window_get_size (window, &width, &height);
	else if (width == -1)
		gdk_window_get_size (window, &width, NULL);
	else if (height == -1)
		gdk_window_get_size (window, NULL, &height);

	if (DETAIL ("menubar") &&
	    !(widget && (murrine_is_panel_widget (widget->parent))))
	{
		WidgetParameters params;
		gboolean horizontal;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		if (params.mrn_gradient.use_rgba)
		{
			params.mrn_gradient.rgba_opacity = MENUBAR_GLOSSY_OPACITY;
		}

		horizontal = height < 2*width;
		/* This is not that great. Ideally we would have a nice vertical menubar. */
		if ((shadow_type != GTK_SHADOW_NONE) && horizontal)
			STYLE_FUNCTION(draw_menubar) (cr, colors, &params, x, y, width, height,
			                              murrine_style->menubarstyle);
	}
	else if (DETAIL ("button") && widget && widget->parent &&
	                 (MRN_IS_TREE_VIEW(widget->parent) ||
	                  MRN_IS_CLIST (widget->parent)))
	{
		WidgetParameters params;
		ListViewHeaderParameters header;
		header.style = murrine_style->listviewheaderstyle;

		gint columns, column_index;
		gboolean resizable = TRUE;

		murrine_set_widget_parameters (widget, style, state_type, &params);
		params.corners = MRN_CORNER_NONE;

		if (MRN_IS_TREE_VIEW (widget->parent))
		{
			murrine_gtk_treeview_get_header_index (GTK_TREE_VIEW(widget->parent),
			                                       widget, &column_index, &columns,
			                                       &resizable);
		}
		else if (MRN_IS_CLIST (widget->parent))
		{
			murrine_gtk_clist_get_header_index (GTK_CLIST(widget->parent),
			                                    widget, &column_index, &columns);
		}

		header.resizable = resizable;

		if (column_index == 0)
			header.order = params.ltr ? MRN_ORDER_FIRST : MRN_ORDER_LAST;
		else if (column_index == columns-1)
			header.order = params.ltr ? MRN_ORDER_LAST : MRN_ORDER_FIRST;
		else
			header.order = MRN_ORDER_MIDDLE;

		gtk_style_apply_default_background (style, window, FALSE, state_type, area, x, y, width, height);

		STYLE_FUNCTION(draw_list_view_header) (cr, colors, &params, &header, x, y, width, height);
	}
	else if (DETAIL ("button") || DETAIL ("buttondefault"))
	{
		WidgetParameters params;

		murrine_set_widget_parameters (widget, style, state_type, &params);
		params.roundness = murrine_style->roundness;

		boolean horizontal = TRUE;
		if (((float)width/height<0.5)|| (murrine_style->glazestyle > 0 && width<height))
			horizontal = FALSE;

		if ((widget && (MRN_IS_COMBO_BOX_ENTRY (widget->parent) || MRN_IS_COMBO (widget->parent))))
		{
			if (murrine_style->roundness > 0)
			{
				if (params.ltr)
				{
					params.corners = MRN_CORNER_TOPRIGHT | MRN_CORNER_BOTTOMRIGHT;
					if (!horizontal)
						params.corners = MRN_CORNER_BOTTOMLEFT | MRN_CORNER_BOTTOMRIGHT;
				}
				else
				{
					params.corners = MRN_CORNER_TOPLEFT | MRN_CORNER_BOTTOMLEFT;
					if (!horizontal)
						params.corners = MRN_CORNER_BOTTOMRIGHT | MRN_CORNER_BOTTOMLEFT;
				}
			}
			else
				params.corners = MRN_CORNER_NONE;

			/* Seriously, why can't non-gtk-apps at least try to be decent citizens?
			   Take this fucking OpenOffice.org 1.9 for example. The morons responsible
			   for this utter piece of crap gave the clip size wrong values! :'(  */
/*			cairo_reset_clip (cr);
			cairo_rectangle (cr, x+ 0.5, y+ 0.5, 10, 10);
			cairo_clip (cr);
			cairo_new_path (cr);
*/
			if (params.xthickness > 1)
			{
				if (params.ltr)
					x--;
				width++;
			}
		}
		else
			if (murrine_style->roundness > 0)
				params.corners = MRN_CORNER_ALL;
			else
				params.corners = MRN_CORNER_NONE;

		if (MRN_IS_TOGGLE_BUTTON (widget) &&
		    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
			params.active = TRUE;

		/* Fix some firefox crap. */
		if (MRN_IS_BUTTON (widget) && MRN_IS_FIXED (widget->parent) && widget->allocation.x == -1 &&  widget->allocation.y == -1)
		{
			gtk_style_apply_default_background (widget->parent->style, window, TRUE, GTK_STATE_NORMAL,
			                                    area, x, y, width, height);
		}

			STYLE_FUNCTION(draw_button) (cr, &murrine_style->colors, &params, x, y, width, height, horizontal);
	}
	else if (DETAIL ("spinbutton_up") || DETAIL ("spinbutton_down"))
	{
		if (state_type == GTK_STATE_ACTIVE)
		{
			WidgetParameters params;
			murrine_set_widget_parameters (widget, style, state_type, &params);

			if (style->xthickness > 1)
			{
				width++;
				if (params.ltr)
					x--;
			}

			if (DETAIL ("spinbutton_up"))
			{
				height+=2;
				if (params.ltr)
					params.corners = MRN_CORNER_TOPRIGHT;
				else
					params.corners = MRN_CORNER_TOPLEFT;
			}
			else
			{
				if (params.ltr)
					params.corners = MRN_CORNER_BOTTOMRIGHT;
				else
					params.corners = MRN_CORNER_BOTTOMLEFT;
			}

			STYLE_FUNCTION(draw_spinbutton_down) (cr, &murrine_style->colors, &params, x, y, width, height);
		}
	}
	else if (DETAIL ("spinbutton"))
	{
		WidgetParameters params;

		murrine_set_widget_parameters (widget, style, state_type, &params);
		params.roundness = murrine_style->roundness;

		boolean horizontal = TRUE;
		if (((float)width/height<0.5)|| (murrine_style->glazestyle > 0 && width<height))
			horizontal = FALSE;

		if (murrine_style->roundness > 0)
		{
			if (params.ltr)
			{
				params.corners = MRN_CORNER_TOPRIGHT | MRN_CORNER_BOTTOMRIGHT;
				if (!horizontal)
					params.corners = MRN_CORNER_BOTTOMLEFT | MRN_CORNER_BOTTOMRIGHT;
			}
			else
			{
				params.corners = MRN_CORNER_TOPLEFT | MRN_CORNER_BOTTOMLEFT;
				if (!horizontal)
					params.corners = MRN_CORNER_BOTTOMRIGHT | MRN_CORNER_BOTTOMLEFT;
			}
		}
		else
			params.corners = MRN_CORNER_NONE;

		if (style->xthickness > 1)
		{
			if (params.ltr)
				x--;
			width++;
		}

		/* draw_spinbutton (cr, &murrine_style->colors, &params, x, y, width, height); */
		STYLE_FUNCTION(draw_button) (cr, &murrine_style->colors, &params, x, y, width, height, horizontal);
	}
	else if (DETAIL ("trough") && widget && MRN_IS_SCALE (widget))
	{
		GtkAdjustment *adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
		WidgetParameters params;
		SliderParameters slider;
		gint slider_length;

		gtk_widget_style_get (widget, "slider-length", &slider_length, NULL);

		slider.inverted   = gtk_range_get_inverted (GTK_RANGE (widget));
		slider.horizontal = (GTK_RANGE (widget)->orientation == GTK_ORIENTATION_HORIZONTAL);
		slider.fill_size  = ((slider.horizontal ? width : height) - slider_length) *
		                     (1 / ((adjustment->upper - adjustment->lower) / (adjustment->value - adjustment->lower)))
		                     + slider_length / 2;
		if (slider.horizontal)
			slider.inverted = slider.inverted != (murrine_get_direction (widget) == GTK_TEXT_DIR_RTL);

		murrine_set_widget_parameters (widget, style, state_type, &params);
		params.corners    = MRN_CORNER_NONE;

		STYLE_FUNCTION(draw_scale_trough) (cr, &murrine_style->colors,
		                                   &params, &slider,
		                                   x, y, width, height);
	}
	else if (DETAIL ("trough") && widget && MRN_IS_PROGRESS_BAR (widget))
	{
		WidgetParameters params;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		STYLE_FUNCTION(draw_progressbar_trough) (cr, colors, &params, x, y, width, height);
	}
	else if (DETAIL ("trough") && widget && (MRN_IS_VSCROLLBAR (widget) || MRN_IS_HSCROLLBAR (widget)))
	{
		WidgetParameters params;
		ScrollBarParameters scrollbar;

		scrollbar.horizontal   = TRUE;
		scrollbar.junction     = murrine_scrollbar_get_junction (widget);
		scrollbar.steppers     = murrine_scrollbar_visible_steppers (widget);
		scrollbar.stepperstyle = murrine_style->stepperstyle;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		if (murrine_style->roundness > 0)
			params.corners = MRN_CORNER_ALL;
		else
			params.corners = MRN_CORNER_NONE;

		if (MRN_IS_RANGE (widget))
			scrollbar.horizontal = GTK_RANGE (widget)->orientation == GTK_ORIENTATION_HORIZONTAL;

		if (murrine_style->stepperstyle != 1 && !params.mrn_gradient.use_rgba)
		{
			if (scrollbar.horizontal)
			{
				x += 2;
				width -= 4;
			}
			else
			{
				y += 2;
				height -= 4;
			}
		}

		STYLE_FUNCTION(draw_scrollbar_trough) (cr, colors, &params, &scrollbar, x, y, width, height);
	}
	else if (DETAIL ("bar"))
	{
		WidgetParameters      params;

		ProgressBarParameters progressbar;
		gdouble               elapsed = 0.0;

		if (widget && MRN_IS_PROGRESS_BAR (widget))
			progressbar.orientation = gtk_progress_bar_get_orientation (GTK_PROGRESS_BAR (widget));
		else
			progressbar.orientation = MRN_ORIENTATION_LEFT_TO_RIGHT;

		if (!params.ltr)
		{
			if (progressbar.orientation == GTK_PROGRESS_LEFT_TO_RIGHT)
				progressbar.orientation = GTK_PROGRESS_RIGHT_TO_LEFT;
			else if (progressbar.orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
				progressbar.orientation = GTK_PROGRESS_LEFT_TO_RIGHT;
		}

#ifdef HAVE_ANIMATION
		if(murrine_style->animation && MRN_IS_PROGRESS_BAR (widget))
		{
			gboolean activity_mode = GTK_PROGRESS (widget)->activity_mode;

			if (!activity_mode)
				murrine_animation_progressbar_add ((gpointer)widget);
		}

		elapsed = murrine_animation_elapsed (widget);
#endif

		murrine_set_widget_parameters (widget, style, state_type, &params);
		params.roundness = murrine_style->roundness;

		/* cairo_reset_clip (cr); */

		/* The x-1 and width+2 are to make the fill cover the left and
		 * right-hand sides of the trough box */

#ifndef HAVE_ANIMATIONTOLEFT
		STYLE_FUNCTION(draw_progressbar_fill) (cr, colors, &params, &progressbar,
		                                       x-1, y, width+2, height,
		                                       10 - (int)(elapsed * 10.0) % 10);
#else
		STYLE_FUNCTION(draw_progressbar_fill) (cr, colors, &params, &progressbar,
		                                       x-1, y, width+2, height,
		                                       10 + (int)(elapsed * 10.0) % 10);
#endif
	}
	else if (DETAIL ("hscale") || DETAIL ("vscale"))
	{
		WidgetParameters params;
		/* SliderParameters slider; */

		murrine_set_widget_parameters (widget, style, state_type, &params);

		boolean horizontal = TRUE;
		if (DETAIL ("vscale"))
			horizontal = FALSE;

		if (murrine_style->roundness > 0)
			params.corners = MRN_CORNER_ALL;
		else
			params.corners = MRN_CORNER_NONE;

		STYLE_FUNCTION(draw_button) (cr, &murrine_style->colors, &params, x, y, width, height, horizontal);

		if (murrine_style->sliderstyle == 1)
			STYLE_FUNCTION(draw_slider_handle) (cr, &murrine_style->colors, &params, x, y, width, height, horizontal);
	}
	else if (DETAIL ("optionmenu"))
	{
		WidgetParameters params;
		OptionMenuParameters optionmenu;

		GtkRequisition indicator_size;
		GtkBorder indicator_spacing;

		murrine_option_menu_get_props (widget, &indicator_size, &indicator_spacing);

		if (widget && murrine_get_direction (widget) == GTK_TEXT_DIR_RTL)
			optionmenu.linepos = (indicator_size.width + indicator_spacing.left + indicator_spacing.right) + style->xthickness;
		else
			optionmenu.linepos = width - (indicator_size.width + indicator_spacing.left + indicator_spacing.right) - style->xthickness;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		if (murrine_style->roundness > 0)
			params.corners = MRN_CORNER_ALL;
		else
			params.corners = MRN_CORNER_NONE;

		STYLE_FUNCTION(draw_optionmenu) (cr, colors, &params, &optionmenu, x, y, width, height);
	}
	else if (DETAIL ("menuitem"))
	{
		WidgetParameters params;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		if (murrine_style->roundness > 0)
			params.corners = MRN_CORNER_ALL;
		else
			params.corners = MRN_CORNER_NONE;

		if (widget && !(MRN_IS_MENU_BAR (widget->parent) && murrine_style->menubaritemstyle))
		{
			if (murrine_style->menustyle != 1 || (MRN_IS_MENU_BAR (widget->parent) && !murrine_style->menubaritemstyle))
				STYLE_FUNCTION(draw_menuitem) (cr, colors, &params, x, y, width, height, murrine_style->menuitemstyle);
			else
				/* little translation */
				STYLE_FUNCTION(draw_menuitem) (cr, colors, &params, x+3, y, width-3, height, murrine_style->menuitemstyle);
		}

		if (widget && MRN_IS_MENU_BAR (widget->parent) && murrine_style->menubaritemstyle)
		{
			params.active = FALSE;
			params.prelight = TRUE;
			params.focus = TRUE;
			params.state_type = MRN_STATE_SELECTED;
			params.xthickness = 2;
			params.ythickness = 2;

			if (murrine_style->roundness > 0)
				params.corners = MRN_CORNER_TOPRIGHT | MRN_CORNER_TOPLEFT;
			else
				params.corners = MRN_CORNER_NONE;

				STYLE_FUNCTION(draw_button) (cr, colors, &params, x, y, width, height+1, TRUE);
		}
	}
	else if (DETAIL ("hscrollbar") || DETAIL ("vscrollbar") || DETAIL ("slider") || DETAIL ("stepper"))
	{
		WidgetParameters    params;
		ScrollBarParameters scrollbar;

		scrollbar.has_color    = FALSE;
		scrollbar.horizontal   = TRUE;
		scrollbar.junction     = murrine_scrollbar_get_junction (widget);
		scrollbar.steppers     = murrine_scrollbar_visible_steppers (widget);
		scrollbar.style        = murrine_style->scrollbarstyle;
		scrollbar.stepperstyle = murrine_style->stepperstyle;

		if (MRN_IS_RANGE (widget))
			scrollbar.horizontal = GTK_RANGE (widget)->orientation == GTK_ORIENTATION_HORIZONTAL;

		if (murrine_style->colorize_scrollbar)
		{
			scrollbar.color = colors->spot[1];
			scrollbar.has_color = TRUE;
		}

		if (!scrollbar.has_color)
			scrollbar.color = colors->bg[0];

		if (murrine_style->has_scrollbar_color)
		{
			murrine_gdk_color_to_rgb (&murrine_style->scrollbar_color, &scrollbar.color.r, &scrollbar.color.g, &scrollbar.color.b);
			scrollbar.has_color = TRUE;
		}

		murrine_set_widget_parameters (widget, style, state_type, &params);

		params.corners = MRN_CORNER_NONE;

		if (DETAIL ("slider"))
		{
			if (murrine_style->roundness == 1)
				params.corners = MRN_CORNER_ALL;
			else
				params.corners = MRN_CORNER_NONE;

			STYLE_FUNCTION(draw_scrollbar_slider) (cr, colors, &params, &scrollbar, x, y, width, height);
		}
		else
		{
			if (murrine_style->roundness > 1)
			{
				ScrollBarStepperParameters stepper;
				GdkRectangle this_rectangle = { x, y, width, height };

				stepper.stepper = murrine_scrollbar_get_stepper (widget, &this_rectangle);

				if (scrollbar.horizontal)
				{
					if (stepper.stepper == MRN_STEPPER_A)
						params.corners = MRN_CORNER_TOPLEFT | MRN_CORNER_BOTTOMLEFT;
					else if (stepper.stepper == MRN_STEPPER_D)
						params.corners = MRN_CORNER_TOPRIGHT | MRN_CORNER_BOTTOMRIGHT;
				}
				else
				{
					if (stepper.stepper == MRN_STEPPER_A)
						params.corners = MRN_CORNER_BOTTOMLEFT | MRN_CORNER_TOPLEFT;
					else if (stepper.stepper == MRN_STEPPER_D)
						params.corners = MRN_CORNER_TOPRIGHT | MRN_CORNER_BOTTOMRIGHT;
				}
			}
			else if (murrine_style->roundness == 1)
				params.corners = MRN_CORNER_ALL;
			else
				params.corners = MRN_CORNER_NONE;

			if (murrine_style->stepperstyle != 1)
				STYLE_FUNCTION(draw_scrollbar_stepper) (cr, colors, &params, &scrollbar, x, y, width, height);
		}
	}
	else if (DETAIL ("toolbar") || DETAIL ("handlebox_bin") || DETAIL ("dockitem_bin"))
	{
		/* Only draw the shadows on horizontal toolbars */
		if (shadow_type != GTK_SHADOW_NONE && height < 2*width )
		{
			WidgetParameters params;
			ToolbarParameters toolbar;
			gboolean horizontal;

			murrine_set_widget_parameters (widget, style, state_type, &params);

			murrine_set_toolbar_parameters (&toolbar, widget, window, x, y);
			toolbar.style = murrine_style->toolbarstyle;
			if ((DETAIL ("handlebox_bin") || DETAIL ("dockitem_bin")) && MRN_IS_BIN (widget))
			{
				GtkWidget* child = gtk_bin_get_child ((GtkBin*) widget);
				/* This is to draw the correct shadow on the handlebox.
				 * We need to draw it here, as otherwise the handle will not get the
				 * background. */
				if (MRN_IS_TOOLBAR (child))
					gtk_widget_style_get (child, "shadow-type", &shadow_type, NULL);
			}

			horizontal = height < 2*width;
			/* This is not that great. Ideally we would have a nice vertical toolbar. */
			if ((shadow_type != GTK_SHADOW_NONE) && horizontal)
				STYLE_FUNCTION(draw_toolbar) (cr, colors, &params, &toolbar, x, y, width, height);
		}
	}
	else if (DETAIL ("trough"))
	{
		/* Do nothing? */
	}
	else if (DETAIL ("menu"))
	{
		WidgetParameters params;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		STYLE_FUNCTION(draw_menu_frame) (cr, colors, &params, x, y, width, height, murrine_style->menustyle);
	}
	else
	{
		murrine_parent_class->draw_box (style, window, state_type, shadow_type, area,
		                                widget, detail, x, y, width, height);
	}

	cairo_destroy (cr);
}

static void
murrine_style_draw_slider (DRAW_ARGS, GtkOrientation orientation)
{
	if (DETAIL ("hscale") || DETAIL ("vscale"))
	{
		murrine_style_draw_box (style, window, state_type, shadow_type, area,
		                        widget, detail, x, y, width, height);
	}
	else
		murrine_parent_class->draw_slider (style, window, state_type, shadow_type, area,
		                                   widget, detail, x, y, width, height, orientation);
}

static void
murrine_style_draw_option (DRAW_ARGS)
{
	MurrineStyle *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;
	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = murrine_begin_paint (window, area);

	WidgetParameters params;
	OptionParameters option;

	option.draw_bullet = ((shadow_type == GTK_SHADOW_IN) || (shadow_type == GTK_SHADOW_ETCHED_IN));
	option.inconsistent = (shadow_type == GTK_SHADOW_ETCHED_IN);
	double trans = 1.0;

	murrine_set_widget_parameters (widget, style, state_type, &params);

	#ifdef HAVE_ANIMATION
		if (murrine_style->animation)
			murrine_animation_connect_checkbox (widget);

		if (murrine_style->animation &&
		    MRN_IS_CHECK_BUTTON (widget) &&
		    murrine_animation_is_animated (widget) &&
		    !gtk_toggle_button_get_inconsistent (GTK_TOGGLE_BUTTON (widget)))
		{
			gfloat elapsed = murrine_animation_elapsed (widget);
			trans = sqrt (sqrt (MIN(elapsed / CHECK_ANIMATION_TIME, 1.0)));
		}
	#endif

	STYLE_FUNCTION(draw_radiobutton) (cr, colors, &params, &option, x, y, width, height, trans);

	cairo_destroy (cr);
}

static void
murrine_style_draw_check (DRAW_ARGS)
{
	MurrineStyle *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;
	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = murrine_begin_paint (window, area);

	WidgetParameters params;
	OptionParameters option;

	option.draw_bullet = ((shadow_type == GTK_SHADOW_IN) || (shadow_type == GTK_SHADOW_ETCHED_IN));
	option.inconsistent = (shadow_type == GTK_SHADOW_ETCHED_IN);
	double trans = 1.0;

	murrine_set_widget_parameters (widget, style, state_type, &params);

	#ifdef HAVE_ANIMATION
		if (murrine_style->animation)
			murrine_animation_connect_checkbox (widget);

		if (murrine_style->animation && MRN_IS_CHECK_BUTTON (widget) &&
		    murrine_animation_is_animated (widget) &&
		    !gtk_toggle_button_get_inconsistent (GTK_TOGGLE_BUTTON (widget)))
		{
			gfloat elapsed = murrine_animation_elapsed (widget);
			trans = sqrt (sqrt (MIN(elapsed / CHECK_ANIMATION_TIME, 1.0)));
		}
	#endif

	STYLE_FUNCTION(draw_checkbox) (cr, colors, &params, &option, x, y, width, height, trans);

	cairo_destroy (cr);
}

static void
murrine_style_draw_tab (DRAW_ARGS)
{
	MurrineStyle *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;

	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = murrine_begin_paint (window, area);

	WidgetParameters params;
	ArrowParameters  arrow;

	arrow.type      = MRN_ARROW_COMBO;
	arrow.direction = MRN_DIRECTION_DOWN;

	murrine_set_widget_parameters (widget, style, state_type, &params);

	STYLE_FUNCTION(draw_arrow) (cr, colors, &params, &arrow, x, y, width, height);

	cairo_destroy (cr);
}

static void
murrine_style_draw_vline (GtkStyle     *style,
                          GdkWindow    *window,
                          GtkStateType  state_type,
                          GdkRectangle *area,
                          GtkWidget    *widget,
                          const gchar  *detail,
                          gint          y1,
                          gint          y2,
                          gint          x)
{
	/* Get toplevel window for this widget */
	GtkWidget* toplevel = gtk_widget_get_toplevel (widget);

	MurrineStyle  *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;
	cairo_t *cr;

	CHECK_ARGS

	cr = murrine_begin_paint (window, area);

	SeparatorParameters separator;
	separator.horizontal = FALSE;

	WidgetParameters params;

	params.drawstyle = MRN_DRAW_STYLE_MURRINE;
	if (murrine_widget_is_rgba (toplevel))
	{
		params.drawstyle = MRN_DRAW_STYLE_RGBA;
	}

	if (!(widget &&
	    MRN_IS_HBOX (widget->parent) &&
	    MRN_IS_TOGGLE_BUTTON (widget->parent->parent) &&
	    MRN_IS_COMBO_BOX (widget->parent->parent->parent)))
	{
		STYLE_FUNCTION(draw_separator) (cr, colors, NULL, &separator, x, y1, 2, y2-y1);
	}
	else
		STYLE_FUNCTION(draw_combo_separator) (cr, colors, NULL, x, y1, 2, y2-y1);

	cairo_destroy (cr);
}

static void
murrine_style_draw_hline (GtkStyle     *style,
                          GdkWindow    *window,
                          GtkStateType  state_type,
                          GdkRectangle *area,
                          GtkWidget    *widget,
                          const gchar  *detail,
                          gint          x1,
                          gint          x2,
                          gint          y)
{
	/* Get toplevel window for this widget */
	GtkWidget* toplevel = gtk_widget_get_toplevel (widget);

	MurrineStyle *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;
	cairo_t *cr;

	CHECK_ARGS

	cr = murrine_begin_paint (window, area);

	SeparatorParameters separator;
	separator.horizontal = TRUE;

	WidgetParameters params;

	params.drawstyle = MRN_DRAW_STYLE_MURRINE;
	if (murrine_widget_is_rgba (toplevel))
	{
		params.drawstyle = MRN_DRAW_STYLE_RGBA;
	}

	STYLE_FUNCTION(draw_separator) (cr, colors, NULL, &separator, x1, y, x2-x1, 2);

	cairo_destroy (cr);
}

static void
murrine_style_draw_shadow_gap (DRAW_ARGS,
                               GtkPositionType gap_side,
                               gint            gap_x,
                               gint            gap_width)
{
	MurrineStyle *murrine_style = MURRINE_STYLE (style);
	const MurrineColors *colors = &murrine_style->colors;
	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = murrine_begin_paint (window, area);

	if (DETAIL ("frame"))
	{
		WidgetParameters params;
		FrameParameters  frame;

		frame.shadow    = shadow_type;
		frame.gap_side  = gap_side;
		frame.gap_x     = gap_x;
		frame.gap_width = gap_width;
		frame.border    = (MurrineRGB*)&colors->shade[4];

		murrine_set_widget_parameters (widget, style, state_type, &params);

		params.corners = MRN_CORNER_ALL;

		if (params.roundness < 2)
			params.corners = MRN_CORNER_NONE;

		STYLE_FUNCTION(draw_frame) (cr, colors, &params, &frame, x, y, width, height);
	}
	else
	{
		murrine_parent_class->draw_shadow_gap (style, window, state_type, shadow_type, area,
		                                       widget, detail, x, y, width, height,
		                                       gap_side, gap_x, gap_width);
	}

	cairo_destroy (cr);
}

static void
murrine_style_draw_resize_grip (GtkStyle      *style,
                                GdkWindow     *window,
                                GtkStateType  state_type,
                                GdkRectangle  *area,
                                GtkWidget     *widget,
                                const gchar   *detail,
                                GdkWindowEdge edge,
                                gint          x,
                                gint          y,
                                gint          width,
                                gint          height)
{
	MurrineStyle *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;

	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = murrine_begin_paint (window, area);

	WidgetParameters params;
	ResizeGripParameters grip;
	grip.edge = (MurrineWindowEdge)edge;

	int lx, ly;

	g_return_if_fail (window != NULL);

	if (edge != GDK_WINDOW_EDGE_SOUTH_EAST)
		return; /* sorry... need to work on this :P */

	murrine_set_widget_parameters (widget, style, state_type, &params);

	STYLE_FUNCTION(draw_resize_grip) (cr, colors, &params, &grip, x, y, width, height);

	cairo_destroy (cr);

	return;
}

static void
murrine_style_draw_arrow (GtkStyle     *style,
                          GdkWindow    *window,
                          GtkStateType  state_type,
                          GtkShadowType shadow,
                          GdkRectangle  *area,
                          GtkWidget     *widget,
                          const gchar   *detail,
                          GtkArrowType  arrow_type,
                          gboolean      fill,
                          gint          x,
                          gint          y,
                          gint          width,
                          gint          height)
{
	MurrineStyle  *murrine_style = MURRINE_STYLE (style);
	MurrineColors *colors = &murrine_style->colors;
	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE;

	cr = murrine_begin_paint (window, area);

	WidgetParameters params;
	ArrowParameters  arrow;

	if (arrow_type == GTK_ARROW_NONE)
	{
		cairo_destroy (cr);
		return;
	}

	arrow.type = MRN_ARROW_NORMAL;
	arrow.direction = (MurrineDirection)arrow_type;

	if (MRN_IS_COMBO_BOX (widget) && !MRN_IS_COMBO_BOX_ENTRY (widget))
	{
		arrow.type = MRN_ARROW_COMBO;
	}

	murrine_set_widget_parameters (widget, style, state_type, &params);

	/* I have no idea why, but the arrow of GtkCombo is larger than in other places.
	 * Subtracting 3 seems to fix this. */
	if (widget && widget->parent && MRN_IS_COMBO (widget->parent->parent))
	{
		if (params.ltr)
			x += 1;
		else
			x += 2;
		width -= 3;
	}

	STYLE_FUNCTION(draw_arrow) (cr, colors, &params, &arrow, x, y, width, height);

	cairo_destroy (cr);
}

static void
murrine_style_draw_layout (GtkStyle     *style,
                           GdkWindow    *window,
                           GtkStateType state_type,
                           gboolean     use_text,
                           GdkRectangle *area,
                           GtkWidget    *widget,
                           const gchar  *detail, gint x, gint y,
                           PangoLayout  *layout)
{
	GdkGC *gc;

	g_return_if_fail (GTK_IS_STYLE (style));
	g_return_if_fail (window != NULL);

	gc = use_text ? style->text_gc[state_type] : style->fg_gc[state_type];

	if (area)
		gdk_gc_set_clip_rectangle (gc, area);

	if (state_type == GTK_STATE_INSENSITIVE)
	{
		MurrineStyle *murrine_style = MURRINE_STYLE (style);
		MurrineColors *colors = &murrine_style->colors;

		WidgetParameters params;

		murrine_set_widget_parameters (widget, style, state_type, &params);

		GdkColor etched;
		MurrineRGB temp;

		if (GTK_WIDGET_NO_WINDOW (widget))
			murrine_shade (&params.parentbg, &temp, 1.2);
		else
			murrine_shade (&colors->bg[widget->state], &temp, 1.2);

		etched.red = (int) (temp.r * 65535);
		etched.green = (int) (temp.g * 65535);
		etched.blue = (int) (temp.b * 65535);

		gdk_draw_layout_with_colors (window, gc, x + 1, y + 1, layout, &etched, NULL);
		gdk_draw_layout (window, gc, x, y, layout);
	}
	else
	{
		gdk_draw_layout (window, gc, x, y, layout);
	}

	if (area)
		gdk_gc_set_clip_rectangle (gc, NULL);
}

static void
murrine_style_init_from_rc (GtkStyle   *style,
                            GtkRcStyle *rc_style)
{
	MurrineStyle *murrine_style = MURRINE_STYLE (style);

	murrine_parent_class->init_from_rc (style, rc_style);

	murrine_style->gradient_stop_1    = MURRINE_RC_STYLE (rc_style)->gradient_stop_1;
	murrine_style->gradient_stop_2    = MURRINE_RC_STYLE (rc_style)->gradient_stop_2;
	murrine_style->gradient_stop_3    = MURRINE_RC_STYLE (rc_style)->gradient_stop_3;
	murrine_style->gradient_stop_4    = MURRINE_RC_STYLE (rc_style)->gradient_stop_4;
	murrine_style->has_gradient_stop  = MURRINE_RC_STYLE (rc_style)->has_gradient_stop;
	murrine_style->highlight_ratio    = MURRINE_RC_STYLE (rc_style)->highlight_ratio;
	murrine_style->innerborder_ratio  = MURRINE_RC_STYLE (rc_style)->innerborder_ratio;
	murrine_style->glazestyle         = MURRINE_RC_STYLE (rc_style)->glazestyle;

	if (murrine_style->has_gradient_stop &&
	    murrine_style->glazestyle != 0)
	{
		double gradient_stop_mid = ((murrine_style->gradient_stop_2)+(murrine_style->gradient_stop_2))/2.0;
		murrine_style->gradient_stop_2 = gradient_stop_mid;
		murrine_style->gradient_stop_3 = gradient_stop_mid;
	}

	/* Ignore roundness > 1 with glazestyle = 2 */
	if (murrine_style->glazestyle == 2)
	{
		if (MURRINE_RC_STYLE (rc_style)->roundness > 0)
			murrine_style->roundness = 1;
		else
			murrine_style->roundness = 0;
	}
	else
		murrine_style->roundness       = MURRINE_RC_STYLE (rc_style)->roundness;
	murrine_style->menubarstyle        = MURRINE_RC_STYLE (rc_style)->menubarstyle;
	murrine_style->menubaritemstyle    = MURRINE_RC_STYLE (rc_style)->menubaritemstyle;
	murrine_style->menustyle           = MURRINE_RC_STYLE (rc_style)->menustyle;
	murrine_style->menuitemstyle       = MURRINE_RC_STYLE (rc_style)->menuitemstyle;
	murrine_style->listviewheaderstyle = MURRINE_RC_STYLE (rc_style)->listviewheaderstyle;
	murrine_style->listviewstyle       = MURRINE_RC_STYLE (rc_style)->listviewstyle;
	murrine_style->sliderstyle         = MURRINE_RC_STYLE (rc_style)->sliderstyle;
	murrine_style->scrollbarstyle      = MURRINE_RC_STYLE (rc_style)->scrollbarstyle;
	murrine_style->has_scrollbar_color = MURRINE_RC_STYLE (rc_style)->has_scrollbar_color;
	murrine_style->stepperstyle        = MURRINE_RC_STYLE (rc_style)->stepperstyle;
	murrine_style->toolbarstyle        = MURRINE_RC_STYLE (rc_style)->toolbarstyle;
	murrine_style->animation           = MURRINE_RC_STYLE (rc_style)->animation;
	murrine_style->gradients           = MURRINE_RC_STYLE (rc_style)->gradients;
	murrine_style->colorize_scrollbar  = MURRINE_RC_STYLE (rc_style)->colorize_scrollbar;
	murrine_style->rgba                = MURRINE_RC_STYLE (rc_style)->rgba;

	if (murrine_style->has_scrollbar_color)
		murrine_style->scrollbar_color = MURRINE_RC_STYLE (rc_style)->scrollbar_color;

	g_assert ((MURRINE_RC_STYLE (rc_style)->style >= 0) && (MURRINE_RC_STYLE (rc_style)->style < MRN_NUM_STYLES));
	murrine_style->style               = MURRINE_RC_STYLE (rc_style)->style;

	switch (murrine_style->style)
	{
		case (MRN_STYLE_NODOKA):
			murrine_style->highlight_ratio = 1.0;
			murrine_style->gradients = TRUE;
			murrine_style->sliderstyle = 1;
			murrine_style->scrollbarstyle = 2;
			murrine_style->stepperstyle = 0;
			murrine_style->colorize_scrollbar = FALSE;
			murrine_style->has_scrollbar_color = FALSE;
			murrine_style->has_gradient_stop = FALSE;
			break;
		case (MRN_STYLE_MIST):
			murrine_style->highlight_ratio = 1.0;
			murrine_style->gradients = FALSE;
			break;
		case (MRN_STYLE_CANDIDO):
			murrine_style->highlight_ratio = 1.0;
			murrine_style->innerborder_ratio = 1.06;
			murrine_style->gradients = TRUE;
			murrine_style->gradient_stop_1 = 1.01;
			murrine_style->gradient_stop_2 = 0.99;
			murrine_style->gradient_stop_3 = 0.99;
			murrine_style->gradient_stop_4 = 0.97;
			break;
		case (MRN_STYLE_CLEARLOOKS):
			murrine_style->gradient_stop_1 = 1.08;
			murrine_style->gradient_stop_2 = 1.02;
			murrine_style->gradient_stop_3 = 1.00;
			murrine_style->gradient_stop_4 = 0.94;
			murrine_style->has_gradient_stop = TRUE;
			murrine_style->gradients = TRUE;
			murrine_style->highlight_ratio = 1.0;
			murrine_style->toolbarstyle = 1;
			murrine_style->innerborder_ratio = 1.02;
			murrine_style->menustyle = 0;
			murrine_style->sliderstyle = 1;
			murrine_style->scrollbarstyle = 2;
			break;
	};
}

static void
murrine_style_realize (GtkStyle * style)
{
	MurrineStyle *murrine_style = MURRINE_STYLE (style);
	double shades[] = {1.15, 0.95, 0.896, 0.82, 0.75, 0.665, 0.5, 0.45, 0.4};
	MurrineRGB spot_color;
	MurrineRGB bg_normal;
	double contrast;
	int i;

	murrine_parent_class->realize (style);

	contrast = MURRINE_RC_STYLE (style->rc_style)->contrast;

	bg_normal.r = style->bg[0].red   / 65535.0;
	bg_normal.g = style->bg[0].green / 65535.0;
	bg_normal.b = style->bg[0].blue  / 65535.0;

	/* Lighter to darker */
	for (i = 0; i < 9; i++)
	{
		murrine_shade (&bg_normal, &murrine_style->colors.shade[i],
		               (shades[i]-0.9) * contrast + 0.9);
	}

	spot_color.r = style->bg[GTK_STATE_SELECTED].red   / 65535.0;
	spot_color.g = style->bg[GTK_STATE_SELECTED].green / 65535.0;
	spot_color.b = style->bg[GTK_STATE_SELECTED].blue  / 65535.0;

	murrine_shade (&spot_color, &murrine_style->colors.spot[0], 1.42);
	murrine_shade (&spot_color, &murrine_style->colors.spot[1], 1.00);
	murrine_shade (&spot_color, &murrine_style->colors.spot[2], 0.65);

	for (i=0; i<5; i++)
	{
		murrine_gdk_color_to_rgb (&style->bg[i],
		                          &murrine_style->colors.bg[i].r,
		                          &murrine_style->colors.bg[i].g,
		                          &murrine_style->colors.bg[i].b);

		murrine_gdk_color_to_rgb (&style->base[i],
		                          &murrine_style->colors.base[i].r,
		                          &murrine_style->colors.base[i].g,
		                          &murrine_style->colors.base[i].b);

		murrine_gdk_color_to_rgb (&style->text[i],
		                          &murrine_style->colors.text[i].r,
		                          &murrine_style->colors.text[i].g,
		                          &murrine_style->colors.text[i].b);

		murrine_gdk_color_to_rgb (&style->fg[i],
		                          &murrine_style->colors.fg[i].r,
		                          &murrine_style->colors.fg[i].g,
		                          &murrine_style->colors.fg[i].b);
	}
}

static void
gdk_cairo_set_source_color_alpha (cairo_t  *cr,
                                  GdkColor *color,
                                  float    alpha)
{
	g_return_if_fail (cr != NULL);
	g_return_if_fail (color != NULL);

	cairo_set_source_rgba (cr,
	                       color->red / 65535.,
	                       color->green / 65535.,
	                       color->blue / 65535.,
	                       alpha);
}

static void
murrine_style_draw_focus (GtkStyle *style, GdkWindow *window, GtkStateType state_type,
                          GdkRectangle *area, GtkWidget *widget, const gchar *detail,
                          gint x, gint y, gint width, gint height)
{
	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = gdk_cairo_create (window);

	gboolean free_dash_list = FALSE;
	gint line_width = 1;
	gint8 *dash_list = "\1\1";

	if (widget)
	{
		gtk_widget_style_get (widget,
		                     "focus-line-width", &line_width,
		                     "focus-line-pattern",
		                     (gchar *) & dash_list, NULL);

		free_dash_list = TRUE;
	}

	if (detail && !strcmp (detail, "add-mode"))
	{
		if (free_dash_list)
			g_free (dash_list);

		dash_list = "\4\4";
		free_dash_list = FALSE;
	}

	if (detail && !strcmp (detail, "colorwheel_light"))
		cairo_set_source_rgb (cr, 0., 0., 0.);
	else if (detail && !strcmp (detail, "colorwheel_dark"))
		cairo_set_source_rgb (cr, 1., 1., 1.);
	else
		gdk_cairo_set_source_color_alpha (cr, &style->fg[state_type], 0.7);

	cairo_set_line_width (cr, line_width);

	if (dash_list[0])
	{
		gint n_dashes = strlen (dash_list);
		gdouble *dashes = g_new (gdouble, n_dashes);
		gdouble total_length = 0;
		gdouble dash_offset;
		gint i;

		for (i = 0; i < n_dashes; i++)
		{
			dashes[i] = dash_list[i];
			total_length += dash_list[i];
		}

		dash_offset = -line_width / 2.;
		while (dash_offset < 0)
			dash_offset += total_length;

		cairo_set_dash (cr, dashes, n_dashes, dash_offset);
		g_free (dashes);
	}

	if (area)
	{
		gdk_cairo_rectangle (cr, area);
		cairo_clip (cr);
	}

	cairo_rectangle (cr, x + line_width / 2., y + line_width / 2., width - line_width, height - line_width);
	cairo_stroke (cr);
	cairo_destroy (cr);

	if (free_dash_list)
		g_free (dash_list);
}

static void
murrine_style_copy (GtkStyle * style, GtkStyle * src)
{
	MurrineStyle * mrn_style = MURRINE_STYLE (style);
	MurrineStyle * mrn_src = MURRINE_STYLE (src);

	mrn_style->animation           = mrn_src->animation;
	mrn_style->colorize_scrollbar  = mrn_src->colorize_scrollbar;
	mrn_style->colors              = mrn_src->colors;
	mrn_style->glazestyle          = mrn_src->glazestyle;
	mrn_style->gradient_stop_1     = mrn_src->gradient_stop_1;
	mrn_style->gradient_stop_2     = mrn_src->gradient_stop_2;
	mrn_style->gradient_stop_3     = mrn_src->gradient_stop_3;
	mrn_style->gradient_stop_4     = mrn_src->gradient_stop_4;
	mrn_style->gradients           = mrn_src->gradients;
	mrn_style->has_gradient_stop   = mrn_src->has_gradient_stop;
	mrn_style->has_scrollbar_color = mrn_src->has_scrollbar_color;
	mrn_style->highlight_ratio     = mrn_src->highlight_ratio;
	mrn_style->innerborder_ratio   = mrn_src->innerborder_ratio;
	mrn_style->listviewheaderstyle = mrn_src->listviewheaderstyle;
	mrn_style->listviewstyle       = mrn_src->listviewstyle;
	mrn_style->menubaritemstyle    = mrn_src->menubaritemstyle;
	mrn_style->menubarstyle        = mrn_src->menubarstyle;
	mrn_style->menuitemstyle       = mrn_src->menuitemstyle;
	mrn_style->menustyle           = mrn_src->menustyle;
	mrn_style->rgba                = mrn_src->rgba;
	mrn_style->roundness           = mrn_src->roundness;
	mrn_style->scrollbar_color     = mrn_src->scrollbar_color;
	mrn_style->scrollbarstyle      = mrn_src->scrollbarstyle;
	mrn_style->sliderstyle 	       = mrn_src->sliderstyle;
	mrn_style->stepperstyle        = mrn_src->stepperstyle;
	mrn_style->style               = mrn_src->style;
	mrn_style->toolbarstyle        = mrn_src->toolbarstyle;

	murrine_parent_class->copy (style, src);
}

static void
murrine_style_unrealize (GtkStyle * style)
{
	murrine_parent_class->unrealize (style);
}

static GdkPixbuf *
set_transparency (const GdkPixbuf *pixbuf, gdouble alpha_percent)
{
	GdkPixbuf *target;
	guchar *data, *current;
	guint x, y, rowstride, height, width;

	g_return_val_if_fail (pixbuf != NULL, NULL);
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	/* Returns a copy of pixbuf with it's non-completely-transparent pixels to
	   have an alpha level "alpha_percent" of their original value. */

	target = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);

	if (alpha_percent == 1.0)
		return target;
	width = gdk_pixbuf_get_width (target);
	height = gdk_pixbuf_get_height (target);
	rowstride = gdk_pixbuf_get_rowstride (target);
	data = gdk_pixbuf_get_pixels (target);

	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			/* The "4" is the number of chars per pixel, in this case, RGBA,
			   the 3 means "skip to the alpha" */
			current = data + (y * rowstride) + (x * 4) + 3;
			*(current) = (guchar) (*(current) * alpha_percent);
		}
	}

	return target;
}

static GdkPixbuf*
scale_or_ref (GdkPixbuf *src,
              int       width,
              int       height)
{
	if (width == gdk_pixbuf_get_width (src) &&
	    height == gdk_pixbuf_get_height (src))
	{
		return g_object_ref (src);
	}
	else
	{
		return gdk_pixbuf_scale_simple (src, width, height, GDK_INTERP_BILINEAR);
	}
}

static GdkPixbuf *
murrine_style_draw_render_icon (GtkStyle            *style,
                                const GtkIconSource *source,
                                GtkTextDirection    direction,
                                GtkStateType        state,
                                GtkIconSize         size,
                                GtkWidget           *widget,
                                const char          *detail)
{
	int width = 1;
	int height = 1;
	GdkPixbuf *scaled;
	GdkPixbuf *stated;
	GdkPixbuf *base_pixbuf;
	GdkScreen *screen;
	GtkSettings *settings;

	/* Oddly, style can be NULL in this function, because
	 * GtkIconSet can be used without a style and if so
	 * it uses this function.
	 */

	base_pixbuf = gtk_icon_source_get_pixbuf (source);

	g_return_val_if_fail (base_pixbuf != NULL, NULL);

	if (widget && gtk_widget_has_screen (widget))
	{
		screen = gtk_widget_get_screen (widget);
		settings = gtk_settings_get_for_screen (screen);
	}
	else if (style->colormap)
	{
		screen = gdk_colormap_get_screen (style->colormap);
		settings = gtk_settings_get_for_screen (screen);
	}
	else
	{
		settings = gtk_settings_get_default ();
		GTK_NOTE (MULTIHEAD,
		g_warning ("Using the default screen for gtk_default_murrine_style_draw_render_icon()"));
	}

	if (size != (GtkIconSize) -1 && !gtk_icon_size_lookup_for_settings (settings, size, &width, &height))
	{
		g_warning (G_STRLOC ": invalid icon size '%d'", size);
		return NULL;
	}

	/* If the size was wildcarded, and we're allowed to scale, then scale; otherwise,
	 * leave it alone.
	 */
	if (size != (GtkIconSize)-1 && gtk_icon_source_get_size_wildcarded (source))
		scaled = scale_or_ref (base_pixbuf, width, height);
	else
		scaled = g_object_ref (base_pixbuf);

	/* If the state was wildcarded, then generate a state. */
	if (gtk_icon_source_get_state_wildcarded (source))
	{
		if (state == GTK_STATE_INSENSITIVE)
		{
			stated = set_transparency (scaled, 0.3);
#if 0
			stated =
				gdk_pixbuf_composite_color_simple (scaled,
				                                   gdk_pixbuf_get_width (scaled),
				                                   gdk_pixbuf_get_height (scaled),
				                                   GDK_INTERP_BILINEAR, 128,
				                                   gdk_pixbuf_get_width (scaled),
				                                   style->bg[state].pixel,
				                                   style->bg[state].pixel);
#endif
			gdk_pixbuf_saturate_and_pixelate (stated, stated, 0.1, FALSE);

			g_object_unref (scaled);
		}
		else if (state == GTK_STATE_PRELIGHT)
		{
			stated = gdk_pixbuf_copy (scaled);

			gdk_pixbuf_saturate_and_pixelate (scaled, stated, 1.2, FALSE);

			g_object_unref (scaled);
		}
		else
		{
			stated = scaled;
		}
	}
	else
		stated = scaled;

	return stated;
}

static void
murrine_style_init (MurrineStyle * style)
{
}

static void
murrine_style_class_init (MurrineStyleClass * klass)
{
	GtkStyleClass *style_class = GTK_STYLE_CLASS (klass);

	murrine_style_class = MURRINE_STYLE_CLASS (klass);
	murrine_parent_class = g_type_class_peek_parent (klass);

	style_class->copy             = murrine_style_copy;
	style_class->realize          = murrine_style_realize;
	style_class->unrealize        = murrine_style_unrealize;
	style_class->init_from_rc     = murrine_style_init_from_rc;
	style_class->draw_handle      = murrine_style_draw_handle;
	style_class->draw_slider      = murrine_style_draw_slider;
	style_class->draw_shadow_gap  = murrine_style_draw_shadow_gap;
	style_class->draw_focus       = murrine_style_draw_focus;
	style_class->draw_box         = murrine_style_draw_box;
	style_class->draw_shadow      = murrine_style_draw_shadow;
	style_class->draw_box_gap     = murrine_style_draw_box_gap;
	style_class->draw_extension   = murrine_style_draw_extension;
	style_class->draw_option      = murrine_style_draw_option;
	style_class->draw_check       = murrine_style_draw_check;
	style_class->draw_flat_box    = murrine_style_draw_flat_box;
	style_class->draw_tab         = murrine_style_draw_tab;
	style_class->draw_vline       = murrine_style_draw_vline;
	style_class->draw_hline       = murrine_style_draw_hline;
	style_class->draw_resize_grip = murrine_style_draw_resize_grip;
	style_class->draw_arrow       = murrine_style_draw_arrow;
	style_class->draw_layout      = murrine_style_draw_layout;
	style_class->render_icon      = murrine_style_draw_render_icon;

	murrine_register_style_murrine (&murrine_style_class->style_functions[MRN_DRAW_STYLE_MURRINE]);
	murrine_style_class->style_functions[MRN_DRAW_STYLE_RGBA] = murrine_style_class->style_functions[MRN_DRAW_STYLE_MURRINE];
	murrine_register_style_rgba (&murrine_style_class->style_functions[MRN_DRAW_STYLE_RGBA]);
}

GType murrine_type_style = 0;

void
murrine_style_register_type (GTypeModule * module)
{
	static const GTypeInfo object_info =
	{
		sizeof (MurrineStyleClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) murrine_style_class_init,
		NULL, /* class_finalize */
		NULL, /* class_data */
		sizeof (MurrineStyle),
		0, /* n_preallocs */
		(GInstanceInitFunc) murrine_style_init,
		NULL
	};

	murrine_type_style = g_type_module_register_type (module, GTK_TYPE_STYLE, "MurrineStyle", &object_info, 0);
}