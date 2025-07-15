/**
 * \file MathRow.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jean-Marc Lasgouttes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "MathRow.h"

#include "InsetMathChar.h"
#include "MathData.h"
#include "MathSupport.h"

#include "BufferView.h"
#include "ColorSet.h"
#include "CoordCache.h"
#include "Cursor.h"
#include "MetricsInfo.h"

#include "mathed/InsetMath.h"

#include "frontends/FontMetrics.h"
#include "frontends/Painter.h"

#include "support/debug.h"
#include "support/docstring.h"
#include "support/lassert.h"

#include <algorithm>
#include <ostream>

using namespace std;

namespace lyx {


MathRow::Element::Element(MetricsInfo const & mi, Type t, MathClass mc)
	: type(t), mclass(mc), before(0), after(0), macro_nesting(mi.base.macro_nesting),
	  marker(marker_type::NO_MARKER), inset(nullptr), compl_unique_to(0), md(nullptr),
	  color(Color_red)
{}


namespace {

// Helper functions for markers

int markerMargin(MathRow::Element const & e)
{
	switch(e.marker) {
	case marker_type::MARKER:
	case marker_type::MARKER2:
	case marker_type::BOX_MARKER:
		return 2;
	case marker_type::NO_MARKER:
		return 0;
	}
	// should not happen
	return 0;
}


void afterMetricsMarkers(MetricsInfo const & , MathRow::Element & e,
                            Dimension & dim)
{
	// handle vertical space for markers
	switch(e.marker) {
	case marker_type::NO_MARKER:
		break;
	case marker_type::MARKER:
		++dim.des;
		break;
	case marker_type::MARKER2:
		++dim.asc;
		++dim.des;
		break;
	case marker_type::BOX_MARKER:
		FontInfo font;
		font.setSize(TINY_SIZE);
		Dimension namedim;
		mathed_string_dim(font, e.inset->name(), namedim);
		int const namewid = 1 + namedim.wid + 1;

		if (namewid > dim.wid)
			e.after += namewid - dim.wid;
		++dim.asc;
		dim.des += 3 + namedim.height();
	}
}


void drawMarkers(PainterInfo const & pi, MathRow::Element const & e,
                 int const x, int const y)
{
	if (e.marker == marker_type::NO_MARKER)
		return;

	CoordCache const & coords = pi.base.bv->coordCache();
	Dimension const dim = coords.insets().dim(e.inset);

	// the marker is before/after the inset. Necessary space has been reserved already.
	int const l = x + e.before - (markerMargin(e) > 0 ? 1 : 0);
	int const r = x + dim.width() - e.after;

	// Grey lower box
	if (e.marker == marker_type::BOX_MARKER) {
		// draw header and rectangle around
		FontInfo font;
		font.setSize(TINY_SIZE);
		font.setColor(Color_mathmacrolabel);
		Dimension namedim;
		mathed_string_dim(font, e.inset->name(), namedim);
		pi.pain.fillRectangle(l, y + dim.des - namedim.height() - 2,
		                      dim.wid, namedim.height() + 2, Color_mathmacrobg);
		pi.pain.text(l, y + dim.des - namedim.des - 1, e.inset->name(), font);
	}

	// Color for corners
	bool const highlight = e.inset->mouseHovered(pi.base.bv)
	                       || e.inset->editing(pi.base.bv);
	ColorCode const pen_color = highlight ? Color_mathframe : Color_mathcorners;
	// If the corners have the same color as the background, do not paint them.
	if (lcolor.getX11HexName(Color_mathbg) == lcolor.getX11HexName(pen_color))
		return;

	// Lower corners in all cases
	int const d = y + dim.descent();
	pi.pain.line(l, d - 3, l, d, pen_color);
	pi.pain.line(r, d - 3, r, d, pen_color);
	pi.pain.line(l, d, l + 3, d, pen_color);
	pi.pain.line(r - 3, d, r, d, pen_color);

	// Upper corners
	if (e.marker == marker_type::BOX_MARKER
	    || e.marker == marker_type::MARKER2) {
		int const a = y - dim.ascent();
		pi.pain.line(l, a + 3, l, a, pen_color);
		pi.pain.line(r, a + 3, r, a, pen_color);
		pi.pain.line(l, a, l + 3, a, pen_color);
		pi.pain.line(r - 3, a, r, a, pen_color);
	}
}

} // namespace


MathRow::MathRow(MetricsInfo & mi, MathData const * md)
{
	// First there is a dummy element of type "open"
	push_back(Element(mi, DUMMY, MC_OPEN));

	// Then insert the MathData argument
	bool const has_contents = md->addToMathRow(*this, mi);

	// A MathRow should not be completely empty
	if (!has_contents) {
		Element e(mi, BOX, MC_ORD);
		// empty cells are visible when they are editable
		e.color = mi.base.macro_nesting == 0 ? Color_mathline : Color_none;
		push_back(e);
	}

	// Finally there is a dummy element of type "close"
	push_back(Element(mi, DUMMY, MC_CLOSE));

	/* Do spacing only in math mode. This test is a bit clumsy,
	 * but it is used in other places for guessing the current mode.
	 */
	bool const dospacing = isMathFont(mi.base.fontname);

	// update classes
	if (dospacing) {
		for (int i = 1 ; i != static_cast<int>(elements_.size()) - 1 ; ++i) {
			if (elements_[i].mclass != MC_UNKNOWN)
				update_class(elements_[i].mclass, elements_[before(i)].mclass,
							 elements_[after(i)].mclass);
		}
	}

	// set spacing
	// We go to the end to handle spacing at the end of equation
	for (int i = 1 ; i != static_cast<int>(elements_.size()) ; ++i) {
		Element & e = elements_[i];
		Element & bef = elements_[before(i)];

		if (dospacing && e.mclass != MC_UNKNOWN) {
			int spc = class_spacing(bef.mclass, e.mclass, mi.base);
			bef.after += spc / 2;
			// this is better than spc / 2 to avoid rounding problems
			e.before += spc - spc / 2;
		}

		// finally reserve space for markers
		/* FIXME : the test below avoids that the spacing grows grows
		 * when a BEGIN/END_SEL element is added (typically start or
		 * end a selection after a fraction). I would think that the
		 * code should just go under the e.mclass != MC_UNKNOWN branch
		 * below, but I am not sure why it has not been done before.
		 * Therefore, until we double check this, be very conservative
		 */
		if (e.type != BEGIN_SEL && e.type != END_SEL)
			bef.after = max(bef.after, markerMargin(bef));
		if (e.mclass != MC_UNKNOWN)
			e.before = max(e.before, markerMargin(e));
		// for linearized insets (macros...) too
		if (e.type == BEGIN)
			bef.after = max(bef.after, markerMargin(e));
		if (e.type == END && e.marker != marker_type::NO_MARKER) {
			Element & aft = elements_[after(i)];
			aft.before = max(aft.before, markerMargin(e));
		}
	}

	// Do not lose spacing allocated to extremities
	if (!elements_.empty()) {
		elements_[after(0)].before += elements_.front().after;
		elements_[before(elements_.size() - 1)].after += elements_.back().before;
	}
}


