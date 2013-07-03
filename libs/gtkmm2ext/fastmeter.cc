/*
    Copyright (C) 2003-2006 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>

#include <gdkmm/rectangle.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/utils.h>

#define UINT_TO_RGB(u,r,g,b) { (*(r)) = ((u)>>16)&0xff; (*(g)) = ((u)>>8)&0xff; (*(b)) = (u)&0xff; }
#define UINT_TO_RGBA(u,r,g,b,a) { UINT_TO_RGB(((u)>>8),r,g,b); (*(a)) = (u)&0xff; }
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;

int FastMeter::min_pattern_metric_size = 10;
int FastMeter::max_pattern_metric_size = 1024;

FastMeter::Pattern10Map FastMeter::vm_pattern_cache;
FastMeter::PatternBgMap FastMeter::vb_pattern_cache;

FastMeter::FastMeter (long hold, unsigned long dimen, Orientation o, int len,
		int clr0, int clr1, int clr2, int clr3,
		int clr4, int clr5, int clr6, int clr7,
		int clr8, int clr9, int bgc0, int bgc1,
		float stp0, float stp1,
		float stp2, float stp3
		)
{
	orientation = o;
	hold_cnt = hold;
	resized = true;
	hold_state = 0;
	current_peak = 0;
	current_level = 0;
	last_peak_rect.width = 0;
	last_peak_rect.height = 0;

	_clr[0] = clr0;
	_clr[1] = clr1;
	_clr[2] = clr2;
	_clr[3] = clr3;
	_clr[4] = clr4;
	_clr[5] = clr5;
	_clr[6] = clr6;
	_clr[7] = clr7;
	_clr[8] = clr8;
	_clr[9] = clr9;

	_bgc[0] = bgc0;
	_bgc[1] = bgc1;

	_stp[0] = stp0;
	_stp[1] = stp1;
	_stp[2] = stp2;
	_stp[3] = stp3;

	set_events (BUTTON_PRESS_MASK|BUTTON_RELEASE_MASK);

	pixrect.x = 1;
	pixrect.y = 1;

	if (!len) {
		len = 250;
	}
	fgpattern = request_vertical_meter(dimen, len, _clr, _stp);
	bgpattern = request_vertical_background (dimen, len, _bgc);
	pixheight = len;
	pixwidth = dimen;

	pixrect.width = pixwidth;
	pixrect.height = pixheight;

	request_width = pixrect.width + 2;
	request_height= pixrect.height + 2;
}

FastMeter::~FastMeter ()
{
}

Cairo::RefPtr<Cairo::Pattern>
FastMeter::generate_meter_pattern (
		int width, int height, int *clr, float *stp)
{
	guint8 r,g,b,a;
	int knee;

	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, width, height);

	/*
	  Cairo coordinate space goes downwards as y value goes up, so invert
	  knee-based positions by using (1.0 - y)
	*/

	UINT_TO_RGBA (clr[9], &r, &g, &b, &a); // top/clip
	cairo_pattern_add_color_stop_rgb (pat, 0.0,
	                                  r/255.0, g/255.0, b/255.0);

	knee = (int)floor((float)height * stp[3] / 115.0f); // -0dB

	UINT_TO_RGBA (clr[8], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - (knee/(double)height),
	                                  r/255.0, g/255.0, b/255.0);

	UINT_TO_RGBA (clr[7], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - (knee/(double)height),
	                                  r/255.0, g/255.0, b/255.0);

	knee = (int)floor((float)height * stp[2]/ 115.0f); // -3dB || -2dB

	UINT_TO_RGBA (clr[6], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - (knee/(double)height),
	                                  r/255.0, g/255.0, b/255.0);

	UINT_TO_RGBA (clr[5], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - (knee/(double)height),
	                                  r/255.0, g/255.0, b/255.0);

	knee = (int)floor((float)height * stp[1] / 115.0f); // -9dB

	UINT_TO_RGBA (clr[4], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - (knee/(double)height),
	                                  r/255.0, g/255.0, b/255.0);

	UINT_TO_RGBA (clr[3], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - (knee/(double)height),
	                                  r/255.0, g/255.0, b/255.0);

	knee = (int)floor((float)height * stp[0] / 115.0f); // -18dB

	UINT_TO_RGBA (clr[2], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - (knee/(double)height),
	                                  r/255.0, g/255.0, b/255.0);

	UINT_TO_RGBA (clr[1], &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - (knee/(double)height),
	                                  r/255.0, g/255.0, b/255.0);

	UINT_TO_RGBA (clr[0], &r, &g, &b, &a); // bottom
	cairo_pattern_add_color_stop_rgb (pat, 1.0,
	                                  r/255.0, g/255.0, b/255.0);

	Cairo::RefPtr<Cairo::Pattern> p (new Cairo::Pattern (pat, false));

	return p;
}


