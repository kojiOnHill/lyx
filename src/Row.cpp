/**
 * \file Row.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author John Levon
 * \author André Pönitz
 * \author Jürgen Vigna
 * \author Jean-Marc Lasgouttes
 *
 * Full author contact details are available in file CREDITS.
 *
 * Metrics for an on-screen text row.
 */

#include <config.h>

#include "Row.h"

#include "DocIterator.h"
#include "Language.h"

#include "frontends/FontMetrics.h"
#include "frontends/InputMethod.h"

#include "support/debug.h"
#include "support/lassert.h"
#include "support/lstrings.h"
#include "support/lyxlib.h"
#include "support/textutils.h"

#include <algorithm>
#include <ostream>

using namespace std;

namespace lyx {

using frontend::FontMetrics;


// Maximum length that a space can be stretched when justifying text
static double const MAX_SPACE_STRETCH = 1.5; //em


int Row::Element::countExpanders() const
{
	if (type != STRING || font.fontInfo().family() == TYPEWRITER_FAMILY)
		return 0;
	return support::countExpanders(str);
}


int Row::Element::expansionAmount() const
{
	if (type != STRING || font.fontInfo().family() == TYPEWRITER_FAMILY)
		return 0;
	return countExpanders() * theFontMetrics(font).em();
}


void Row::Element::setExtra(double extra_per_em)
{
	if (type != STRING || font.fontInfo().family() == TYPEWRITER_FAMILY)
		return;
	extra = extra_per_em * theFontMetrics(font).em();
}


double Row::Element::pos2x(pos_type const i) const
{
	// This can happen with inline completion when clicking on the
	// row after the completion.
	if (i < pos || i > endpos)
		return 0;

	double w = 0;
	//handle first the two bounds of the element
	if (i == endpos && type != VIRTUAL && type != PREEDIT)
		w = isRTL() ? 0 : full_width();
	else if (i == pos || type != STRING)
		w = isRTL() ? full_width() : 0;
	else {
		FontMetrics const & fm = theFontMetrics(font);
		w = fm.pos2x(str, i - pos, isRTL(), extra);
	}

	return w;
}


pos_type Row::Element::x2pos(int &x) const
{
	//lyxerr << "x2pos: x=" << x << " w=" << width() << " " << *this;
	size_t i = 0;

	switch (type) {
	case STRING: {
		FontMetrics const & fm = theFontMetrics(font);
		i = fm.x2pos(str, x, isRTL(), extra);
		break;
	}
	case VIRTUAL:
	case PREEDIT:
		// those elements are actually empty (but they have a width)
		i = 0;
		x = isRTL() ? int(full_width()) : 0;
		break;
	case INSET:
	case SPACE:
	case MARGINSPACE:
		// those elements contain only one position. Round to
		// the closest side.
		if (x > (full_width() + 1) / 2) {
			x = int(full_width());
			i = !isRTL();
		} else {
			x = 0;
			i = isRTL();
		}
	}
	//lyxerr << "=> p=" << pos + i << " x=" << x << endl;
	return pos + i;
}


bool Row::Element::splitAt(int const width, int next_width, SplitType split_type,
                           Row::Elements & tail)
{
	bool const wrap_any =
	        type != PREEDIT ? !font.language()->wordWrap() : lang_wrap_any;

	// Not a string nor a preedit string, or already OK.
	if ((type != STRING && type != PREEDIT) ||
	        ((type == STRING || type == PREEDIT) && dim.wid > 0 && dim.wid < width))
		return false;

	FontMetrics const & fm = theFontMetrics(font);

	// A a string that is not breakable
	if (!(row_flags & CanBreakInside)) {
		// has width been computed yet?
		if (dim.wid == 0)
			dim = fm.dimension(str);
		return false;
	}

	FontMetrics::Breaks breaks = fm.breakString(str, width, next_width,
                                                isRTL(), wrap_any || split_type == FORCE);

	/** if breaking did not really work, give up
	 * case 1: split type is FIT and the first element is longer than the limit;
	 * case 2: the first break occurs at the front of the string
	 */
	if ((split_type == FIT && breaks.front().nspc_wid > width)
	    || (breaks.size() > 1 && breaks.front().len == 0)) {
		if (dim.wid == 0)
			dim = fm.dimension(str);
		return false;
	}

	// type is STRING or PREEDIT
	Element first_e(type, pos, font, change);

	// should next element eventually replace *this?
	bool first = true;
	docstring::size_type i = 0;
	pos_type curpos = pos;
	for (FontMetrics::Break const & brk : breaks) {
		Element e(type, curpos, font, change);
		e.str = str.substr(i, brk.len);
		e.dim = brk.dim;
		e.nspc_wid = brk.nspc_wid;
		e.row_flags = CanBreakInside | BreakAfter;
		if (type == PREEDIT) {
			e.endpos = e.pos;
			e.im = im;
			e.char_format_index = char_format_index;
		} else
			e.endpos = e.pos + brk.len;

		if (first) {
			// this element eventually goes to *this
			e.row_flags |= row_flags & ~AfterFlags;
			first_e = e;
			first = false;
		} else
			tail.push_back(e);

		curpos = e.endpos;
		i += brk.len;
	}

	if (!tail.empty()) {
		// Avoid having a last empty element. This happens when
		// breaking at the trailing space of string
		if (tail.back().str.empty())
			tail.pop_back();
		else {
			// Copy the after flags of the original element to the last one.
			tail.back().row_flags &= ~BreakAfter;
			tail.back().row_flags |= row_flags & AfterFlags;
		}
		// first_e row should be broken after the original element
		first_e.row_flags |= BreakAfter;
	} else {
#if 1
		// remove the BreakAfter that got added above.
		first_e.row_flags &= ~BreakAfter;
#else
		// FIXME : the code below looks like a good idea, but I do not
		//         have a use case yet. The question is what happens
		//         when breaking at the end of a string with a
		//         trailing space.
		// if it turns out that no breaking was necessary, remove the
		// BreakAfter that got added above.
		if (first_e.dim.wid <= width)
			first_e.row_flags &= ~BreakAfter;
#endif
		// Restore the after flags of the original element.
		first_e.row_flags |= row_flags & AfterFlags;
	}

	// update ourselves
	swap(first_e, *this);
	return true;
}


void Row::Element::rtrim()
{
	if (type != STRING || str.empty() || !isSpace(str.back()))
		return;
	/* This is intended for strings that have been created by splitAt.
	 * If There is a trailing space, we remove it and decrease endpos,
	 * since spaces at row break are invisible.
	 */
	str.pop_back();
	endpos = pos + str.length();
	dim.wid = nspc_wid;
}


bool Row::isMarginSelected(bool margin_begin, DocIterator const & beg,
		DocIterator const & end) const
{
	pos_type const sel_pos = margin_begin ? sel_beg : sel_end;
	pos_type const margin_pos = margin_begin ? pos_ : end_;

	// Is there a selection and is the chosen margin selected ?
	if (!selection() || sel_pos != margin_pos)
		return false;
	else if (beg.pos() == end.pos())
		// This is a special case in which the space between after
		// pos i-1 and before pos i is selected, i.e. the margins
		// (see DocIterator::boundary_).
		return beg.boundary() && !end.boundary();
	else if (end.pos() == margin_pos)
		// If the selection ends around the margin, it is only
		// drawn if the cursor is after the margin.
		return !end.boundary();
	else if (beg.pos() == margin_pos)
		// If the selection begins around the margin, it is
		// only drawn if the cursor is before the margin.
		return beg.boundary();
	else
		return true;
}


void Row::setSelection(pos_type beg, pos_type end) const
{
	if (pos_ >= beg && pos_ <= end)
		change(sel_beg, pos_);
	else if (beg > pos_ && beg <= end_)
		change(sel_beg, beg);
	else
		change(sel_beg, -1);

	if (end_ >= beg && end_ <= end)
		change(sel_end,end_);
	else if (end < end_ && end >= pos_)
		change(sel_end, end);
	else
		change(sel_end, -1);
}


void Row::setSelectionAndMargins(DocIterator const & beg,
		DocIterator const & end) const
{
	setSelection(beg.pos(), end.pos());

	change(end_margin_sel, isMarginSelected(false, beg, end));
	change(begin_margin_sel, isMarginSelected(true, beg, end));
}


void Row::clearSelectionAndMargins() const
{
	change(sel_beg, -1);
	change(sel_end, -1);
	change(end_margin_sel, false);
	change(begin_margin_sel, false);
}


bool Row::selection() const
{
	return sel_beg != -1 && sel_end != -1;
}


ostream & operator<<(ostream & os, Row::Element const & e)
{
	if (e.isRTL())
		os << e.endpos << "<<" << e.pos << " ";
	else
		os << e.pos << ">>" << e.endpos << " ";

	switch (e.type) {
	case Row::STRING:
		os << "STRING: `" << to_utf8(e.str) << "' ("
		   << e.countExpanders() << " expanders.), ";
		break;
	case Row::VIRTUAL:
		os << "VIRTUAL: `" << to_utf8(e.str) << "', ";
		break;
	case Row::PREEDIT:
		os << "PREEDIT: `" << to_utf8(e.str) << "', ";
		break;
	case Row::INSET:
		os << "INSET: " << to_utf8(e.inset->layoutName()) << ", ";
		break;
	case Row::SPACE:
		os << "SPACE: ";
		break;
	case Row::MARGINSPACE:
		os << "MARGINSPACE: ";
	}
	os << "width=" << e.full_width() << ", row_flags=" << e.row_flags;
	return os;
}


ostream & operator<<(ostream & os, Row::Elements const & elts)
{
	double x = 0;
	for (Row::Element const & e : elts) {
		os << "x=" << x << " => " << e << endl;
		x += e.full_width();
	}
	return os;
}


ostream & operator<<(ostream & os, Row const & row)
{
	os << " pit: " << row.pit_ << " pos: " << row.pos_ << " end: " << row.end_
	   << " left_margin: " << row.left_margin
	   << " width: " << row.dim_.wid
	   << " right_margin: " << row.right_margin
	   << " ascent: " << row.dim_.asc
	   << " descent: " << row.dim_.des
	   << " separator: " << row.separator
	   << " label_hfill: " << row.label_hfill
	   << " end_boundary: " << row.end_boundary()
	   << " flushed: " << row.flushed_
	   << " rtl=" << row.rtl_ << "\n";
	// We cannot use the operator above, unfortunately
	double x = row.left_margin;
	for (Row::Element const & e : row.elements_) {
		os << "x=" << x << " => " << e << endl;
		x += e.full_width();
	}
	return os;
}


int Row::left_x() const
{
	double x = left_margin;
	const_iterator const end = elements_.end();
	const_iterator cit = elements_.begin();
	while (cit != end && cit->isVirtual()) {
		x += cit->full_width();
		++cit;
	}
	return support::iround(x);
}


int Row::right_x() const
{
	double x = dim_.wid;
	const_iterator const begin = elements_.begin();
	const_iterator cit = elements_.end();
	while (cit != begin) {
		--cit;
		if (cit->isVirtual())
			x -= cit->full_width();
		else
			break;
	}
	return support::iround(x);
}


bool Row::setExtraWidth(int w)
{
	if (w < 0)
		// this is not expected to happen (but it does)
		return false;
	// amount of expansion: number of expanders time the em value for each
	// string element
	int exp_amount = 0;
	for (Element const & e : elements_)
		exp_amount += e.expansionAmount();
	if (!exp_amount)
		return false;
	// extra length per expander per em
	double extra_per_em = double(w) / exp_amount;
	if (extra_per_em > MAX_SPACE_STRETCH)
		// do not stretch more than MAX_SPACE_STRETCH em per expander
		return false;
	// add extra length to each element proportionally to its em.
	for (Element & e : elements_)
		if (e.type == STRING)
			e.setExtra(extra_per_em);
	// update row dimension
	dim_.wid += w;
	return true;
}


pair<pos_type, bool> Row::x2pos(int & x) const
{
	pos_type retpos = pos();
	bool boundary = false;
	if (empty())
		x = left_margin;
	else if (x <= left_margin) {
		retpos = front().left_pos();
		x = left_margin;
	} else if (x >= width()) {
		retpos = back().right_pos();
		x = width();
	} else {
		double w = left_margin;
		const_iterator cit = begin();
		const_iterator cend = end();
		for ( ; cit != cend; ++cit) {
			if (w <= x &&  w + cit->full_width() > x) {
				int x_offset = int(x - w);
				retpos = cit->x2pos(x_offset);
				x = int(x_offset + w);
				break;
			}
			w += cit->full_width();
		}
		if (cit == end()) {
			retpos = back().right_pos();
			x = width();
		}
		/** This tests for the case where the cursor is placed
		 * just before a font direction change. See comment on
		 * the boundary_ member in DocIterator.h to understand
		 * how boundary helps here.
		 */
		else if (retpos == cit->endpos
		         && ((!cit->isRTL() && cit + 1 != end()
		              && (cit + 1)->isRTL())
		             || (cit->isRTL() && cit != begin()
		                 && !(cit - 1)->isRTL())))
			boundary = true;
	}

	if (empty())
		boundary = end_boundary();
	/** This tests for the case where the cursor is set at the end
	 * of a row which has been broken due something else than a
	 * separator (a display inset or a forced breaking of the
	 * row). We know that there is a separator when the end of the
	 * row is larger than the end of its last element.
	 */
	else if (retpos == back().endpos && back().endpos == endpos()) {
		// FIXME: need a row flag here to say that cursor cannot be at the end
		Inset const * inset = back().inset;
		if (inset && (inset->lyxCode() == NEWLINE_CODE
		              || inset->lyxCode() == SEPARATOR_CODE))
			retpos = back().pos;
		else
			boundary = end_boundary();
	}

	return make_pair(retpos, boundary);
}


bool Row::sameString(Font const & f, Change const & ch) const
{
	if (elements_.empty())
		return false;
	Element const & elt = elements_.back();
	return elt.type == STRING && !elt.final
		   && elt.font == f && elt.change == ch;
}


// FIXME: remove this and move the changebar update to Row::push_back()
void Row::finalizeLast()
{
	if (elements_.empty())
		return;
	Element & elt = elements_.back();
	if (elt.final)
		return;
	elt.final = true;
	if (elt.change.changed())
		changebar_ = true;
}


void Row::addInset(pos_type const pos, Inset const * ins,
                   Font const & f, Change const & ch)
{
	finalizeLast();
	Element e(INSET, pos, f, ch);
	e.inset = ins;
	e.row_flags = ins->rowFlags();
	elements_.push_back(e);
	changebar_ |= ins->isChanged();
}


void Row::addChar(pos_type const pos, char_type const c,
                  Font const & f, Change const & ch)
{
	if (!sameString(f, ch)) {
		finalizeLast();
		Element e(STRING, pos, f, ch);
		e.row_flags = CanBreakInside;
		elements_.push_back(e);
	}
	back().str += c;
	back().endpos = pos + 1;
}


void Row::addVirtual(pos_type const pos, docstring const & s,
             Font const & f, Change const & ch)
{
	finalizeLast();
	Element e(VIRTUAL, pos, f, ch);
	e.str = s;
	e.dim = theFontMetrics(f).dimension(s);
	e.endpos = pos;
	// Copy after* flags from previous elements, forbid break before element
	int const prev_row_flags = elements_.empty() ? Inline : elements_.back().row_flags;
	int const can_inherit = AfterFlags & ~AlwaysBreakAfter;
	e.row_flags = (prev_row_flags & can_inherit) | NoBreakBefore;
	elements_.push_back(e);
	dim_.wid += e.dim.wid;
	finalizeLast();
}


void Row::addPreedit(pos_type const pos, docstring const & s, Font const & f,
                     frontend::InputMethod * im, pos_type const char_format_index,
                     Change const & ch)
{
	finalizeLast();
	Element e(PREEDIT, pos, f, ch);
	e.str = s;
	e.im = im;
	e.dim.wid = im->horizontalAdvance(s, char_format_index);
	e.endpos = pos;
	e.char_format_index = char_format_index;
	e.lang_wrap_any = im->canWrapAnywhere(char_format_index);
	e.row_flags = CanBreakInside | CanBreakBefore | CanBreakAfter;
	elements_.push_back(e);
	dim_.wid += e.dim.wid;
	finalizeLast();
}


void Row::addSpace(pos_type const pos, int const width,
		   Font const & f, Change const & ch)
{
	finalizeLast();
	Element e(SPACE, pos, f, ch);
	e.dim.wid = width;
	elements_.push_back(e);
	dim_.wid += e.dim.wid;
}


void Row::addMarginSpace(pos_type const pos, int const width,
		   Font const & f, Change const & ch)
{
	finalizeLast();
	Element e(MARGINSPACE, pos, f, ch);
	e.dim.wid = width;
	e.row_flags = NoBreakBefore;
	elements_.push_back(e);
	dim_.wid += e.dim.wid;
}


void Row::push_back(Row::Element const & e)
{
	elements_.push_back(e);
	dim_.wid += e.dim.wid;
}


void Row::pop_back()
{
	dim_.wid -= elements_.back().dim.wid;
	elements_.pop_back();
}


namespace {

// Move stuff after \c it from \c from and the end of \c to.
void moveElements(Row::Elements & from, Row::Elements::iterator const & it,
                  Row::Elements & to)
{
	to.insert(to.end(), it, from.end());
	from.erase(it, from.end());
	if (!from.empty())
		from.back().row_flags = (from.back().row_flags & ~AfterFlags) | AlwaysBreakAfter;
}

}


Row::Elements Row::shortenIfNeeded(int const max_width, int const next_width)
{
	// FIXME: performance: if the last element is a string, we would
	// like to avoid computing its length.
	finalizeLast();
	if (empty() || width() <= max_width)
		return Elements();

	Elements::iterator const beg = elements_.begin();
	Elements::iterator const end = elements_.end();
	int wid = left_margin;
	// the smallest row width we know we can achieve by breaking a string.
	int min_row_wid = dim_.wid;

	// Search for the first element that goes beyond right margin
	Elements::iterator cit = beg;
	for ( ; cit != end ; ++cit) {
		if (wid + cit->dim.wid > max_width)
			break;
		wid += cit->dim.wid;
	}

	if (cit == end) {
		// This should not happen since the row is too long.
		LYXERR0("Something is wrong, cannot shorten row: " << *this);
		return Elements();
	}

	// Iterate backwards over breakable elements and try to break them
	Elements::iterator cit_brk = cit;
	int wid_brk = wid + cit_brk->dim.wid;
	++cit_brk;
	Elements tail;
	while (cit_brk != beg) {
		--cit_brk;
		// make a copy of the element to work on it.
		Element brk = *cit_brk;
		/* If the current element allows breaking row after itself,
		 * and if the row is already short enough after this element,
		 * then cut right after it.
		 */
		if (wid_brk <= max_width && brk.row_flags & CanBreakAfter) {
			end_ = brk.endpos;
			dim_.wid = wid_brk;
			moveElements(elements_, cit_brk + 1, tail);
			return tail;
		}
		// assume now that the current element is not there
		wid_brk -= brk.dim.wid;
		/* If the current element allows breaking row before itself,
		 * and if the row is already short enough before this element,
		 * then cut right before it.
		 */
		if (wid_brk <= max_width && brk.row_flags & CanBreakBefore && cit_brk != beg) {
			end_ = (cit_brk -1)->endpos;
			dim_.wid = wid_brk;
			moveElements(elements_, cit_brk, tail);
			return tail;
		}
		/* We have found a suitable separable element. This is the common case.
		 * Try to break it cleanly at a length that is both
		 * - less than the available space on the row
		 * - shorter than the natural width of the element, in order to enforce
		 *   break-up.
		 */
		int const split_width =  min(max_width - wid_brk, brk.dim.wid - 2);
		if (brk.splitAt(split_width, next_width, BEST_EFFORT, tail)) {
			/* if this element originally did not cause a row overflow
			 * in itself, and the remainder of the row would still be
			 * too large after breaking, then we will have issues in
			 * next row. Thus breaking here does not help.
			 */
			if (wid_brk + cit_brk->dim.wid < max_width
			    && min_row_wid - (wid_brk + brk.dim.wid) >= next_width) {
				tail.clear();
				break;
			}
			/* if we did not manage to fit a part of the element into
			 * the split_width limit, at least remember that we can
			 * shorten the row if needed.
			 */
			if (brk.dim.wid > split_width) {
				min_row_wid = wid_brk + brk.dim.wid;
				tail.clear();
				continue;
			}
			// We have found a proper place where to break this string element.
			end_ = brk.endpos;
			*cit_brk = brk;
			dim_.wid = wid_brk + brk.dim.wid;
			// If there are other elements, they should be removed.
			moveElements(elements_, cit_brk + 1, tail);
			return tail;
		}
		LATTEST(tail.empty());
	}

	if (cit != beg && cit->row_flags & NoBreakBefore) {
		// It is not possible to separate this element from the
		// previous one. (e.g. VIRTUAL)
		--cit;
		wid -= cit->dim.wid;
	}

	if (cit != beg) {
		// There is no usable separator, but several elements have
		// been added. We can cut right here.
		end_ = cit->pos;
		dim_.wid = wid;
		moveElements(elements_, cit, tail);
		return tail;
	}

	/* If we are here, it means that we have not found a separator to
	 * shorten the row. Let's try to break it again, but force
	 * splitting this time.
	 */
	if (cit->splitAt(max_width - wid, next_width, FORCE, tail)) {
		end_ = cit->endpos;
		dim_.wid = wid + cit->dim.wid;
		// If there are other elements, they should be removed.
		moveElements(elements_, cit + 1, tail);
		return tail;
	}

	// cit == beg; remove all elements after the first one.
	moveElements(elements_, cit + 1, tail);
	return tail;
}


void Row::reverseRTL()
{
	pos_type i = 0;
	pos_type const end = elements_.size();
	while (i < end) {
		// gather a sequence of elements with the same direction
		bool const rtl = elements_[i].isRTL();
		pos_type j = i;
		while (j < end && elements_[j].isRTL() == rtl)
			++j;
		// if the direction is not the same as the paragraph
		// direction, the sequence has to be reverted.
		if (rtl != rtl_)
			reverse(elements_.begin() + i, elements_.begin() + j);
		i = j;
	}
	// If the paragraph itself is RTL, reverse everything
	if (rtl_)
		reverse(elements_.begin(), elements_.end());
}


Row::const_iterator const
Row::findElementHelper(pos_type const pos, bool const boundary, double & x) const
{
	/**
	 * When boundary is true, position i is in the row element (pos, endpos)
	 * if
	 *    pos < i <= endpos
	 * whereas, when boundary is false, the test is
	 *    pos <= i < endpos
	 * The correction below allows to handle both cases.
	*/
	int const boundary_corr = (boundary && pos) ? -1 : 0;

	x = left_margin;

	/** Early return in trivial cases
	 * 1) the row is empty
	 * 2) the position is the left-most position of the row; there
	 * is a quirk here however: if the first element is virtual
	 * (end-of-par marker for example), then we have to look
	 * closer
	 */
	if (empty()
	    || (pos == begin()->left_pos() && !boundary
			&& !begin()->isVirtual()))
		return begin();

	const_iterator cit = begin();
	for ( ; cit != end() ; ++cit) {
		/** Look whether the cursor is inside the element's span. Note
		 * that it is necessary to take the boundary into account, and
		 * to accept virtual elements, in which case the position
		 * will be before the virtual element.
		 */
		if ((pos + boundary_corr >= cit->pos && pos + boundary_corr < cit->endpos)
		    || (cit->isVirtual() && pos + boundary_corr == cit->pos)) {
			// FIXME: shall we use `pos + boundary_corr' here?
			x += cit->pos2x(pos);
			break;
		}
		x += cit->full_width();
	}

	if (cit == end())
		--cit;

	return cit;
}


Row::const_iterator const Row::findElement(pos_type const pos, bool const boundary) const
{
	double dummy;
	return findElementHelper(pos, boundary, dummy);
}


double Row::pos2x(pos_type const pos, bool const boundary) const
{
	double x;
	findElementHelper(pos, boundary, x);
	return x;
}


} // namespace lyx