int MathRow::before(int i) const
{
	do
		--i;
	while (elements_[i].mclass == MC_UNKNOWN);

	return i;
}


int MathRow::after(int i) const
{
	do
		++i;
	while (elements_[i].mclass == MC_UNKNOWN);

	return i;
}


void MathRow::metrics(MetricsInfo & mi, Dimension & dim)
{
	dim.wid = 0;
	// In order to compute the dimension of macros and their
	// arguments, it is necessary to keep track of them.
	vector<pair<InsetMath const *, Dimension>> dim_insets;
	vector<pair<MathData const *, Dimension>> dim_cells;
	CoordCache & coords = mi.base.bv->coordCache();
	for (Element & e : elements_) {
		mi.base.macro_nesting = e.macro_nesting;
		Dimension d;
		switch (e.type) {
		case DUMMY:
		case BEGIN_SEL:
		case END_SEL:
			break;
		case INSET:
			e.inset->metrics(mi, d);
			d.wid += e.before + e.after;
			coords.insets().add(e.inset, d);
			break;
		case BEGIN:
			if (e.inset) {
				dim_insets.push_back(make_pair(e.inset, Dimension()));
				dim_insets.back().second.wid += e.before + e.after;
				d.wid = e.before + e.after;
				e.inset->beforeMetrics();
			}
			if (e.md)
				dim_cells.push_back(make_pair(e.md, Dimension()));
			break;
		case END:
			if (e.inset) {
				e.inset->afterMetrics();
				LATTEST(dim_insets.back().first == e.inset);
				d = dim_insets.back().second;
				afterMetricsMarkers(mi, e, d);
				d.wid += e.before + e.after;
				coords.insets().add(e.inset, d);
				dim_insets.pop_back();
				// We do not want to count the width again, but the
				// padding and the vertical dimension are meaningful.
				d.wid = e.before + e.after;
			}
			if (e.md) {
				LATTEST(dim_cells.back().first == e.md);
				coords.cells().add(e.md, dim_cells.back().second);
				dim_cells.pop_back();
			}
			break;
		case BOX:
			d = theFontMetrics(mi.base.font).dimension('I');
			if (e.color != Color_none) {
				// allow for one pixel before/after the box.
				d.wid += e.before + e.after + 2;
			} else {
				// hide the box, but keep its height
				d.wid = 0;
			}
			break;
		}

		if (!d.empty()) {
			dim += d;
			// Now add the dimension to current macros and arguments.
			for (auto & dim_macro : dim_insets)
				dim_macro.second += d;
			for (auto & dim_cell : dim_cells)
				dim_cell.second += d;
		}

		if (e.compl_text.empty())
			continue;
		FontInfo font = mi.base.font;
		augmentFont(font, "mathnormal");
		dim.wid += mathed_string_width(font, e.compl_text);
	}
	LATTEST(dim_insets.empty() && dim_cells.empty());
}


