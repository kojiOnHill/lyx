/**
 * \file InsetPreview.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Vincent van Ravesteijn
 *
 * Full author contact details are available in file CREDITS.
 */
#include "config.h"

#include "InsetPreview.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Cursor.h"
#include "Dimension.h"
#include "MetricsInfo.h"
#include "RenderPreview.h"
#include "texstream.h"

#include "graphics/PreviewImage.h"

#include "mathed/InsetMathHull.h"
#include "mathed/MacroTable.h"

#include <sstream>

using namespace std;

namespace lyx {


InsetPreview::InsetPreview(Buffer * buf)
	: InsetText(buf), preview_(new RenderPreview(this))
{
	setDrawFrame(true);
	setFrameColor(Color_previewframe);
}


InsetPreview::~InsetPreview()
{}


InsetPreview::InsetPreview(InsetPreview const & other)
	: InsetText(other)
{
	preview_.reset(new RenderPreview(*other.preview_, this));
}


InsetPreview & InsetPreview::operator=(InsetPreview const & other)
{
	if (&other == this)
		return *this;

	InsetText::operator=(other);
	preview_.reset(new RenderPreview(*other.preview_, this));

	return *this;
}


void InsetPreview::write(ostream & os) const
{
	os << "Preview" << "\n";
	text().write(os);
}


MacroNameSet gatherMacroDefinitions(const Buffer* buffer, const Inset * inset)
{
	// Collect macros for this inset.
	// Not done yet: this function returns a list of macro *definitions*.
	MacroNameSet macros;
	buffer->listMacroNames(macros);

	// Look for math insets and collect definitions for the used macros.
	MacroNameSet defs;
	DocIterator const dbeg = doc_iterator_begin(buffer, inset);
	DocIterator dit = dbeg;
	DocIterator const dend = doc_iterator_end(buffer, inset);
	if (!dit.nextInset())
		dit.forwardInset();

	for (; dit != dend; dit.forwardInset()) {
		InsetMath * im = dit.nextInset()->asInsetMath();
		InsetMathHull * hull = im ? im->asHullInset() : nullptr;
		if (!hull)
			continue;
		for (idx_type idx = 0; idx < hull->nargs(); ++idx)
			hull->usedMacros(hull->cell(idx), dbeg, macros, defs);
	}

	return defs;
}


docstring insetToLaTeXSnippet(const Buffer* buffer, const Inset * inset)
{
	odocstringstream str;
	otexstream os(str);
	OutputParams runparams(&buffer->params().encoding());
	inset->latex(os, runparams);

	MacroNameSet defs = gatherMacroDefinitions(buffer, inset);
	docstring macro_preamble;
	for (const auto& def : defs)
		macro_preamble.append(def);

	return macro_preamble + str.str();
}


void InsetPreview::addPreview(DocIterator const & inset_pos,
	graphics::PreviewLoader &) const
{
	preparePreview(inset_pos);
}


void InsetPreview::preparePreview(DocIterator const & pos) const
{
	docstring const snippet = insetToLaTeXSnippet(pos.buffer(), this);
	preview_->addPreview(snippet, *pos.buffer());
}


bool InsetPreview::previewState(BufferView * bv) const
{
	if (!editing(bv) && RenderPreview::previewText()) {
		graphics::PreviewImage const * pimage =
			preview_->getPreviewImage(bv->buffer());
		return pimage && pimage->image();
	}
	return false;
}


void InsetPreview::reloadPreview(DocIterator const & pos) const
{
	preparePreview(pos);
	preview_->startLoading(*pos.buffer());
}


void InsetPreview::metrics(MetricsInfo & mi, Dimension & dim) const
{
	if (previewState(mi.base.bv)) {
		preview_->metrics(mi, dim);

		dim.wid = max(dim.wid, 4);
		dim.asc = max(dim.asc, 4);

		dim.asc += topOffset(mi.base.bv);
		dim.des += bottomOffset(mi.base.bv);
		// insert a one pixel gap
		dim.wid += 1;
		Dimension dim_dummy;
		MetricsInfo mi_dummy = mi;
		InsetText::metrics(mi_dummy, dim_dummy);
		return;
	}
	InsetText::metrics(mi, dim);
}


void InsetPreview::draw(PainterInfo & pi, int x, int y) const
{
	if (previewState(pi.base.bv)) {
		// one pixel gap in front
		preview_->draw(pi, x + 1, y);
	} else
		InsetText::draw(pi, x, y);
}


void InsetPreview::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	bool const has_preview = previewState(&cur.bv());
	InsetText::doDispatch(cur, cmd);
	if (&cur.inset() == this && has_preview != previewState(&cur.bv()))
		// FIXME : it should be possible to trigger a SinglePar update
		//   on the parent paragraph.
		cur.screenUpdateFlags(Update::Force);
}


void InsetPreview::edit(Cursor & cur, bool front, EntryDirection entry_from)
{
	bool const has_preview = previewState(&cur.bv());
	cur.push(*this);
	InsetText::edit(cur, front, entry_from);
	if (has_preview) {
		// The inset contents dimension is in general different from the
		// one of the instant preview image, so we have to indicate to the
		// BufferView that a metrics update is needed.
		// FIXME : it should be possible to trigger a SinglePar update
		//   on the parent paragraph.
		cur.screenUpdateFlags(Update::Force);
	}
}


Inset * InsetPreview::editXY(Cursor & cur, int x, int y)
{
	if (previewState(&cur.bv())) {
		edit(cur, true, ENTRY_DIRECTION_IGNORE);
		return this;
	}
	cur.push(*this);
	return InsetText::editXY(cur, x, y);
}


bool InsetPreview::notifyCursorEnters(Cursor const & old, Cursor & cur)
{
	cur.screenUpdateFlags(Update::Force);
	return InsetText::notifyCursorEnters(old, cur);
}


bool InsetPreview::notifyCursorLeaves(Cursor const & old, Cursor & cur)
{
	reloadPreview(old);
	cur.screenUpdateFlags(Update::Force);
	return InsetText::notifyCursorLeaves(old, cur);
}


void InsetPreview::notifyMouseSelectionDone(Cursor & cur)
{
	reloadPreview(cur.realAnchor());
	cur.screenUpdateFlags(Update::Force);
	return InsetText::notifyMouseSelectionDone(cur);
}


} // namespace lyx