Cairo::RefPtr<Cairo::Pattern>
FastMeter::generate_meter_background (
		int width, int height, int *clr)
{
	guint8 r0,g0,b0,r1,g1,b1,a;

	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, width, height);

	UINT_TO_RGBA (clr[0], &r0, &g0, &b0, &a);
	UINT_TO_RGBA (clr[1], &r1, &g1, &b1, &a);

	cairo_pattern_add_color_stop_rgb (pat, 0.0,
	                                  r1/255.0, g1/255.0, b1/255.0);

	cairo_pattern_add_color_stop_rgb (pat, 1.0,
	                                  r0/255.0, g0/255.0, b0/255.0);

	Cairo::RefPtr<Cairo::Pattern> p (new Cairo::Pattern (pat, false));

	return p;
}

Cairo::RefPtr<Cairo::Pattern>
FastMeter::request_vertical_meter(
		int width, int height, int *clr, float *stp)
{
	if (height < min_pattern_metric_size)
		height = min_pattern_metric_size;
	if (height > max_pattern_metric_size)
		height = max_pattern_metric_size;

	const Pattern10MapKey key (width, height,
			stp[0], stp[1], stp[2], stp[3],
			clr[0], clr[1], clr[2], clr[3],
			clr[4], clr[5], clr[6], clr[7],
			clr[8], clr[9]);
	Pattern10Map::iterator i;
	if ((i = vm_pattern_cache.find (key)) != vm_pattern_cache.end()) {
		return i->second;
	}

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_pattern (
		width, height, clr, stp);
	vm_pattern_cache[key] = p;

	return p;
}

Cairo::RefPtr<Cairo::Pattern>
FastMeter::request_vertical_background(
		int width, int height, int *bgc)
{
	if (height < min_pattern_metric_size)
		height = min_pattern_metric_size;
	if (height > max_pattern_metric_size)
		height = max_pattern_metric_size;

	const PatternBgMapKey key (width, height, bgc[0], bgc[1]);
	PatternBgMap::iterator i;
	if ((i = vb_pattern_cache.find (key)) != vb_pattern_cache.end()) {
		return i->second;
	}

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_background (
		width, height, bgc);
	vb_pattern_cache[key] = p;

	return p;
}


void
FastMeter::set_hold_count (long val)
{
	if (val < 1) {
		val = 1;
	}

	hold_cnt = val;
	hold_state = 0;
	current_peak = 0;

	queue_draw ();
}

void
FastMeter::on_size_request (GtkRequisition* req)
{
	req->height = request_height;
	req->height = max(req->height, min_pattern_metric_size);
	req->height = min(req->height, max_pattern_metric_size);
	req->height += 2;

	req->width  = request_width;
}

void
FastMeter::on_size_allocate (Gtk::Allocation &alloc)
{
	if (alloc.get_width() != request_width) {
		alloc.set_width (request_width);
	}

	int h = alloc.get_height();
	h = max (h, min_pattern_metric_size + 2);
	h = min (h, max_pattern_metric_size + 2);

	if (h != alloc.get_height()) {
		alloc.set_height (h);
	}

	if (pixheight != h) {
		fgpattern = request_vertical_meter (request_width, h, _clr, _stp);
		bgpattern = request_vertical_background (request_width, h, _bgc);
		pixheight = h - 2;
		pixwidth  = request_width - 2;
	}

	DrawingArea::on_size_allocate (alloc);
	resized = true;
}

bool
FastMeter::on_expose_event (GdkEventExpose* ev)
{
	return vertical_expose (ev);
}