void MathRow::draw(PainterInfo & pi, int x, int const y) const
{
	Changer change_color;
	CoordCache & coords = pi.base.bv->coordCache();
	for (Element const & e : elements_) {
		switch (e.type) {
		case INSET: {
			// This is hackish: the math inset does not know that space
			// has been added before and after it; we alter its dimension
			// while it is drawing, because it relies on this value.
			Geometry & g = coords.insets().geometry(e.inset);
			g.dim.wid -= e.before + e.after;
			if (pi.pain.develMode() && !e.inset->isBufferValid()) {
				pi.pain.fillRectangle(x + e.before, y - g.dim.ascent(),
				                      g.dim.width(), g.dim.height(), Color_error);
				LYXERR0("Unset Buffer member in " << insetName(e.inset->lyxCode()));
			}
			if (e.im == nullptr)
				e.inset->draw(pi, x + e.before, y);
			else
				e.inset->asInsetMath()->asCharInset()
				       ->drawWithInputMethod(pi, x + e.before, y, e.im,
				                             e.char_format_index);
			g.pos = {x, y};
			g.dim.wid += e.before + e.after;
			drawMarkers(pi, e, x, y);
			x += g.dim.wid;
			break;
		}
		case BEGIN:
			if (e.md) {
				coords.cells().add(e.md, x, y);
				e.md->drawSelection(pi, x, y);
			}
			if (e.inset) {
				coords.insets().add(e.inset, x, y);
				drawMarkers(pi, e, x, y);
				e.inset->beforeDraw(pi);
			}
			x += e.before + e.after;
			break;
		case END:
			if (e.inset)
				e.inset->afterDraw(pi);
			x += e.before + e.after;
			break;
		case BEGIN_SEL:
			// Change the color if the selection is indeed active
			// FIXME: it would be better to make sure that metrics are
			//   computed again when selection status changes.
			if (pi.base.bv->cursor().selection())
				change_color = pi.base.font.changeColor(Color_selectionmath);
			break;
		case END_SEL:
			change_color = noChange();
			break;
		case BOX: {
			if (e.color == Color_none)
				break;
			Dimension const d = theFontMetrics(pi.base.font).dimension('I');
			pi.pain.rectangle(x + e.before + 1, y - d.ascent(),
			                  d.width() - 1, d.height() - 1, e.color);
			x += d.wid + 2 + e.before + e.after;
			break;
		}
		case DUMMY:
			break;
		}

		if (e.compl_text.empty())
			continue;
		FontInfo f = pi.base.font;
		augmentFont(f, "mathnormal");

		// draw the unique and the non-unique completion part
		// Note: this is not time-critical as it is
		// only done once per screen.
		docstring const s1 = e.compl_text.substr(0, e.compl_unique_to);
		docstring const s2 = e.compl_text.substr(e.compl_unique_to);

		if (!s1.empty()) {
			f.setColor(Color_inlinecompletion);
			// offset the text by e.after to make sure that the
			// spacing is after the completion, not before.
			pi.pain.text(x - e.after, y, s1, f);
			x += mathed_string_width(f, s1);
		}
		if (!s2.empty()) {
			f.setColor(Color_nonunique_inlinecompletion);
			pi.pain.text(x - e.after, y, s2, f);
			x += mathed_string_width(f, s2);
		}
	}
}


int MathRow::kerning(BufferView const * bv) const
{
	if (elements_.empty())
		return 0;
	InsetMath const * inset = elements_[before(elements_.size() - 1)].inset;
	return inset ? inset->kerning(bv) : 0;
}


ostream & operator<<(ostream & os, MathRow::Element const & e)
{
	switch (e.type) {
	case MathRow::DUMMY:
		os << (e.mclass == MC_OPEN ? "{" : "}");
		break;
	case MathRow::INSET:
		os << "<" << e.before << "-"
		   << to_utf8(class_to_string(e.mclass))
		   << "-" << e.after << ">";
		break;
	case MathRow::BEGIN:
		if (e.inset)
			os << "\\" << to_utf8(e.inset->name())
			   << "^" << e.macro_nesting << "[";
		if (e.md)
			os << "(";
		break;
	case MathRow::END:
		if (e.md)
			os << ")";
		if (e.inset)
			os << "]";
		break;
	case MathRow::BEGIN_SEL:
		os << "<sel>";
		break;
	case MathRow::END_SEL:
		os << "</sel>" ;
		break;
	case MathRow::BOX:
		os << "<" << e.before << "-[]-" << e.after << ">";
		break;
	}
	return os;
}


ostream & operator<<(ostream & os, MathRow const & mrow)
{
	for (MathRow::Element const & e : mrow)
		os << e << "  ";
	return os;
}

} // namespace lyx
