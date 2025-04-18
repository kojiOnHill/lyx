/**
 * \file DocIterator.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 * \author Alfredo Braunstein
 *
 * Full author contact details are available in file CREDITS.
 */


#include <config.h>

#include "DocIterator.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "Encoding.h"
#include "Font.h"
#include "InsetList.h"
#include "Language.h"
#include "Paragraph.h"
#include "Text.h"

#include "mathed/MathData.h"
#include "mathed/InsetMath.h"
#include "mathed/InsetMathHull.h"

#include "insets/InsetTabular.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/lassert.h"

#include <ostream>

using namespace std;
using namespace lyx::support;

namespace lyx {


DocIterator doc_iterator_begin(const Buffer * buf0, const Inset * inset0)
{
	Buffer * buf = const_cast<Buffer *>(buf0);
	Inset * inset = const_cast<Inset *>(inset0);
	DocIterator dit(buf, inset ? inset : &buf->inset());
	dit.forwardPos();
	return dit;
}


DocIterator doc_iterator_end(const Buffer * buf0, const Inset * inset0)
{
	Buffer * buf = const_cast<Buffer *>(buf0);
	Inset * inset = const_cast<Inset *>(inset0);
	return DocIterator(buf, inset ? inset : &buf->inset());
}


DocIterator DocIterator::clone(Buffer * buffer) const
{
	LASSERT(buffer->isClone(), return DocIterator());
	Inset * inset = &buffer->inset();
	DocIterator dit(buffer);
	size_t const n = slices_.size();
	for (size_t i = 0 ; i != n; ++i) {
		LBUFERR(inset);
		dit.push_back(slices_[i]);
		dit.top().inset_ = inset;
		if (i + 1 != n)
			inset = dit.nextInset();
	}
	return dit;
}


bool DocIterator::inRegexped() const
{
	InsetMath * im = inset().asInsetMath();
	if (!im)
		return false;
	InsetMathHull * hull = im->asHullInset();
	return hull && hull->getType() == hullRegexp;
}


LyXErr & operator<<(LyXErr & os, DocIterator const & it)
{
	os.stream() << it;
	return os;
}


Inset * DocIterator::nextInset() const
{
	LASSERT(!empty(), return nullptr);
	if (pos() == lastpos())
		return nullptr;
	if (pos() > lastpos()) {
		LYXERR0("Should not happen, but it does: pos() = "
			<< pos() << ", lastpos() = " << lastpos());
		return nullptr;
	}
	if (inMathed())
		return nextAtom().nucleus();
	return paragraph().getInset(pos());
}


Inset * DocIterator::prevInset() const
{
	LASSERT(!empty(), return nullptr);
	if (pos() == 0)
		return nullptr;
	if (inMathed()) {
		if (cell().empty())
			// FIXME: this should not happen but it does.
			// See bug 3189
			// http://www.lyx.org/trac/ticket/3189
			return nullptr;
		else
			return prevAtom().nucleus();
	}
	return paragraph().getInset(pos() - 1);
}


Inset * DocIterator::realInset() const
{
	LASSERT(inTexted(), return nullptr);
	// if we are in a tabular, we need the cell
	if (inset().lyxCode() == TABULAR_CODE) {
		InsetTabular * tabular = inset().asInsetTabular();
		return tabular->cell(idx()).get();
	}
	return &inset();
}


InsetMath & DocIterator::nextMath()
{
	return *nextAtom().nucleus();
}


InsetMath & DocIterator::prevMath()
{
	return *prevAtom().nucleus();
}


MathAtom & DocIterator::prevAtom() const
{
	LASSERT(!empty(), /**/);
	LASSERT(pos() > 0, /**/);
	return cell()[pos() - 1];
}


MathAtom & DocIterator::nextAtom() const
{
	LASSERT(!empty(), /**/);
	//lyxerr << "lastpos: " << lastpos() << " next atom:\n" << *this << endl;
	LASSERT(pos() < lastpos(), /**/);
	return cell()[pos()];
}


Text * DocIterator::text() const
{
	LASSERT(!empty(), return nullptr);
	return top().text();
}


Paragraph & DocIterator::paragraph() const
{
	if (!inTexted()) {
		LYXERR0(*this);
		LBUFERR(false);
	}
	return top().paragraph();
}


Paragraph & DocIterator::innerParagraph() const
{
	LBUFERR(!empty());
	return innerTextSlice().paragraph();
}


FontSpan DocIterator::locateWord(word_location const loc) const
{
	FontSpan f = FontSpan();

	if (!top().text()->empty()) {
		f.first = pos();
		top().paragraph().locateWord(f.first, f.last, loc);
	}
	return f;
}


CursorSlice const & DocIterator::innerTextSlice() const
{
	LBUFERR(!empty());
	// go up until first non-0 text is hit
	// (innermost text is 0 in mathed)
	for (int i = depth() - 1; i >= 0; --i)
		if (slices_[i].text())
			return slices_[i];

	// This case is in principle not possible. We _must_
	// be inside a Text.
	LBUFERR(false);
	// Squash warning
	static const CursorSlice c;
	return c;
}


docstring DocIterator::paragraphGotoArgument(bool const nopos) const
{
	CursorSlice const & s = innerTextSlice();
	return nopos ? convert<docstring>(s.paragraph().id())
		     : convert<docstring>(s.paragraph().id())
		       + ' ' + convert<docstring>(s.pos());
}


DocIterator DocIterator::getInnerText() const
{
	DocIterator texted = *this;
	while (!texted.inTexted())
		texted.pop_back();
	return texted;
}


pit_type DocIterator::lastpit() const
{
	return inMathed() ? 0 : text()->paragraphs().size() - 1;
}


pos_type DocIterator::lastpos() const
{
	return inMathed() ? cell().size() : paragraph().size();
}


idx_type DocIterator::lastidx() const
{
	return top().lastidx();
}


size_t DocIterator::nargs() const
{
	// assume 1x1 grid for main text
	return top().nargs();
}


size_t DocIterator::ncols() const
{
	// assume 1x1 grid for main text
	return top().ncols();
}


size_t DocIterator::nrows() const
{
	// assume 1x1 grid for main text
	return top().nrows();
}


row_type DocIterator::row() const
{
	return top().row();
}


col_type DocIterator::col() const
{
	return top().col();
}


MathData & DocIterator::cell() const
{
//	LASSERT(inMathed(), /**/);
	return top().cell();
}


Text * DocIterator::innerText() const
{
	LASSERT(!empty(), return nullptr);
	return innerTextSlice().text();
}


Inset * DocIterator::innerInsetOfType(int code) const
{
	for (int i = depth() - 1; i >= 0; --i)
		if (slices_[i].inset_->lyxCode() == code)
			return slices_[i].inset_;
	return nullptr;
}


bool DocIterator::posBackward()
{
	if (pos() == 0)
		return false;
	--pos();
	return true;
}


bool DocIterator::posForward()
{
	if (pos() == lastpos())
		return false;
	++pos();
	return true;
}


// This duplicates code above, but is in the critical path.
// So please think twice before adding stuff
void DocIterator::forwardPos()
{
	// this dog bites his tail
	if (empty()) {
		push_back(CursorSlice(*inset_));
		return;
	}

	CursorSlice & tip = top();
	//lyxerr << "XXX\n" << *this << endl;

	// not at cell/paragraph end?
	if (tip.pos() != tip.lastpos()) {
		// move into an inset to the right if possible
		Inset * n = nullptr;
		if (inMathed())
			n = (tip.cell().begin() + tip.pos())->nucleus();
		else
			n = paragraph().getInset(tip.pos());
		if (n && n->isActive()) {
			//lyxerr << "... descend" << endl;
			push_back(CursorSlice(*n));
			return;
		}
	}

	// jump to the next cell/paragraph if possible
	if (!tip.at_end()) {
		tip.forwardPos();
		return;
	}

	// otherwise leave inset and jump over inset as a whole
	pop_back();
	// 'tip' is invalid now...
	if (!empty())
		++top().pos();
}


void DocIterator::forwardPosIgnoreCollapsed()
{
	Inset * const nextinset = nextInset();
	// FIXME: the check for asInsetMath() shouldn't be necessary
	// but math insets do not return a sensible editable() state yet.
	if (nextinset && !nextinset->asInsetMath()
	    && !nextinset->editable()) {
		++top().pos();
		return;
	}
	forwardPos();
}


void DocIterator::forwardPar()
{
	forwardPos();

	while (!empty() && (!inTexted() || pos() != 0)) {
		if (inTexted()) {
			pos_type const lastp = lastpos();
			Paragraph const & par = paragraph();
			pos_type & pos = top().pos();
			if (par.insetList().empty())
				pos = lastp;
			else
				while (pos < lastp && !par.isInset(pos))
					++pos;
		}
		forwardPos();
	}
}


void DocIterator::forwardChar()
{
	forwardPos();
	while (!empty() && pos() == lastpos())
		forwardPos();
}


void DocIterator::forwardInset()
{
	forwardPos();

	while (!empty() && !nextInset()) {
		if (inTexted()) {
			pos_type const lastp = lastpos();
			Paragraph const & par = paragraph();
			pos_type & pos = top().pos();
			while (pos < lastp && !par.isInset(pos))
				++pos;
			if (pos < lastp)
				break;
		}
		forwardPos();
	}
}


void DocIterator::backwardChar()
{
	backwardPos();
	while (!empty() && pos() == lastpos())
		backwardPos();
}


void DocIterator::backwardPos()
{
	//this dog bites his tail
	if (empty()) {
		push_back(CursorSlice(*inset_));
		top().idx() = lastidx();
		top().pit() = lastpit();
		top().pos() = lastpos();
		return;
	}

	// at inset beginning?
	if (top().at_begin()) {
		pop_back();
		return;
	}

	top().backwardPos();

	// entered another cell/paragraph from the right?
	if (top().pos() == top().lastpos())
		return;

	// move into an inset to the left if possible
	Inset * n = nullptr;
	if (inMathed())
		n = (top().cell().begin() + top().pos())->nucleus();
	else
		n = paragraph().getInset(top().pos());
	if (n && n->isActive()) {
		push_back(CursorSlice(*n));
		top().idx() = lastidx();
		top().pit() = lastpit();
		top().pos() = lastpos();
	}
}


void DocIterator::backwardPosIgnoreCollapsed()
{
	backwardPos();
	if (inTexted()) {
		Inset const * ins = realInset();
		if (ins && !ins->editable()) {
			pop_back(); // move out of collapsed inset
		}
	}
}


void DocIterator::backwardInset()
{
	backwardPos();

	while (!empty() && !nextInset()) {
		if (inTexted()) {
			pos_type const lastp = lastpos();
			Paragraph const & par = paragraph();
			pos_type & pos = top().pos();
			while (pos > 0 && (pos == lastp || !par.isInset(pos)))
				--pos;
			if (pos > 0)
				break;
		}
		backwardPos();
	}
}


bool DocIterator::hasPart(DocIterator const & it) const
{
	// it can't be a part if it is larger
	if (it.depth() > depth())
		return false;

	// as inset adresses are the 'last' level
	return &it.top().inset() == &slices_[it.depth() - 1].inset();
}


bool DocIterator::allowSpellCheck() const
{
	/// spell check is disabled if the iterator position
	/// is inside of an inset which disables the spell checker
	size_t const n = depth();
	for (size_t i = 0; i < n; ++i) {
		if (!slices_[i].inset_->allowSpellCheck())
			return false;
	}
	return true;
}


void DocIterator::updateInsets(Inset * inset)
{
	// this function re-creates the cache of inset pointers.
	//lyxerr << "converting:\n" << *this << endl;
	DocIterator dit = *this;
	size_t const n = slices_.size();
	slices_.resize(0);
	for (size_t i = 0 ; i < n; ++i) {
		if (dit[i].empty() && pos() > 0 && prevMath().lyxCode() == MATH_SCRIPT_CODE)
			// Workaround: With empty optional argument and a trailing script,
			// we have empty slices in math macro args (#11676)
			// FIXME: Find real cause!
			continue;
		LBUFERR(inset);
		push_back(dit[i]);
		top().inset_ = inset;
		if (i + 1 != n)
			inset = nextInset();
	}
	//lyxerr << "converted:\n" << *this << endl;
}


bool DocIterator::fixIfBroken()
{
	if (empty())
		return false;

	// Go through the slice stack from the bottom.
	// Check that all coordinates (idx, pit, pos) are correct and
	// that the inset is the one which is claimed to be there
	Inset * inset = &slices_[0].inset();
	size_t i = 0;
	size_t n = slices_.size();
	for (; i != n; ++i) {
		CursorSlice & cs = slices_[i];
		if (&cs.inset() != inset || ! cs.inset().isActive()) {
			// the whole slice is wrong, chop off this as well
			LYXERR(Debug::DEBUG, "fixIfBroken(): inset changed");
			// The code is different from below to avoid doing --i when i == 0.
			LYXERR(Debug::DEBUG, "fixIfBroken(): cursor chopped after " << i);
			resize(i);
			return true;
		} else if (cs.idx() > cs.lastidx()) {
			cs.idx() = cs.lastidx();
			cs.pit() = cs.lastpit();
			cs.pos() = cs.lastpos();
			LYXERR(Debug::DEBUG, "fixIfBroken(): idx fixed");
			break;
		} else if (cs.pit() > cs.lastpit()) {
			cs.pit() = cs.lastpit();
			cs.pos() = cs.lastpos();
			LYXERR(Debug::DEBUG, "fixIfBroken(): pit fixed");
			break;
		} else if (cs.pos() > cs.lastpos()) {
			cs.pos() = cs.lastpos();
			LYXERR(Debug::DEBUG, "fixIfBroken(): pos fixed");
			break;
		} else if (i != n - 1 && cs.pos() != cs.lastpos()) {
			// get inset which is supposed to be in the next slice
			if (cs.inset().inMathed())
				inset = (cs.cell().begin() + cs.pos())->nucleus();
			else if (Inset * csInset = cs.paragraph().getInset(cs.pos()))
				inset = csInset;
			else {
				// there are slices left, so there must be another inset
				break;
			}
		}
	}

	// Did we make it through the whole slice stack? Otherwise there
	// was a problem at slice i, and we have to chop off above
	if (i < n) {
		LYXERR(Debug::DEBUG, "fixIfBroken(): cursor chopped at " << i);
		resize(i + 1);
		return true;
	} else
		return false;
}


void DocIterator::sanitize()
{
	// keep a copy of the slices
	vector<CursorSlice> const sl = slices_;
	slices_.clear();
	if (buffer_)
		inset_ = &buffer_->inset();
	Inset * inset = inset_;
	// re-add the slices one by one, and adjust the inset pointer.
	for (size_t i = 0, n = sl.size(); i != n; ++i) {
		if (inset == nullptr) {
			// FIXME
			LYXERR0("Null inset on cursor stack.");
			fixIfBroken();
			break;
		}
		if (!inset->isActive()) {
			LYXERR0("Inset found on cursor stack is not active.");
			fixIfBroken();
			break;
		}
		push_back(sl[i]);
		top().inset_ = inset;
		if (fixIfBroken())
			break;
		if (i + 1 != n)
			inset = nextInset();
	}
}


void DocIterator::updateAfterInsertion(DocIterator const & dit, int const count)
{
	LASSERT(dit.inTexted(), return);
	size_type const n = find(&dit.inset());
	if (n == lyx::npos)
		// we do not point to the inset where this happened.
		return;
	CursorSlice & sl = slices_[n];
	if (sl.idx() != dit.idx() || sl.pit() != dit.pit() || sl.pos() < dit.pos())
		// the change has been happening in a place that we do not care about.
		return;

	if (count < 0 && sl.pos() < dit.pos() - count) {
		if (n + 1 < slices_.size())
			// the inset we pointed to has been chopped-off.
			resize(n + 1);
		else
			// set cursor at the start of deleted part.
			sl.pos() = dit.pos();
	} else
		// adapt cursor position.
		sl.pos() += count;
}


bool DocIterator::isInside(Inset const * p) const
{
	for (CursorSlice const & sl : slices_)
		if (&sl.inset() == p)
			return true;
	return false;
}


void DocIterator::leaveInset(Inset const & inset)
{
	for (size_t i = 0; i != slices_.size(); ++i) {
		if (&slices_[i].inset() == &inset) {
			resize(i);
			return;
		}
	}
}


size_type DocIterator::find(MathData const & cell) const
{
	for (size_t l = 0; l != slices_.size(); ++l) {
		if (slices_[l].asInsetMath() && &slices_[l].cell() == &cell)
			return l;
	}
	return lyx::npos;
}


size_type DocIterator::find(Inset const * inset) const
{
	for (size_t l = 0; l != slices_.size(); ++l) {
		if (&slices_[l].inset() == inset)
			return l;
	}
	return lyx::npos;
}


void DocIterator::resize(size_type count, vector<CursorSlice> & cut)
{
	LASSERT(count <= depth(), return);
	cut = vector<CursorSlice>(slices_.begin() + count, slices_.end());
	slices_.resize(count);
}


void DocIterator::append(vector<CursorSlice> const & x)
{
	slices_.insert(slices_.end(), x.begin(), x.end());
}


void DocIterator::append(idx_type idx, pos_type pos)
{
	slices_.push_back(CursorSlice());
	top().idx() = idx;
	top().pos() = pos;
}


docstring DocIterator::getPossibleLabel() const
{
	return inMathed() ? from_ascii("eq:") : text()->getPossibleLabel(*this);
}


Encoding const * DocIterator::getEncoding() const
{
	if (empty())
		return nullptr;

	BufferParams const & bp = buffer()->params();
	if (bp.useNonTeXFonts)
		return encodings.fromLyXName("utf8-plain");

	// With platex, we don't switch encodings (not even if forced).
	if (bp.encoding().package() == Encoding::japanese)
		return &bp.encoding();

	CursorSlice const & sl = innerTextSlice();
	Text const & text = *sl.text();
	Language const * lang =
		text.getPar(sl.pit()).getFont(bp, sl.pos(), text.outerFont(sl.pit())).language();
	// If we have a custom encoding for the buffer, we don't switch
	// encodings (see output_latex::switchEncoding())
	bool const customenc = bp.inputenc != "auto-legacy" && bp.inputenc != "auto-legacy-plain";
	Encoding const * enc = customenc ? &bp.encoding() : lang->encoding();

	// Some insets force specific encodings sometimes (e.g., listings in
	// multibyte context forces singlebyte).
	if (inset().forcedEncoding(enc, encodings.fromLyXName("iso8859-1"))) {
		// Get the language outside the inset
		size_t const n = depth();
		for (size_t i = 0; i < n; ++i) {
			Text const & otext = *slices_[i].text();
			Language const * olang =
					otext.getPar(slices_[i].pit()).getFont(bp, slices_[i].pos(),
									       otext.outerFont(slices_[i].pit())).language();
			Encoding const * oenc = olang->encoding();
			if (oenc->name() != "inherit")
				return inset().forcedEncoding(enc, oenc);
		}
		// Fall back to buffer encoding if no outer lang was found.
		return inset().forcedEncoding(enc, &bp.encoding());
	}

	// Inherited encoding (latex_language) is determined by the context
	// Look for the first outer encoding that is not itself "inherit"
	if (lang->encoding()->name() == "inherit") {
		size_t const n = depth();
		for (size_t i = 0; i < n; ++i) {
			Text const & otext = *slices_[i].text();
			Language const * olang =
					otext.getPar(slices_[i].pit()).getFont(bp, slices_[i].pos(),
									       otext.outerFont(slices_[i].pit())).language();
			// Again, if we have a custom encoding, this is used
			// instead of the language's.
			Encoding const * oenc = customenc ? &bp.encoding() : olang->encoding();
			if (olang->encoding()->name() != "inherit")
				return oenc;
		}
	}

	return enc;
}


ostream & operator<<(ostream & os, DocIterator const & dit)
{
	for (size_t i = 0, n = dit.depth(); i != n; ++i)
		os << " " << dit[i] << "\n";
	return os;
}


///////////////////////////////////////////////////////

StableDocIterator::StableDocIterator(DocIterator const & dit) :
	data_(dit.internalData())
{
	for (size_t i = 0, n = data_.size(); i != n; ++i)
		data_[i].inset_ = nullptr;
}


DocIterator StableDocIterator::asDocIterator(Buffer * buf) const
{
	DocIterator dit(buf);
	dit.slices_ = data_;
	dit.sanitize();
	return dit;
}


ostream & operator<<(ostream & os, StableDocIterator const & dit)
{
	for (size_t i = 0, n = dit.data_.size(); i != n; ++i)
		os << " " << dit.data_[i] << "\n";
	return os;
}


bool operator==(StableDocIterator const & dit1, StableDocIterator const & dit2)
{
	return dit1.data_ == dit2.data_;
}


} // namespace lyx