bool
FastMeter::vertical_expose (GdkEventExpose* ev)
{
	Glib::RefPtr<Gdk::Window> win = get_window ();
	gint top_of_meter;
	GdkRectangle intersection;
	GdkRectangle background;

	cairo_t* cr = gdk_cairo_create (get_window ()->gobj());

	if (resized) {
		cairo_set_source_rgb (cr, 0, 0, 0); // black
		rounded_rectangle (cr, 0, 0, pixrect.width + 2, pixheight + 2, 2);
		cairo_fill (cr);
	}

	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	top_of_meter = (gint) floor (pixheight * current_level);

	/* reset the height & origin of the rect that needs to show the pixbuf
	 */

	pixrect.height = top_of_meter;
	pixrect.y = 1 + pixheight - top_of_meter;

	background.x = 1;
	background.y = 1;
	background.width = pixrect.width;
	background.height = pixheight - top_of_meter;

	if (gdk_rectangle_intersect (&background, &ev->area, &intersection)) {
		cairo_set_source (cr, bgpattern->cobj());
		cairo_rectangle (cr, intersection.x, intersection.y, intersection.width, intersection.height);
		cairo_fill (cr);
	}

	if (gdk_rectangle_intersect (&pixrect, &ev->area, &intersection)) {
		// draw the part of the meter image that we need. the area we draw is bounded "in reverse" (top->bottom)
		cairo_set_source (cr, fgpattern->cobj());
		cairo_rectangle (cr, intersection.x, intersection.y, intersection.width, intersection.height);
		cairo_fill (cr);
	}

	// draw peak bar

	if (hold_state) {
		last_peak_rect.x = 1;
		last_peak_rect.width = pixwidth;
		last_peak_rect.y = 1 + pixheight - (gint) floor (pixheight * current_peak);
		last_peak_rect.height = min(2, pixheight - last_peak_rect.y);

		cairo_set_source (cr, fgpattern->cobj());
		cairo_rectangle (cr, 1, last_peak_rect.y, pixwidth, last_peak_rect.height);
		cairo_fill (cr);

	} else {
		last_peak_rect.width = 0;
		last_peak_rect.height = 0;
	}

	cairo_destroy (cr);

	return TRUE;
}

void
FastMeter::set (float lvl)
{
	float old_level = current_level;
	float old_peak = current_peak;

	current_level = lvl;

	if (lvl > current_peak) {
		current_peak = lvl;
		hold_state = hold_cnt;
	}

	if (hold_state > 0) {
		if (--hold_state == 0) {
			current_peak = lvl;
		}
	}

	if (current_level == old_level && current_peak == old_peak && hold_state == 0) {
		return;
	}


	Glib::RefPtr<Gdk::Window> win;

	if ((win = get_window()) == 0) {
		queue_draw ();
		return;
	}

	queue_vertical_redraw (win, old_level);
}

void
FastMeter::queue_vertical_redraw (const Glib::RefPtr<Gdk::Window>& win, float old_level)
{
	GdkRectangle rect;

	gint new_top = (gint) floor (pixheight * current_level);

	rect.x = 1;
	rect.width = pixwidth;
	rect.height = new_top;
	rect.y = 1 + pixheight - new_top;

	if (current_level > old_level) {
		/* colored/pixbuf got larger, just draw the new section */
		/* rect.y stays where it is because of X coordinates */
		/* height of invalidated area is between new.y (smaller) and old.y
		   (larger).
		   X coordinates just make my brain hurt.
		*/
		rect.height = pixrect.y - rect.y;
	} else {
		/* it got smaller, compute the difference */
		/* rect.y becomes old.y (the smaller value) */
		rect.y = pixrect.y;
		/* rect.height is the old.y (smaller) minus the new.y (larger)
		*/
		rect.height = pixrect.height - rect.height;
	}

	GdkRegion* region = 0;
	bool queue = false;

	if (rect.height != 0) {

		/* ok, first region to draw ... */

		region = gdk_region_rectangle (&rect);
		queue = true;
	}

	/* redraw the last place where the last peak hold bar was;
	   the next expose will draw the new one whether its part of
	   expose region or not.
	*/

	if (last_peak_rect.width * last_peak_rect.height != 0) {
		if (!queue) {
			region = gdk_region_new ();
			queue = true;
		}
		gdk_region_union_with_rect (region, &last_peak_rect);
	}

	if (queue) {
		gdk_window_invalidate_region (win->gobj(), region, true);
	}
	if (region) {
		gdk_region_destroy(region);
		region = 0;
	}
}

void
FastMeter::clear ()
{
	current_level = 0;
	current_peak = 0;
	hold_state = 0;
	queue_draw ();
}
