/**
 * \file src/TextMetrics.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Asger Alstrup
 * \author Lars Gullik Bjønnes
 * \author Jean-Marc Lasgouttes
 * \author John Levon
 * \author André Pönitz
 * \author Dekel Tsur
 * \author Jürgen Vigna
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "TextMetrics.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "CoordCache.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "Layout.h"
#include "LyXRC.h"
#include "MetricsInfo.h"
#include "ParagraphParameters.h"
#include "RowPainter.h"
#include "Session.h"
#include "Text.h"
#include "TextClass.h"
#include "VSpace.h"

#include "insets/InsetSeparator.h"
#include "insets/InsetText.h"

#include "mathed/MacroTable.h"

#include "frontends/FontMetrics.h"
#include "frontends/NullPainter.h"

#include "support/debug.h"
#include "support/lassert.h"
#include "support/Changer.h"

#include <stdlib.h>
#include <cmath>

using namespace std;


namespace lyx {

using frontend::FontMetrics;

namespace {


int numberOfLabelHfills(Paragraph const & par, Row const & row)
{
	pos_type last = row.endpos() - 1;
	pos_type first = row.pos();

	// hfill *DO* count at the beginning of paragraphs!
	if (first) {
		while (first < last && par.isHfill(first))
			++first;
	}

	last = min(last, par.beginOfBody());
	int n = 0;
	for (pos_type p = first; p < last; ++p) {
		if (par.isHfill(p))
			++n;
	}
	return n;
}

// FIXME: this needs to be rewritten, probably by merging it into some
// code that, besides counting, sets the active status of the space
// inset in the row element.
int numberOfHfills(Row const & row, ParagraphMetrics const & pm,
                   pos_type const body_pos)
{
	int n = 0;
	Row::const_iterator cit = row.begin();
	Row::const_iterator const end = row.end();
	for ( ; cit != end ; ++cit)
		if (cit->pos >= body_pos
		    && cit->inset && pm.hfillExpansion(row, cit->pos))
			++n;
	return n;
}


} // namespace

/////////////////////////////////////////////////////////////////////
//
// TextMetrics
//
/////////////////////////////////////////////////////////////////////


TextMetrics::TextMetrics(BufferView * bv, Text * text)
	: bv_(bv), text_(text), dim_(bv_->workWidth(), 10, 10),
	  max_width_(dim_.wid), tight_(false)
{}


bool TextMetrics::contains(pit_type pit) const
{
	return par_metrics_.find(pit) != par_metrics_.end();
}


void TextMetrics::forget(pit_type pit)
{
	par_metrics_.erase(pit);
}


pair<pit_type, ParagraphMetrics const *> TextMetrics::first() const
{
	ParMetricsCache::const_iterator it = par_metrics_.begin();
	return make_pair(it->first, &it->second);
}


pair<pit_type, ParagraphMetrics const *> TextMetrics::last() const
{
	LBUFERR(!par_metrics_.empty());
	ParMetricsCache::const_reverse_iterator it = par_metrics_.rbegin();
	return make_pair(it->first, &it->second);
}


bool TextMetrics::isLastRow(Row const & row) const
{
	ParagraphList const & pars = text_->paragraphs();
	return row.endpos() >= pars[row.pit()].size()
		&& row.pit() + 1 == pit_type(pars.size());
}


bool TextMetrics::isFirstRow(Row const & row) const
{
	return row.pos() == 0 && row.pit() == 0;
}


void TextMetrics::setRowChanged(pit_type pit, pos_type pos)
{
	for (auto & pm_pair : par_metrics_)
		if (pm_pair.first == pit)
			for (Row & row : pm_pair.second.rows())
				if (row.pos() == pos)
					row.changed(true);
}


ParagraphMetrics & TextMetrics::parMetrics(pit_type pit, bool redo)
{
	ParMetricsCache::iterator pmc_it = par_metrics_.find(pit);
	if (pmc_it == par_metrics_.end()) {
		pmc_it = par_metrics_.insert(
			make_pair(pit, ParagraphMetrics(text_->getPar(pit)))).first;
	}
	if (pmc_it->second.rows().empty() && redo)
		redoParagraph(pit);
	return pmc_it->second;
}


ParagraphMetrics const & TextMetrics::parMetrics(pit_type pit) const
{
	return const_cast<TextMetrics *>(this)->parMetrics(pit, true);
}


ParagraphMetrics & TextMetrics::parMetrics(pit_type pit)
{
	return parMetrics(pit, true);
}


void TextMetrics::newParMetricsDown()
{
	pair<pit_type, ParagraphMetrics> const & last = *par_metrics_.rbegin();
	pit_type const pit = last.first + 1;
	if (pit == int(text_->paragraphs().size()))
		return;

	// do it and update its position.
	redoParagraph(pit);
	par_metrics_[pit].setPosition(last.second.bottom() + par_metrics_[pit].ascent());
	updatePosCache(pit);
}


void TextMetrics::newParMetricsUp()
{
	pair<pit_type, ParagraphMetrics> const & first = *par_metrics_.begin();
	if (first.first == 0)
		return;

	pit_type const pit = first.first - 1;
	// do it and update its position.
	redoParagraph(pit);
	par_metrics_[pit].setPosition(first.second.top() - par_metrics_[pit].descent());
	updatePosCache(pit);
}


void TextMetrics::updateMetrics(pit_type const anchor_pit, int const anchor_ypos,
                                int const bv_height)
{
	LASSERT(text_->isMainText(), return);
	pit_type const npit = pit_type(text_->paragraphs().size());

	if (!contains(anchor_pit))
		// Rebreak anchor paragraph.
		redoParagraph(anchor_pit);
	ParagraphMetrics & anchor_pm = parMetrics(anchor_pit);
	anchor_pm.setPosition(anchor_ypos);

	// Redo paragraphs above anchor if necessary.
	int y1 = anchor_ypos - anchor_pm.ascent();
	// We are now just above the anchor paragraph.
	pit_type pit1 = anchor_pit - 1;
	for (; pit1 >= 0 && y1 > 0; --pit1) {
		if (!contains(pit1))
			redoParagraph(pit1);
		ParagraphMetrics & pm = parMetrics(pit1);
		y1 -= pm.descent();
		// Save the paragraph position in the cache.
		pm.setPosition(y1);
		y1 -= pm.ascent();
	}

	// Redo paragraphs below the anchor if necessary.
	int y2 = anchor_ypos + anchor_pm.descent();
	// We are now just below the anchor paragraph.
	pit_type pit2 = anchor_pit + 1;
	for (; pit2 < npit && y2 < bv_height; ++pit2) {
		if (!contains(pit2))
			redoParagraph(pit2);
		ParagraphMetrics & pm = parMetrics(pit2);
		y2 += pm.ascent();
		// Save the paragraph position in the cache.
		pm.setPosition(y2);
		y2 += pm.descent();
	}

	LYXERR(Debug::PAINTING, "TextMetrics::updateMetrics "
		<< " anchor pit = " << anchor_pit
		<< " anchor ypos = " << anchor_ypos
		<< " y1 = " << y1
		<< " y2 = " << y2
		<< " pit1 = " << pit1
		<< " pit2 = " << pit2);
}


bool TextMetrics::metrics(MetricsInfo const & mi, Dimension & dim, int min_width)
{
	LBUFERR(mi.base.textwidth > 0);
	max_width_ = mi.base.textwidth;
	tight_ = mi.tight_insets;
	// backup old dimension.
	Dimension const old_dim = dim_;
	// reset dimension.
	dim_ = Dimension();
	dim_.wid = min_width;
	pit_type const npar = text_->paragraphs().size();
	if (npar > 1 && !tight_)
		// If there is more than one row, expand the text to
		// the full allowable width.
		dim_.wid = max_width_;

	//lyxerr << "TextMetrics::metrics: width: " << mi.base.textwidth
	//	<< " maxWidth: " << max_width_ << "\nfont: " << mi.base.font << endl;

	bool changed = false;
	int h = 0;
	for (pit_type pit = 0; pit != npar; ++pit) {
		// create rows, but do not set alignment yet
		changed |= redoParagraph(pit, false);
		ParagraphMetrics const & pm = par_metrics_[pit];
		h += pm.height();
		if (dim_.wid < pm.width())
			dim_.wid = pm.width();
	}

	// Now set alignment for all rows (the width might not have been known before).
	for (pit_type pit = 0; pit != npar; ++pit) {
		ParagraphMetrics & pm = par_metrics_[pit];
		for (Row & row : pm.rows())
			setRowAlignment(row, dim_.wid);
	}

	dim_.asc = par_metrics_[0].ascent();
	dim_.des = h - dim_.asc;
	//lyxerr << "dim_.wid " << dim_.wid << endl;
	//lyxerr << "dim_.asc " << dim_.asc << endl;
	//lyxerr << "dim_.des " << dim_.des << endl;

	changed |= dim_ != old_dim;
	dim = dim_;
	return changed;
}


void TextMetrics::updatePosCache(pit_type pit) const
{
	frontend::NullPainter np;
	PainterInfo pi(bv_, np);
	drawParagraph(pi, pit, origin_.x_, par_metrics_[pit].position());
}


int TextMetrics::rightMargin(ParagraphMetrics const & pm) const
{
	return text_->isMainText() ? pm.rightMargin(*bv_) : 0;
}


int TextMetrics::rightMargin(pit_type const pit) const
{
	return text_->isMainText() ? par_metrics_[pit].rightMargin(*bv_) : 0;
}


void TextMetrics::applyOuterFont(Font & font) const
{
	FontInfo lf(font_.fontInfo());
	lf.reduce(bv_->buffer().params().getFont().fontInfo());
	font.fontInfo().realize(lf);
}


// This is an arbitrary magic value
static const pos_type force_label = -12345;

// if pos == force_label, this function will return the label font instead.
// This undocumented and only for use by labelDisplayFont()
Font TextMetrics::displayFont(pit_type pit, pos_type pos) const
{
	LASSERT(pos >= 0 || pos == force_label, { static Font f; return f; });

	ParagraphList const & pars = text_->paragraphs();
	Paragraph const & par = pars[pit];
	Layout const & layout = par.layout();
	Buffer const & buffer = bv_->buffer();
	// FIXME: broken?
	BufferParams const & bparams = buffer.params();
	bool const label = pos < par.beginOfBody() || pos == force_label;

	Font f = (pos == force_label) ? Font(inherit_font, bparams.language)
	                              : par.getFontSettings(bparams, pos);
	FontInfo const & lf = label ? layout.labelfont : layout.font;

	// We specialize the 95% common case:
	if (!par.getDepth()) {
		if (!text_->isMainText())
			applyOuterFont(f);

		FontInfo rlf = label ? layout.reslabelfont : layout.resfont;

		// In case the default family has been customized
		if (lf.family() == INHERIT_FAMILY)
			rlf.setFamily(bparams.getFont().fontInfo().family());
		f.fontInfo().realize(rlf);
		return f;
	}

	// The uncommon case need not be optimized as much.
	// FIXME : the two code paths seem different in function.
	f.fontInfo().realize(lf);

	if (!text_->isMainText())
		applyOuterFont(f);

	// Realize against environment font information
	// NOTE: the cast to pit_type should be removed when pit_type
	// changes to a unsigned integer.
	if (pit < pit_type(pars.size()))
		f.fontInfo().realize(text_->outerFont(pit).fontInfo());

	// Realize with the fonts of lesser depth.
	f.fontInfo().realize(bparams.getFont().fontInfo());

	return f;
}


Font TextMetrics::labelDisplayFont(pit_type pit) const
{
	return displayFont(pit, force_label);
}


bool TextMetrics::isRTL(CursorSlice const & sl, bool boundary) const
{
	if (!sl.text())
		return false;

	int correction = 0;
	if (boundary && sl.pos() > 0)
		correction = -1;

	return displayFont(sl.pit(), sl.pos() + correction).isVisibleRightToLeft();
}


bool TextMetrics::isRTLBoundary(pit_type pit, pos_type pos) const
{
	// no RTL boundary at paragraph start
	if (pos == 0)
		return false;

	Font const & left_font = displayFont(pit, pos - 1);

	return isRTLBoundary(pit, pos, left_font);
}


// isRTLBoundary returns false on a real end-of-line boundary,
// because otherwise the two boundary types get mixed up.
// This is the whole purpose of this being in TextMetrics.
bool TextMetrics::isRTLBoundary(pit_type pit, pos_type pos,
		Font const & font) const
{
	if (// no RTL boundary at paragraph start
	    pos == 0
	    // if the metrics have not been calculated, then we are not
	    // on screen and can safely ignore issues about boundaries.
	    || !contains(pit))
		return false;

	ParagraphMetrics const & pm = par_metrics_[pit];
	// no RTL boundary in empty paragraph
	if (pm.rows().empty())
		return false;

	pos_type const endpos = pm.getRow(pos - 1, false).endpos();
	pos_type const startpos = pm.getRow(pos, false).pos();
	// no RTL boundary at line start:
	// abc\n   -> toggle to RTL ->    abc\n     (and not:    abc\n|
	// |                              |                               )
	if (pos == startpos && pos == endpos) // start of cur row, end of prev row
		return false;

	Paragraph const & par = text_->getPar(pit);
	// no RTL boundary at line break:
	// abc|\n    -> move right ->   abc\n       (and not:    abc\n|
	// FED                          FED|                     FED     )
	if (startpos == pos && endpos == pos && endpos != par.size()
		&& (par.isNewline(pos - 1)
			|| par.isEnvSeparator(pos - 1)
			|| par.isLineSeparator(pos - 1)
			|| par.isSeparator(pos - 1)))
		return false;

	bool const left = font.isVisibleRightToLeft();
	bool right;
	if (pos == par.size())
		right = par.isRTL(bv_->buffer().params());
	else
		right = displayFont(pit, pos).isVisibleRightToLeft();

	return left != right;
}


bool TextMetrics::redoParagraph(pit_type const pit, bool const align_rows)
{
	Paragraph & par = text_->getPar(pit);
	// IMPORTANT NOTE: We pass 'false' explicitly in order to not call
	// redoParagraph() recursively inside parMetrics.
	Dimension old_dim = parMetrics(pit, false).dim();
	ParagraphMetrics & pm = par_metrics_[pit];
	pm.reset(par);

	Buffer & buffer = bv_->buffer();
	bool changed = false;

	// Check whether there are InsetBibItems that need fixing
	// FIXME: This check ought to be done somewhere else. It is the reason
	// why text_ is not const. But then, where else to do it?
	// Well, how can you end up with either (a) a biblio environment that
	// has no InsetBibitem, (b) a biblio environment with more than one
	// InsetBibitem or (c) a paragraph that has a bib item but is no biblio
	// environment? I think the answer is: when paragraphs are merged;
	// when layout is set; when material is pasted.
	if (par.brokenBiblio()) {
		Cursor & cur = bv_->cursor();
		// In some cases, we do not know how to record undo
		if (&cur.inset() == &text_->inset())
			cur.recordUndo(pit, pit);

		int const moveCursor = par.fixBiblio(buffer);

		// Is it necessary to update the cursor?
		if (&cur.inset() == &text_->inset() && cur.pit() == pit) {
			if (moveCursor > 0)
				cur.posForward();
			else if (moveCursor < 0 && cur.pos() >= -moveCursor)
				cur.posBackward();
		}
	}

	// Optimisation: this is used in the next two loops
	// so better to calculate that once here.
	int const right_margin = rightMargin(pm);

	// iterator pointing to paragraph to resolve macros
	DocIterator parPos = text_->macrocontextPosition();
	if (!parPos.empty())
		parPos.pit() = pit;
	else {
		LYXERR(Debug::INFO, "MacroContext not initialised!"
			<< " Going through the buffer again and hope"
			<< " the context is better then.");
		// FIXME audit updateBuffer calls
		// This should not be here, but it is not clear yet where else it
		// should be.
		bv_->buffer().updateBuffer();
		parPos = text_->macrocontextPosition();
		LBUFERR(!parPos.empty());
		parPos.pit() = pit;
	}

	// redo insets
	par.setBeginOfBody();
	Font const bufferfont = buffer.params().getFont();
	CoordCache::Insets & insetCache = bv_->coordCache().insets();
	map <Inset const *, int> extrawidths;
	for (auto const & e : par.insetList()) {
		// FIXME Doesn't this HAVE to be non-empty?
		// position already initialized?
		if (!parPos.empty()) {
			parPos.pos() = e.pos;

			// A macro template would normally not be visible
			// by itself. But the tex macro semantics allow
			// recursion, so we artifically take the context
			// after the macro template to simulate this.
			if (e.inset->lyxCode() == MATHMACRO_CODE)
				parPos.pos()++;
		}

		// If there is an end of paragraph marker, its size should be
		// substracted to the available width. The logic here is
		// almost the same as in tokenizeParagraph, remember keep them in sync.
		int eop = 0;
		if (e.pos + 1 == par.size()
		      && (lyxrc.paragraph_markers || par.lookupChange(par.size()).changed())
		      && size_type(pit + 1) < text_->paragraphs().size()) {
			Font f(text_->layoutFont(pit));
			// ¶ U+00B6 PILCROW SIGN
			eop = theFontMetrics(f).width(char_type(0x00B6));
		}

		// do the metric calculation
		Dimension dim;
		int const w = max_width_ - leftMargin(pit, e.pos)
			- right_margin - eop;
		Font const & font = e.inset->inheritFont() ?
			displayFont(pit, e.pos) : bufferfont;
		MacroContext mc(&buffer, parPos);
		MetricsInfo mi(bv_, font.fontInfo(), w, mc, e.pos == 0, tight_);
		mi.base.outer_font = displayFont(pit, e.pos).fontInfo();
		e.inset->metrics(mi, dim);
		/* FIXME: This is a hack. This allows InsetMathHull to state
		 * that it needs some elbow room beyond its width, in order to
		 * fit the numbering and/or the left margin (with left
		 * alignment), which are outside of the inset itself.
		 *
		 * To this end, InsetMathHull::metrics() sets a value in
		 * MetricsInfo::extrawidth and this value is added later to
		 * the width of the row that contains the inset (when this row
		 * is tight or shorter than the max allowed width).
		 *
		 * See ticket #12320 for details.
		*/
		extrawidths[e.inset] = mi.extrawidth;

		if (!insetCache.has(e.inset) || insetCache.dim(e.inset) != dim) {
			insetCache.add(e.inset, dim);
			changed = true;
		}
	}

	// Transform the paragraph into a single row containing all the elements.
	Row const bigrow = tokenizeParagraph(pit);
	// Split the row in several rows fitting in available width
	pm.rows() = breakParagraph(bigrow);

	// Add the needed extra width to the rows that contain the insets that request it
	for (Row & row : pm.rows())
		for (Row::Element & e : row)
			if (e.type == Row::INSET && (row.width() < max_width_ || tight_))
				row.dim().wid += extrawidths[e.inset];

	/* If there is more than one row, expand the text to the full
	 * allowable width. This setting here is needed for the
	 * setRowAlignment() below. We do nothing when tight insets are
	 * requested.
	 */
	if (pm.rows().size() > 1 && !tight_ && dim_.wid < max_width_)
			dim_.wid = max_width_;

	// Compute height and alignment of the rows.
	for (Row & row : pm.rows()) {
		setRowHeight(row);
		if (align_rows)
			setRowAlignment(row, max(dim_.wid, row.width()));

		pm.dim().wid = max(pm.dim().wid, row.width() + row.right_margin);
		pm.dim().des += row.height();
	}

	// This type of margin can only be handled at the global paragraph level
	if (par.layout().margintype == MARGIN_RIGHT_ADDRESS_BOX) {
		int offset = 0;
		if (par.isRTL(buffer.params())) {
			// globally align the paragraph to the left.
			int minleft = max_width_;
			for (Row const & row : pm.rows())
				minleft = min(minleft, row.left_margin);
			offset = right_margin - minleft;
		} else {
			// globally align the paragraph to the right.
			int maxwid = 0;
			for (Row const & row : pm.rows())
				maxwid = max(maxwid, row.width());
			offset = max_width_ - right_margin - maxwid;
		}

		for (Row & row : pm.rows()) {
			row.left_margin += offset;
			row.dim().wid += offset;
		}
	}

	// The space above and below the paragraph.
	int top = parTopSpacing(pit);
	int bottom = parBottomSpacing(pit);

	// Top and bottom margin of the document (only at top-level)
	// FIXME: It might be better to move this in another method
	// specially tailored for the main text.
	if (text_->isMainText()) {
		if (pit == 0)
			top += bv_->topMargin();
		if (pit + 1 == pit_type(text_->paragraphs().size())) {
			bottom += bv_->bottomMargin();
		}
	}

	// Add the top/bottom space to rows and paragraph metrics
	pm.rows().front().dim().asc += top;
	pm.rows().back().dim().des += bottom;
	pm.dim().des += top + bottom;

	// Move the pm ascent to be the same as the first row ascent
	pm.dim().asc += pm.rows().front().ascent();
	pm.dim().des -= pm.rows().front().ascent();

	changed |= old_dim.height() != pm.dim().height();

	return changed;
}


LyXAlignment TextMetrics::getAlign(Paragraph const & par, Row const & row) const
{
	LyXAlignment align = par.getAlign(bv_->buffer().params());

	// handle alignment inside tabular cells
	Inset const & owner = text_->inset();
	bool forced_block = false;
	switch (owner.contentAlignment()) {
	case LYX_ALIGN_BLOCK:
		// In general block align is the default state, but here it is
		// an explicit choice. Therefore it should not be overridden
		// later.
		forced_block = true;
		// fall through
	case LYX_ALIGN_CENTER:
	case LYX_ALIGN_LEFT:
	case LYX_ALIGN_RIGHT:
		if (align == LYX_ALIGN_NONE || align == LYX_ALIGN_BLOCK)
			align = owner.contentAlignment();
		break;
	default:
		// unchanged (use align)
		break;
	}

	// Display-style insets should always be on a centered row
	if (Inset const * inset = par.getInset(row.pos())) {
		if (inset->rowFlags() & Display) {
			if (inset->rowFlags() & AlignLeft)
				align = LYX_ALIGN_LEFT;
			else if (inset->rowFlags() & AlignRight)
				align = LYX_ALIGN_RIGHT;
			else
				align = LYX_ALIGN_CENTER;
		}
	}

	if (align == LYX_ALIGN_BLOCK) {
		// If this row has been broken abruptly by a display inset, or
		// it is the end of the paragraph, or the user requested we
		// not justify stuff, then don't stretch.
		// A forced block alignment can only be overridden the 'no
		// justification on screen' setting.
		if ((row.flushed() && !forced_block)
		    || !bv_->buffer().params().justification)
			align = row.isRTL() ? LYX_ALIGN_RIGHT : LYX_ALIGN_LEFT;
	}

	return align;
}


void TextMetrics::setRowAlignment(Row & row, int width) const
{
	row.label_hfill = 0;
	row.separator = 0;

	Paragraph const & par = text_->getPar(row.pit());

	int const w = width - row.right_margin - row.width();
	// FIXME: put back this assertion when the crash on new doc is solved.
	//LASSERT(w >= 0, /**/);

	// is there a manual margin with a manual label
	Layout const & layout = par.layout();

	int nlh = 0;
	if (layout.margintype == MARGIN_MANUAL
	    && layout.labeltype == LABEL_MANUAL) {
		/// We might have real hfills in the label part
		nlh = numberOfLabelHfills(par, row);

		// A manual label par (e.g. List) has an auto-hfill
		// between the label text and the body of the
		// paragraph too.
		// But we don't want to do this auto hfill if the par
		// is empty.
		if (!par.empty())
			++nlh;

		if (nlh && !par.getLabelWidthString().empty())
			row.label_hfill = labelFill(row) / double(nlh);
	}

	// are there any hfills in the row?
	ParagraphMetrics const & pm = par_metrics_[row.pit()];
	int nh = numberOfHfills(row, pm, par.beginOfBody());
	int hfill = 0;
	int hfill_rem = 0;

	// We don't have to look at the alignment if the row is already
	// larger then the permitted width as then we force the
	// LEFT_ALIGN'edness!
	if (row.width() >= max_width_)
		return;

	if (nh == 0) {
		// Common case : there is no hfill, and the alignment will be
		// meaningful
		switch (getAlign(par, row)) {
		case LYX_ALIGN_BLOCK:
			// Expand expanding characters by a total of w
			if (!row.setExtraWidth(w) && row.isRTL()) {
				// Justification failed and the text is RTL: align to the right
				row.left_margin += w;
				row.dim().wid += w;
			}
			break;
		case LYX_ALIGN_LEFT:
			// a displayed inset that is flushed
			if (Inset const * inset = par.getInset(row.pos())) {
				row.left_margin += inset->indent(*bv_);
				row.dim().wid += inset->indent(*bv_);
			}
			break;
		case LYX_ALIGN_RIGHT:
			if (Inset const * inset = par.getInset(row.pos())) {
				int const new_w = max(w - inset->indent(*bv_), 0);
				row.left_margin += new_w;
				row.dim().wid += new_w;
			} else {
				row.left_margin += w;
				row.dim().wid += w;
			}
			break;
		case LYX_ALIGN_CENTER:
			row.dim().wid += w / 2;
			row.left_margin += w / 2;
			break;
		case LYX_ALIGN_NONE:
		case LYX_ALIGN_LAYOUT:
		case LYX_ALIGN_SPECIAL:
		case LYX_ALIGN_DECIMAL:
			break;
		}
		return;
	}

	// Case nh > 0. There are hfill separators.
	hfill = w / nh;
	hfill_rem = w % nh;
	row.dim().wid += w;
	// Set size of hfill insets
	pos_type const endpos = row.endpos();
	pos_type body_pos = par.beginOfBody();
	if (body_pos > 0
	    && (body_pos > endpos || !par.isLineSeparator(body_pos - 1)))
		body_pos = 0;

	CoordCache::Insets & insetCache = bv_->coordCache().insets();
	for (Row::Element & e : row) {
		if (row.label_hfill && e.endpos == body_pos
		    && e.type == Row::SPACE)
			e.dim.wid -= int(row.label_hfill * (nlh - 1));
		if (e.inset && pm.hfillExpansion(row, e.pos)) {
			if (e.pos >= body_pos) {
				e.dim.wid += hfill;
				--nh;
				if (nh == 0)
					e.dim.wid += hfill_rem;
			} else
				e.dim.wid += int(row.label_hfill);
			// Cache the inset dimension.
			insetCache.add(e.inset, e.dim);
		}
	}
}


int TextMetrics::labelFill(Row const & row) const
{
	Paragraph const & par = text_->getPar(row.pit());
	LBUFERR(par.beginOfBody() > 0 || par.isEnvSeparator(0));

	int w = 0;
	// iterate over elements before main body (except the last one,
	// which is extra space).
	for (Row::Element const & e : row) {
		if (e.endpos >= par.beginOfBody())
			break;
		w += e.dim.wid;
	}

	docstring const & label = par.params().labelWidthString();
	if (label.empty())
		return 0;

	FontMetrics const & fm
		= theFontMetrics(text_->labelFont(par));

	return max(0, fm.width(label) - w);
}


namespace {

/**
 * Calling Text::getFont is slow. While rebreaking we scan a
 * paragraph from left to right calling getFont for every char.  This
 * simple class address this problem by hidding an optimization trick
 * (not mine btw -AB): the font is reused in the whole font span.  The
 * class handles transparently the "hidden" (not part of the fontlist)
 * label font (as getFont does).
 **/
class FontIterator
{
public:
	///
	FontIterator(TextMetrics const & tm,
		Paragraph const & par, pit_type pit, pos_type pos)
		: tm_(tm), par_(par), pit_(pit), pos_(pos),
		font_(tm.displayFont(pit, pos)),
		endspan_(par.fontSpan(pos).last),
		bodypos_(par.beginOfBody())
	{}

	///
	Font const & operator*() const { return font_; }

	///
	FontIterator & operator++()
	{
		++pos_;
		if (pos_ < par_.size() && (pos_ > endspan_ || pos_ == bodypos_)) {
			font_ = tm_.displayFont(pit_, pos_);
			endspan_ = par_.fontSpan(pos_).last;
		}
		return *this;
	}

	///
	Font * operator->() { return &font_; }

private:
	///
	TextMetrics const & tm_;
	///
	Paragraph const & par_;
	///
	pit_type pit_;
	///
	pos_type pos_;
	///
	Font font_;
	///
	pos_type endspan_;
	///
	pos_type bodypos_;
};

} // namespace


Row TextMetrics::tokenizeParagraph(pit_type const pit) const
{
	Row row;
	row.pit(pit);
	Paragraph const & par = text_->getPar(pit);
	Buffer const & buf = text_->inset().buffer();
	BookmarksSection::BookmarkPosList bpl =
		theSession().bookmarks().bookmarksInPar(buf.fileName(), par.id());

	pos_type const end = par.size();
	pos_type const body_pos = par.beginOfBody();

	// check for possible inline completion
	DocIterator const & ic_it = bv_->inlineCompletionPos();
	pos_type ic_pos = -1;
	if (ic_it.inTexted() && ic_it.text() == text_ && ic_it.pit() == pit)
		ic_pos = ic_it.pos();

	// Now we iterate through until we reach the right margin
	// or the end of the par, then build a representation of the row.
	pos_type i = 0;
	FontIterator fi = FontIterator(*this, par, pit, 0);
	// The real stopping condition is a few lines below.
	while (true) {
		// Firstly, check whether there is a bookmark here.
		if (lyxrc.bookmarks_visibility == LyXRC::BMK_INLINE)
			for (auto const & bp_p : bpl)
				if (bp_p.second == i) {
					Font f = *fi;
					f.fontInfo().setColor(Color_bookmark);
					// ❶ U+2776 DINGBAT NEGATIVE CIRCLED DIGIT ONE
					char_type const ch = 0x2775 + bp_p.first;
					row.addVirtual(i, docstring(1, ch), f, Change());
				}

		// The stopping condition is here so that the display of a
		// bookmark can take place at paragraph start too.
		if (i >= end)
			break;

		char_type c = par.getChar(i);
		// The most special cases are handled first.
		if (par.isInset(i)) {
			Inset const * ins = par.getInset(i);
			Dimension dim = bv_->coordCache().insets().dim(ins);
			row.add(i, ins, dim, *fi, par.lookupChange(i));
		} else if (c == ' ' && i + 1 == body_pos) {
			// This space is an \item separator. Represent it with a
			// special space element, which dimension will be computed
			// in breakRow.
			FontMetrics const & fm = theFontMetrics(text_->labelFont(par));
			int const wid = fm.width(par.layout().labelsep);
			row.addMarginSpace(i, wid, *fi, par.lookupChange(i));
		} else if (c == '\t')
			row.addSpace(i, theFontMetrics(*fi).width(from_ascii("    ")),
			             *fi, par.lookupChange(i));
		else if (c == 0x2028 || c == 0x2029) {
			/**
			 * U+2028 LINE SEPARATOR
			 * U+2029 PARAGRAPH SEPARATOR

			 * These are special unicode characters that break
			 * lines/pragraphs. Not handling them leads to trouble wrt
			 * Qt QTextLayout formatting. We add a visible character
			 * on screen so that the user can see that something is
			 * happening.
			*/
			row.finalizeLast();
			// ⤶ U+2936 ARROW POINTING DOWNWARDS THEN CURVING LEFTWARDS
			// ¶ U+00B6 PILCROW SIGN
			char_type const screen_char = (c == 0x2028) ? 0x2936 : 0x00B6;
			row.add(i, screen_char, *fi, par.lookupChange(i));
		} else
			// row elements before body are unbreakable
			row.add(i, c, *fi, par.lookupChange(i));

		// add inline completion width
		// draw logically behind the previous character
		if (ic_pos == i + 1 && !bv_->inlineCompletion().empty()) {
			docstring const comp = bv_->inlineCompletion();
			size_t const uniqueTo =bv_->inlineCompletionUniqueChars();
			Font f = *fi;

			if (uniqueTo > 0) {
				f.fontInfo().setColor(Color_inlinecompletion);
				row.addVirtual(i + 1, comp.substr(0, uniqueTo), f, Change());
			}
			f.fontInfo().setColor(Color_nonunique_inlinecompletion);
			row.addVirtual(i + 1, comp.substr(uniqueTo), f, Change());
		}

		++i;
		++fi;
	}
	row.finalizeLast();
	row.endpos(end);

	// End of paragraph marker, either if LyXRc requires it, or there
	// is an end of paragraph change. The logic here is almost the
	// same as in redoParagraph, remember keep them in sync.
	ParagraphList const & pars = text_->paragraphs();
	Change const & endchange = par.lookupChange(end);
	if (endchange.changed())
		row.needsChangeBar(true);
	if ((lyxrc.paragraph_markers || endchange.changed())
	    && size_type(pit + 1) < pars.size()) {
		// add a virtual element for the end-of-paragraph
		// marker; it is shown on screen, but does not exist
		// in the paragraph.
		Font f(text_->layoutFont(pit));
		f.fontInfo().setColor(Color_paragraphmarker);
		f.setLanguage(par.getParLanguage(buf.params()));
		// ¶ U+00B6 PILCROW SIGN
		row.addVirtual(end, docstring(1, char_type(0x00B6)), f, endchange);
	}

	return row;
}


namespace {

/** Helper template flexible_const_iterator<T>
 * A way to iterate over a const container, but insert fake elements in it.
 * In the case of a row, we will have to break some elements, which
 * create new ones. This class allows to abstract this.
 * Only the required parts are implemented for now.
 */
template<class T>
class flexible_const_iterator {
	typedef typename T::value_type value_type;
public:

	//
	flexible_const_iterator & operator++() {
		if (pile_.empty())
			++cit_;
		else
			pile_.pop_back();
		return *this;
	}

	value_type operator*() const { return pile_.empty() ? *cit_ : pile_.back(); }

	value_type const * operator->() const { return pile_.empty() ? &*cit_ : &pile_.back(); }

	void put(value_type const & e) { pile_.push_back(e); }

	// Put a sequence of elements on the pile (in reverse order!)
	void put(vector<value_type> const & elts) {
		pile_.insert(pile_.end(), elts.rbegin(), elts.rend());
	}

// This should be private, but declaring the friend functions is too much work
//private:
	typename T::const_iterator cit_;
	// A vector that is used as like a pile to store the elements to
	// consider before incrementing the underlying iterator.
	vector<value_type> pile_;
};


template<class T>
flexible_const_iterator<T> flexible_begin(T const & t)
{
	return { t.begin(), vector<typename T::value_type>() };
}


template<class T>
flexible_const_iterator<T> flexible_end(T const & t)
{
	return { t.end(), vector<typename T::value_type>() };
}


// Equality is only possible if respective piles are empty
template<class T>
bool operator==(flexible_const_iterator<T> const & t1,
                flexible_const_iterator<T> const & t2)
{
	return t1.cit_ == t2.cit_ && t1.pile_.empty() && t2.pile_.empty();
}


Row newRow(TextMetrics const & tm, pit_type pit, pos_type pos, bool is_rtl)
{
	Row nrow;
	nrow.pit(pit);
	nrow.pos(pos);
	nrow.left_margin = tm.leftMargin(pit, pos);
	nrow.right_margin = tm.rightMargin(pit);
	nrow.setRTL(is_rtl);
	if (is_rtl)
		swap(nrow.left_margin, nrow.right_margin);
	// Remember that the row width takes into account the left_margin
	// but not the right_margin.
	nrow.dim().wid = nrow.left_margin;
	return nrow;
}


void cleanupRow(Row & row, bool at_end)
{
	if (row.empty()) {
		row.endpos(row.pos());
		return;
	}

	row.endpos(row.back().endpos);
	// remove trailing spaces on row break
	if (!at_end && !row.flushed())
		row.back().rtrim();
	// boundary exists when there was no space at the end of row
	row.end_boundary(!at_end && row.back().endpos == row.endpos());
	// make sure that the RTL elements are in reverse ordering
	row.reverseRTL();
}


// Implement the priorities described in RowFlags.h.
bool needsRowBreak(int f1, int f2)
{
	if (f1 & AlwaysBreakAfter /*|| f2 & AlwaysBreakBefore*/)
		return true;
	if (f1 & NoBreakAfter || f2 & NoBreakBefore)
		return false;
	if (f1 & BreakAfter || f2 & BreakBefore)
		return true;
	return false;
}


}


RowList TextMetrics::breakParagraph(Row const & bigrow) const
{
	RowList rows;
	bool const is_rtl = text_->isRTL(bigrow.pit());
	bool const end_label = text_->getEndLabel(bigrow.pit()) != END_LABEL_NO_LABEL;
	int const next_width = max_width_ - leftMargin(bigrow.pit(), bigrow.endpos())
		- rightMargin(bigrow.pit());

	int width = 0;
	flexible_const_iterator<Row> fcit = flexible_begin(bigrow);
	flexible_const_iterator<Row> const end = flexible_end(bigrow);
	while (true) {
		bool const row_empty = rows.empty() || rows.back().empty();
		// The row flags of previous element, if there is one.
		// Otherwise we use NoBreakAfter to avoid an empty row before
		// e.g. a displayed equation.
		int const f1 = row_empty ? NoBreakAfter : rows.back().back().row_flags;
		// The row flags of next element, if there is one.
		// Otherwise we use NoBreakBefore (see above), unless the
		// paragraph has an end label (for which an empty row is OK).
		int const f2 = (fcit == end) ? (end_label ? Inline : NoBreakBefore)
		                             : fcit->row_flags;
		if (rows.empty() || needsRowBreak(f1, f2)) {
			if (!rows.empty()) {
				// Flush row as requested by row flags
				rows.back().flushed((f1 & Flush) || (f2 & FlushBefore));
				cleanupRow(rows.back(), false);
			}
			pos_type pos = rows.empty() ? 0 : rows.back().endpos();
			rows.push_back(newRow(*this, bigrow.pit(), pos, is_rtl));
			// the width available for the row.
			width = max_width_ - rows.back().right_margin;
		}

		// The stopping condition is here because we may need a new
		// empty row at the end.
		if (fcit == end)
			break;

		// Next element to consider is either the top of the temporary
		// pile, or the place when we were in main row
		Row::Element elt = *fcit;
		Row::Elements tail;
		elt.splitAt(width - rows.back().width(), next_width, Row::FIT, tail);
		Row & rb = rows.back();
		if (elt.type == Row::MARGINSPACE)
			elt.dim.wid = max(elt.dim.wid, leftMargin(bigrow.pit()) - rb.width());
		rb.push_back(elt);
		rb.finalizeLast();
		if (rb.width() > width) {
			// Keep the tail for later; this ought to be rare, but play safe.
			if (!tail.empty())
				fcit.put(tail);
			// if the row is too large, try to cut at last separator.
			tail = rb.shortenIfNeeded(width, next_width);
		}

		// Go to next element
		++fcit;

		// Handle later the elements returned by splitAt or shortenIfNeeded.
		fcit.put(tail);
	}

	if (!rows.empty()) {
		// Last row in paragraph is flushed
		rows.back().flushed(true);
		cleanupRow(rows.back(), true);
		// Is there an end-of-paragraph change?
		if (bigrow.needsChangeBar())
			rows.back().needsChangeBar(true);
	}

	return rows;
}


int TextMetrics::parTopSpacing(pit_type const pit) const
{
	Paragraph const & par = text_->getPar(pit);
	Layout const & layout = par.layout();

	int asc = 0;
	ParagraphList const & pars = text_->paragraphs();
	double const dh = defaultRowHeight();

	BufferParams const & bparams = bv_->buffer().params();
	Inset const & inset = text_->inset();
	// some parskips VERY EASY IMPLEMENTATION
	if (bparams.paragraph_separation == BufferParams::ParagraphSkipSeparation
		&& !inset.getLayout().parbreakIsNewline()
		&& !par.layout().parbreak_is_newline
		&& pit > 0
		&& ((layout.isParagraph() && par.getDepth() == 0)
		    || (pars[pit - 1].layout().isParagraph()
		        && pars[pit - 1].getDepth() == 0))) {
		asc += bparams.getDefSkip().inPixels(*bv_);
	}

	if (par.params().startOfAppendix())
		asc += int(3 * dh);

	// special code for the top label
	if (layout.labelIsAbove()
	    && (!layout.isParagraphGroup() || text_->isFirstInSequence(pit))
	    && !par.labelString().empty()) {
		FontInfo labelfont = text_->labelFont(par);
		FontMetrics const & lfm = theFontMetrics(labelfont);
		asc += int(lfm.maxHeight() * layout.spacing.getValue()
		                           * text_->spacing(par)
		           + (layout.topsep + layout.labelbottomsep) * dh);
	}

	// Add the layout spaces, for example before and after
	// a section, or between the items of a itemize or enumerate
	// environment.

	pit_type prev = text_->depthHook(pit, par.getDepth());
	Paragraph const & prevpar = pars[prev];
	double layoutasc = 0;
	if (prev != pit
	    && prevpar.layout() == layout
	    && prevpar.getDepth() == par.getDepth()
	    && prevpar.getLabelWidthString() == par.getLabelWidthString()) {
		layoutasc = layout.itemsep * dh;
	} else if (pit != 0 && layout.topsep > 0)
		// combine the separation between different layouts (with same depth)
		layoutasc = max(0.0,
			prevpar.getDepth() != par.getDepth() ? layout.topsep
			: layout.topsep - prevpar.layout().bottomsep) * dh;

	asc += int(layoutasc * 2 / (2 + pars[pit].getDepth()));

	prev = text_->outerHook(pit);
	if (prev != pit_type(pars.size())) {
		asc += int(pars[prev].layout().parsep * dh);
	} else if (pit != 0) {
		Paragraph const & prevpar2 = pars[pit - 1];
		if (prevpar2.getDepth() != 0 || prevpar2.layout() == layout)
			asc += int(layout.parsep * dh);
	}

	return asc;
}


int TextMetrics::parBottomSpacing(pit_type const pit) const
{
	double layoutdesc = 0;
	ParagraphList const & pars = text_->paragraphs();
	double const dh = defaultRowHeight();

	// add the layout spaces, for example before and after
	// a section, or between the items of a itemize or enumerate
	// environment
	pit_type nextpit = pit + 1;
	if (nextpit != pit_type(pars.size())) {
		pit_type cpit = pit;

		if (pars[cpit].getDepth() > pars[nextpit].getDepth()) {
			double usual = pars[cpit].layout().bottomsep * dh;
			double unusual = 0;
			cpit = text_->depthHook(cpit, pars[nextpit].getDepth());
			if (pars[cpit].layout() != pars[nextpit].layout()
				|| pars[nextpit].getLabelWidthString() != pars[cpit].getLabelWidthString())
				unusual = pars[cpit].layout().bottomsep * dh;
			layoutdesc = max(unusual, usual);
		} else if (pars[cpit].getDepth() == pars[nextpit].getDepth()) {
			if (pars[cpit].layout() != pars[nextpit].layout()
				|| pars[nextpit].getLabelWidthString() != pars[cpit].getLabelWidthString())
				layoutdesc = int(pars[cpit].layout().bottomsep * dh);
		}
	}

	return int(layoutdesc * 2 / (2 + pars[pit].getDepth()));
}


void TextMetrics::setRowHeight(Row & row) const
{
	Paragraph const & par = text_->getPar(row.pit());
	Layout const & layout = par.layout();
	// leading space (line spacing) factor based on current paragraph
	double spacing_val = layout.spacing.getValue() * text_->spacing(par);

	// if this is the first row but not the first paragraph, take into
	// account the spacing of the previous paragraph.
	if (row.pos() == 0 && row.pit() > 0) {
		// for the first row in the paragraph,
		// use previous paragraphs line spacing if it is larger
		Paragraph const & previousPar = text_->getPar(row.pit() - 1);
		Layout const & previousLayout = previousPar.layout();
		// leading space factor based on previous paragraph
		double const previous_spacing_val
			= previousLayout.spacing.getValue() * text_->spacing(previousPar);
		if (previous_spacing_val > spacing_val)
			spacing_val = previous_spacing_val;
	}

	// Initial value for ascent (useful if row is empty).
	Font const font = displayFont(row.pit(), row.pos());
	FontMetrics const & fm = theFontMetrics(font);
	int maxasc = int(fm.maxAscent()
		// add leading space
		+ fm.maxHeight() * (spacing_val - 1));
	int maxdes = int(fm.maxDescent());

	// Take label string into account (useful if labelfont is large)
	if (row.pos() == 0 && layout.labelIsInline()) {
		FontInfo const labelfont = text_->labelFont(par);
		FontMetrics const & lfm = theFontMetrics(labelfont);
		maxasc = max(maxasc, int(lfm.maxAscent()
			// add leading space
			+ lfm.maxHeight() * (spacing_val - 1)));
		maxdes = max(maxdes, int(lfm.maxDescent()));
	}

	// Find the ascent/descent of the row contents
	for (Row::Element const & e : row) {
		if (e.inset) {
			maxasc = max(maxasc, e.dim.ascent());
			maxdes = max(maxdes, e.dim.descent());
		} else {
			FontMetrics const & fm2 = theFontMetrics(e.font);
			maxasc = max(maxasc, int(fm2.maxAscent()
				// add leading space
				+ fm2.maxHeight() * (spacing_val - 1)));
			maxdes = max(maxdes, int(fm2.maxDescent()));
		}
	}

	// This is nicer with box insets
	++maxasc;
	++maxdes;

	row.dim().asc = maxasc;
	row.dim().des = maxdes;

	// This is useful for selections
	row.contents_dim() = row.dim();
}


// x is an absolute screen coord
// returns the column near the specified x-coordinate of the row
// x is set to the real beginning of this column
pos_type TextMetrics::getPosNearX(Row const & row, int & x,
				  bool & boundary) const
{
	//LYXERR0("getPosNearX(" << x << ") row=" << row);
	/// For the main Text, it is possible that this pit is not
	/// yet in the CoordCache when moving cursor up.
	/// x Paragraph coordinate is always 0 for main text anyway.
	int const xo = origin_.x_;
	x -= xo;

	// Adapt to cursor row scroll offset if applicable.
	int const offset = bv_->horizScrollOffset(text_, row.pit(), row.pos());
	x += offset;

	pos_type pos = row.pos();
	boundary = false;
	if (row.empty())
		x = row.left_margin;
	else if (x <= row.left_margin) {
		pos = row.front().left_pos();
		x = row.left_margin;
	} else if (x >= row.width()) {
		pos = row.back().right_pos();
		x = row.width();
	} else {
		double w = row.left_margin;
		Row::const_iterator cit = row.begin();
		Row::const_iterator cend = row.end();
		for ( ; cit != cend; ++cit) {
			if (w <= x &&  w + cit->full_width() > x) {
				int x_offset = int(x - w);
				pos = cit->x2pos(x_offset);
				x = int(x_offset + w);
				break;
			}
			w += cit->full_width();
		}
		if (cit == row.end()) {
			pos = row.back().right_pos();
			x = row.width();
		}
		/** This tests for the case where the cursor is placed
		 * just before a font direction change. See comment on
		 * the boundary_ member in DocIterator.h to understand
		 * how boundary helps here.
		 */
		else if (pos == cit->endpos
		         && ((!cit->isRTL() && cit + 1 != row.end()
		              && (cit + 1)->isRTL())
		             || (cit->isRTL() && cit != row.begin()
		                 && !(cit - 1)->isRTL())))
			boundary = true;
	}

	/** This tests for the case where the cursor is set at the end
	 * of a row which has been broken due something else than a
	 * separator (a display inset or a forced breaking of the
	 * row). We know that there is a separator when the end of the
	 * row is larger than the end of its last element.
	 */
	if (!row.empty() && pos == row.back().endpos
	    && row.back().endpos == row.endpos()) {
		Inset const * inset = row.back().inset;
		if (inset && (inset->lyxCode() == NEWLINE_CODE
		              || inset->lyxCode() == SEPARATOR_CODE))
			pos = row.back().pos;
		else
			boundary = row.end_boundary();
	}

	x += xo - offset;
	//LYXERR0("getPosNearX ==> pos=" << pos << ", boundary=" << boundary);

	return pos;
}


pos_type TextMetrics::x2pos(pit_type pit, int row, int x) const
{
	// We play safe and use parMetrics(pit) to make sure the
	// ParagraphMetrics will be redone and OK to use if needed.
	// Otherwise we would use an empty ParagraphMetrics in
	// upDownInText() while in selection mode.
	ParagraphMetrics const & pm = parMetrics(pit);

	LBUFERR(row < int(pm.rows().size()));
	bool bound = false;
	Row const & r = pm.rows()[row];
	return getPosNearX(r, x, bound);
}


// y is screen coordinate
pit_type TextMetrics::getPitNearY(int y)
{
	LASSERT(!text_->paragraphs().empty(), return -1);
	LASSERT(!par_metrics_.empty(), return -1);
	LYXERR(Debug::PAINTING, "y: " << y << " cache size: " << par_metrics_.size());

	// look for highest numbered paragraph with y coordinate less than given y
	pit_type pit = -1;
	int yy = -1;
	ParMetricsCache::const_iterator it = par_metrics_.begin();
	ParMetricsCache::const_iterator et = par_metrics_.end();
	ParMetricsCache::const_iterator last = et;
	--last;

	if (y < it->second.top()) {
		// We are looking for a position that is before the first paragraph in
		// the cache (which is in priciple off-screen, that is before the
		// visible part.
		if (it->first == 0)
			// We are already at the first paragraph in the inset.
			return 0;
		// OK, this is the paragraph we are looking for.
		pit = it->first - 1;
		newParMetricsUp();
		return pit;
	}

	if (y >= par_metrics_[last->first].bottom()) {
		// We are looking for a position that is after the last paragraph in
		// the cache (which is in priciple off-screen), that is before the
		// visible part.
		pit = last->first + 1;
		if (pit == int(text_->paragraphs().size()))
			//  We are already at the last paragraph in the inset.
			return last->first;
		// OK, this is the paragraph we are looking for.
		newParMetricsDown();
		return pit;
	}

	for (; it != et; ++it) {
		LYXERR(Debug::PAINTING, "examining: pit: " << it->first
			<< " y: " << it->second.position());

		if (it->first >= pit && it->second.top() <= y) {
			pit = it->first;
			yy = it->second.position();
		}
	}

	LYXERR(Debug::PAINTING, "found best y: " << yy << " for pit: " << pit);

	return pit;
}


Row const & TextMetrics::getPitAndRowNearY(int & y, pit_type & pit,
	bool assert_in_view, bool up)
{
	ParagraphMetrics const & pm = par_metrics_[pit];

	int yy = pm.top();
	LBUFERR(!pm.rows().empty());
	RowList::const_iterator rit = pm.rows().begin();
	RowList::const_iterator rlast = pm.rows().end();
	--rlast;
	for (; rit != rlast; yy += rit->height(), ++rit)
		if (yy + rit->height() > y)
			break;

	if (assert_in_view) {
		if (!up && yy + rit->height() > y) {
			if (rit != pm.rows().begin()) {
				y = yy;
				--rit;
			} else if (pit != 0) {
				--pit;
				newParMetricsUp();
				ParagraphMetrics const & pm2 = par_metrics_[pit];
				rit = pm2.rows().end();
				--rit;
				y = yy;
			}
		} else if (up && yy != y) {
			if (rit != rlast) {
				y = yy + rit->height();
				++rit;
			} else if (pit < int(text_->paragraphs().size()) - 1) {
				++pit;
				newParMetricsDown();
				ParagraphMetrics const & pm2 = par_metrics_[pit];
				rit = pm2.rows().begin();
				y = pm2.position();
			}
		}
	}
	return *rit;
}


// x,y are absolute screen coordinates
// sets cursor recursively descending into nested editable insets
Inset * TextMetrics::editXY(Cursor & cur, int x, int y,
	bool assert_in_view, bool up)
{
	if (lyxerr.debugging(Debug::WORKAREA)) {
		LYXERR0("TextMetrics::editXY(cur, " << x << ", " << y << ")");
		cur.bv().coordCache().dump();
	}
	pit_type pit = getPitNearY(y);
	LASSERT(pit != -1, return 0);
	Row const & row = getPitAndRowNearY(y, pit, assert_in_view, up);
	cur.pit() = pit;

	// Do we cover an inset?
	InsetList::Element * e = checkInsetHit(pit, x, y);

	if (!e) {
		// No inset, set position in the text
		bool bound = false; // is modified by getPosNearX
		cur.pos() = getPosNearX(row, x, bound);
		cur.boundary(bound);
		cur.setCurrentFont();
		cur.setTargetX(x);
		return 0;
	}

	Inset * inset = e->inset;
	//lyxerr << "inset " << inset << " hit at x: " << x << " y: " << y << endl;

	// Set position in front of inset
	cur.pos() = e->pos;
	cur.boundary(false);
	cur.setTargetX(x);

	// Try to descend recursively inside the inset.
	Inset * edited = inset->editXY(cur, x, y);
	// FIXME: it is not clear that the test on position is needed
	// Remove it if/when semantics of editXY is clarified
	if (cur.text() == text_ && cur.pos() == e->pos) {
		// non-editable inset, set cursor after the inset if x is
		// nearer to that position (bug 9628)
		bool bound = false; // is modified by getPosNearX
		cur.pos() = getPosNearX(row, x, bound);
		cur.boundary(bound);
		cur.setCurrentFont();
		cur.setTargetX(x);
	}

	if (cur.top().text() == text_)
		cur.setCurrentFont();
	return edited;
}


void TextMetrics::setCursorFromCoordinates(Cursor & cur, int const x, int const y)
{
	LASSERT(text_ == cur.text(), return);
	pit_type const pit = getPitNearY(y);
	LASSERT(pit != -1, return);

	ParagraphMetrics const & pm = par_metrics_[pit];

	int yy = pm.position() - pm.rows().front().ascent();
	LYXERR(Debug::PAINTING, "x: " << x << " y: " << y <<
		" pit: " << pit << " yy: " << yy);

	int r = 0;
	LBUFERR(pm.rows().size());
	for (; r < int(pm.rows().size()) - 1; ++r) {
		Row const & row = pm.rows()[r];
		if (yy + row.height() > y)
			break;
		yy += row.height();
	}

	Row const & row = pm.rows()[r];

	LYXERR(Debug::PAINTING, "row " << r << " from pos: " << row.pos());

	bool bound = false;
	int xx = x;
	pos_type const pos = getPosNearX(row, xx, bound);

	LYXERR(Debug::PAINTING, "setting cursor pit: " << pit << " pos: " << pos);

	text_->setCursor(cur, pit, pos, true, bound);
	// remember new position.
	cur.setTargetX();
}


//takes screen x,y coordinates
InsetList::Element * TextMetrics::checkInsetHit(pit_type pit, int x, int y)
{
	Paragraph const & par = text_->paragraphs()[pit];
	CoordCache::Insets const & insetCache = bv_->coordCache().getInsets();

	LYXERR(Debug::PAINTING, "x: " << x << " y: " << y << "  pit: " << pit);

	for (InsetList::Element const & e : par.insetList()) {
		LYXERR(Debug::PAINTING, "examining inset " << e.inset);

		if (insetCache.covers(e.inset, x, y)) {
			LYXERR(Debug::PAINTING, "Hit inset: " << e.inset);
			return const_cast<InsetList::Element *>(&e);
		}
	}

	LYXERR(Debug::PAINTING, "No inset hit. ");
	return nullptr;
}


//takes screen x,y coordinates
Inset * TextMetrics::checkInsetHit(int x, int y)
{
	pit_type const pit = getPitNearY(y);
	LASSERT(pit != -1, return 0);
	InsetList::Element * e = checkInsetHit(pit, x, y);

	if (!e)
		return 0;

	return e->inset;
}


int TextMetrics::cursorX(CursorSlice const & sl,
		bool boundary) const
{
	LASSERT(sl.text() == text_, return 0);

	ParagraphMetrics const & pm = par_metrics_[sl.pit()];
	if (pm.rows().empty())
		return 0;
	Row const & row = pm.getRow(sl.pos(), boundary);
	pos_type const pos = sl.pos();

	double x = 0;
	row.findElement(pos, boundary, x);
	return int(x);

}


int TextMetrics::cursorY(CursorSlice const & sl, bool boundary) const
{
	//lyxerr << "TextMetrics::cursorY: boundary: " << boundary << endl;
	ParagraphMetrics const & pm = parMetrics(sl.pit());
	if (pm.rows().empty())
		return 0;

	int h = 0;
	h -= parMetrics(0).rows()[0].ascent();
	for (pit_type pit = 0; pit < sl.pit(); ++pit) {
		h += parMetrics(pit).height();
	}
	int pos = sl.pos();
	if (pos && boundary)
		--pos;
	size_t const rend = pm.pos2row(pos);
	for (size_t rit = 0; rit != rend; ++rit)
		h += pm.rows()[rit].height();
	h += pm.rows()[rend].ascent();
	return h;
}


// the cursor set functions have a special mechanism. When they
// realize you left an empty paragraph, they will delete it.

bool TextMetrics::cursorHome(Cursor & cur)
{
	LASSERT(text_ == cur.text(), return false);
	ParagraphMetrics const & pm = par_metrics_[cur.pit()];
	Row const & row = pm.getRow(cur.pos(),cur.boundary());
	return text_->setCursor(cur, cur.pit(), row.pos());
}


bool TextMetrics::cursorEnd(Cursor & cur)
{
	LASSERT(text_ == cur.text(), return false);
	// if not on the last row of the par, put the cursor before
	// the final space exept if I have a spanning inset or one string
	// is so long that we force a break.
	pos_type end = cur.textRow().endpos();
	if (end == 0)
		// empty text, end-1 is no valid position
		return false;
	bool boundary = false;
	if (end != cur.lastpos()) {
		if (!cur.paragraph().isLineSeparator(end-1)
		    && !cur.paragraph().isNewline(end-1)
		    && !cur.paragraph().isEnvSeparator(end-1))
			boundary = true;
		else
			--end;
	} else if (cur.paragraph().isEnvSeparator(end-1))
		--end;
	return text_->setCursor(cur, cur.pit(), end, true, boundary);
}


void TextMetrics::deleteLineForward(Cursor & cur)
{
	LASSERT(text_ == cur.text(), return);
	if (cur.lastpos() == 0) {
		// Paragraph is empty, so we just go forward
		text_->cursorForward(cur);
	} else {
		cur.resetAnchor();
		cur.selection(true); // to avoid deletion
		cursorEnd(cur);
		cur.setSelection();
		// What is this test for ??? (JMarc)
		if (!cur.selection())
			text_->deleteWordForward(cur);
		else
			cap::cutSelection(cur, false);
		cur.checkBufferStructure();
	}
}


int TextMetrics::leftMargin(pit_type pit) const
{
	// FIXME: what is the semantics? It depends on whether the
	// paragraph is empty!
	return leftMargin(pit, text_->paragraphs()[pit].size());
}


int TextMetrics::leftMargin(pit_type const pit, pos_type const pos) const
{
	ParagraphList const & pars = text_->paragraphs();

	LASSERT(pit >= 0, return 0);
	LASSERT(pit < int(pars.size()), return 0);
	Paragraph const & par = pars[pit];
	LASSERT(pos >= 0, return 0);
	// We do not really care whether pos > par.size(), since we do not
	// access the data. It can be actually useful, when querying the
	// margin without indentation (see leftMargin(pit_type).

	Buffer const & buffer = bv_->buffer();
	//lyxerr << "TextMetrics::leftMargin: pit: " << pit << " pos: " << pos << endl;
	DocumentClass const & tclass = buffer.params().documentClass();
	Layout const & layout = par.layout();
	FontMetrics const & bfm = theFontMetrics(buffer.params().getFont());

	docstring parindent = layout.parindent;

	int l_margin = 0;

	if (text_->isMainText()) {
		l_margin += bv_->leftMargin();
		l_margin += bfm.signedWidth(tclass.leftmargin());
	}

	int depth = par.getDepth();
	if (depth != 0) {
		// find the next level paragraph
		pit_type newpar = text_->outerHook(pit);
		if (newpar != pit_type(pars.size())) {
			if (pars[newpar].layout().isEnvironment()) {
				int nestmargin = depth * nestMargin();
				if (text_->isMainText())
					nestmargin += changebarMargin();
				l_margin = max(leftMargin(newpar), nestmargin);
				// Remove the parindent that has been added
				// if the paragraph was empty.
				if (pars[newpar].empty() &&
				    buffer.params().paragraph_separation ==
				    BufferParams::ParagraphIndentSeparation) {
					docstring pi = pars[newpar].layout().parindent;
					l_margin -= bfm.signedWidth(pi);
				}
			}
			if (tclass.isDefaultLayout(par.layout())
			    || tclass.isPlainLayout(par.layout())) {
				if (pars[newpar].params().noindent())
					parindent.erase();
				else
					parindent = pars[newpar].layout().parindent;
			}
		}
	}

	// Check for reasons to remove indentation.
	// First, at document level.
	if (buffer.params().paragraph_separation ==
			BufferParams::ParagraphSkipSeparation)
		parindent.erase();
	// This happens after sections or environments in standard classes.
	// We have to check the previous layout at same depth.
	else if (pit > 0 && pars[pit - 1].getDepth() >= par.getDepth()) {
		pit_type prev = text_->depthHook(pit, par.getDepth());
		if (par.layout() == pars[prev].layout()) {
			if (prev != pit - 1
			    && pars[pit - 1].layout().nextnoindent)
				parindent.erase();
		} else if (pars[prev].layout().nextnoindent)
			parindent.erase();
	}
	// The previous paragraph may have ended with a separator inset.
	if (pit > 0) {
		Paragraph const & ppar = pars[pit - 1];
		if (ppar.size() > 0) {
			auto * in = dynamic_cast<InsetSeparator const *>(ppar.getInset(ppar.size() - 1));
			if (in != nullptr && in->nextnoindent())
				parindent.erase();
		}
	}

	FontInfo const labelfont = text_->labelFont(par);
	FontMetrics const & lfm = theFontMetrics(labelfont);

	switch (layout.margintype) {
	case MARGIN_DYNAMIC:
		if (!layout.leftmargin.empty()) {
			l_margin += bfm.signedWidth(layout.leftmargin);
		}
		if (!par.labelString().empty()) {
			l_margin += lfm.signedWidth(layout.labelindent);
			l_margin += lfm.width(par.labelString());
			l_margin += lfm.width(layout.labelsep);
		}
		break;

	case MARGIN_MANUAL: {
		l_margin += lfm.signedWidth(layout.labelindent);
		// The width of an empty par, even with manual label, should be 0
		if (!par.empty() && pos >= par.beginOfBody()) {
			if (!par.getLabelWidthString().empty()) {
				docstring labstr = par.getLabelWidthString();
				l_margin += lfm.width(labstr);
				l_margin += lfm.width(layout.labelsep);
			}
		}
		break;
	}

	case MARGIN_STATIC: {
		l_margin += bfm.signedWidth(layout.leftmargin) * 4
		             / (par.getDepth() + 4);
		break;
	}

	case MARGIN_FIRST_DYNAMIC:
		if (layout.labeltype == LABEL_MANUAL) {
			// if we are at position 0, we are never in the body
			if (pos > 0 && pos >= par.beginOfBody())
				l_margin += lfm.signedWidth(layout.leftmargin);
			else
				l_margin += lfm.signedWidth(layout.labelindent);
		} else if (pos != 0
			   // Special case to fix problems with
			   // theorems (JMarc)
			   || (layout.labeltype == LABEL_STATIC
			       && layout.latextype == LATEX_ENVIRONMENT
			       && !text_->isFirstInSequence(pit))) {
			l_margin += lfm.signedWidth(layout.leftmargin);
		} else if (!layout.labelIsAbove()) {
			l_margin += lfm.signedWidth(layout.labelindent);
			l_margin += lfm.width(layout.labelsep);
			l_margin += lfm.width(par.labelString());
		}
		break;

	case MARGIN_RIGHT_ADDRESS_BOX:
		// This is handled globally in redoParagraph().
		break;
	}

	if (!par.params().leftIndent().zero())
		l_margin += par.params().leftIndent().inPixels(max_width_, lfm.em());

	LyXAlignment align = par.getAlign(bv_->buffer().params());

	// set the correct parindent
	if (pos == 0
	    && (layout.labeltype == LABEL_NO_LABEL
	        || layout.labeltype == LABEL_ABOVE
	        || layout.labeltype == LABEL_CENTERED
	        || (layout.labeltype == LABEL_STATIC
	            && layout.latextype == LATEX_ENVIRONMENT
	            && !text_->isFirstInSequence(pit)))
	    && (align == LYX_ALIGN_BLOCK || align == LYX_ALIGN_LEFT)
	    && !par.params().noindent()
	    // in some insets, paragraphs are never indented
	    && !text_->inset().neverIndent()
	    // display style insets do not need indentation
	    && !(!par.empty()
	         && par.isInset(0)
	         && par.getInset(0)->rowFlags() & Display)
	    && (!(tclass.isDefaultLayout(par.layout())
	        || tclass.isPlainLayout(par.layout()))
	        || buffer.params().paragraph_separation
				== BufferParams::ParagraphIndentSeparation)) {
		/* use the parindent of the layout when the default
		 * indentation is used otherwise use the indentation set in
		 * the document settings
		 */
		if (buffer.params().getParIndent().empty())
			l_margin += bfm.signedWidth(parindent);
		else
			l_margin += buffer.params().getParIndent().inPixels(max_width_, bfm.em());
	}

	return l_margin;
}


void TextMetrics::draw(PainterInfo & pi, int x, int y) const
{
	if (par_metrics_.empty())
		return;

	origin_.x_ = x;
	origin_.y_ = y;

	y -= par_metrics_.begin()->second.ascent();
	for (auto & pm_pair : par_metrics_) {
		pit_type const pit = pm_pair.first;
		ParagraphMetrics & pm = pm_pair.second;
		y += pm.ascent();
		// Save the paragraph position in the cache.
		pm.setPosition(y);
		drawParagraph(pi, pit, x, y);
		y += pm.descent();
	}
}


void TextMetrics::drawParagraph(PainterInfo & pi, pit_type const pit, int const x, int y) const
{
	ParagraphMetrics const & pm = par_metrics_[pit];
	if (pm.rows().empty())
		return;
	size_t const nrows = pm.rows().size();
	int const wh = bv_->workHeight();
	// Remember left and right margin for drawing math numbers
	Changer changeleft = changeVar(pi.leftx, x + leftMargin(pit));
	Changer changeright = changeVar(pi.rightx, x + width() - rightMargin(pit));

	// Use fast lane in nodraw stage.
	if (pi.pain.isNull()) {
		for (size_t i = 0; i != nrows; ++i) {

			Row const & row = pm.rows()[i];
			// Adapt to cursor row scroll offset if applicable.
			int row_x = x - bv_->horizScrollOffset(text_, pit, row.pos());
			if (i)
				y += row.ascent();

			// It is not needed to draw on screen if we are not inside
			bool const inside = (y + row.descent() >= 0 && y - row.ascent() < wh);
			if (inside) {
				RowPainter rp(pi, *text_, row, row_x, y);
				rp.paintOnlyInsets();
			}
			y += row.descent();
		}
		return;
	}

	Cursor const & cur = bv_->cursor();
	DocIterator sel_beg = cur.selectionBegin();
	DocIterator sel_end = cur.selectionEnd();
	bool selection = cur.selection()
		// This is our text.
		&& cur.text() == text_
		// if the anchor is outside, this is not our selection
		&& cur.normalAnchor().text() == text_
		&& pit >= sel_beg.pit() && pit <= sel_end.pit();

	// We store the begin and end pos of the selection relative to this par
	DocIterator sel_beg_par = cur.selectionBegin();
	DocIterator sel_end_par = cur.selectionEnd();

	// We care only about visible selection.
	if (selection) {
		if (pit != sel_beg.pit()) {
			sel_beg_par.pit() = pit;
			sel_beg_par.pos() = 0;
		}
		if (pit != sel_end.pit()) {
			sel_end_par.pit() = pit;
			sel_end_par.pos() = sel_end_par.lastpos();
		}
	}

	if (text_->isRTL(pit))
		swap(pi.leftx, pi.rightx);

	BookmarksSection::BookmarkPosList bpl =
		theSession().bookmarks().bookmarksInPar(bv_->buffer().fileName(), pm.id());

	for (size_t i = 0; i != nrows; ++i) {

		Row const & row = pm.rows()[i];
		// Adapt to cursor row scroll offset if applicable.
		int row_x = x - bv_->horizScrollOffset(text_, pit, row.pos());
		if (i)
			y += row.ascent();

		// It is not needed to draw on screen if we are not inside.
		bool const inside = (y + row.descent() >= 0
			&& y - row.ascent() < wh);
		if (!inside) {
			// Inset positions have already been set in nodraw stage.
			y += row.descent();
			continue;
		}

		if (selection)
			row.setSelectionAndMargins(sel_beg_par, sel_end_par);
		else
			row.clearSelectionAndMargins();

		// The row knows nothing about the paragraph, so we have to check
		// whether this row is the first or last and update the margins.
		if (row.selection()) {
			if (row.sel_beg == 0)
				row.change(row.begin_margin_sel, sel_beg.pit() < pit);
			if (row.sel_end == sel_end_par.lastpos())
				row.change(row.end_margin_sel, sel_end.pit() > pit);
		}

		// Take this opportunity to spellcheck the row contents.
		if (row.changed() && pi.do_spellcheck && lyxrc.spellcheck_continuously) {
			text_->getPar(pit).spellCheck();
		}

		RowPainter rp(pi, *text_, row, row_x, y);

		// Don't paint the row if a full repaint has not been requested
		// and if it has not changed.
		if (!pi.full_repaint && !row.changed()) {
			// Paint only the insets if the text itself is
			// unchanged.
			rp.paintOnlyInsets();
			rp.paintTooLargeMarks(
				row_x + row.left_x() < bv_->leftMargin(),
				row_x + row.right_x() > bv_->workWidth() - bv_->rightMargin());
			row.changed(false);
			y += row.descent();
			continue;
		}

		// Clear background of this row if paragraph background was not
		// already cleared because of a full repaint.
		if (!pi.full_repaint && row.changed()) {
			LYXERR(Debug::PAINTING, "Clear rect@("
			       << x << ", " << y - row.ascent() << ")="
			       << width() << " x " << row.height());
			pi.pain.fillRectangle(x, y - row.ascent(),
			                      width(), row.height(), pi.background_color);
		}

		// Instrumentation for testing row cache (see also
		// 12 lines lower):
		if (lyxerr.debugging(Debug::PAINTING)
		    && (row.selection() || pi.full_repaint || row.changed())) {
			string const foreword = text_->isMainText() ? "main text redraw "
				: "inset text redraw: ";
			LYXERR0(foreword << "pit=" << pit << " row=" << i
			        << (row.selection() ? " row_selection": "")
			        << (pi.full_repaint ? " full_repaint" : "")
			        << (row.changed() ? " row.changed" : ""));
		}

		// Backup full_repaint status and force full repaint
		// for inner insets as the Row has been cleared out.
		bool tmp = pi.full_repaint;
		pi.full_repaint = true;

		rp.paintSelection();
		rp.paintAppendix();
		rp.paintDepthBar();
		if (row.needsChangeBar())
			rp.paintChangeBar();
		if (i == 0)
			rp.paintFirst();
		if (i == nrows - 1)
			rp.paintLast();
		rp.paintText();
		rp.paintTooLargeMarks(
			row_x + row.left_x() < bv_->leftMargin(),
			row_x + row.right_x() > bv_->workWidth() - bv_->rightMargin());
		// indicate bookmarks presence in margin
		if (lyxrc.bookmarks_visibility == LyXRC::BMK_MARGIN)
			for (auto const & bp_p : bpl)
				if (bp_p.second >= row.pos() && bp_p.second < row.endpos())
					rp.paintBookmark(bp_p.first);

		y += row.descent();

#if 0
		// This debug code shows on screen which rows are repainted.
		// FIXME: since the updates related to caret blinking restrict
		// the painter to a small rectangle, the numbers are not
		// updated when this happens. Change the code in
		// GuiWorkArea::Private::show/hideCaret if this is important.
		static int count = 0;
		++count;
		FontInfo fi(sane_font);
		fi.setSize(TINY_SIZE);
		fi.setColor(Color_red);
		pi.pain.text(row_x, y, convert<docstring>(count), fi);
#endif

		// Restore full_repaint status.
		pi.full_repaint = tmp;

		row.changed(false);
	}

	//LYXERR(Debug::PAINTING, ".");
}


void TextMetrics::completionPosAndDim(Cursor const & cur, int & x, int & y,
	Dimension & dim) const
{
	DocIterator from = cur.bv().cursor();
	DocIterator to = from;
	text_->getWord(from.top(), to.top(), PREVIOUS_WORD);

	// The vertical dimension of the word
	Font const font = displayFont(cur.pit(), from.pos());
	FontMetrics const & fm = theFontMetrics(font);
	// the +1's below are related to the extra pixels added in setRowHeight
	dim.asc = fm.maxAscent() + 1;
	dim.des = fm.maxDescent() + 1;

	// get position on screen of the word start and end
	//FIXME: Is it necessary to explicitly set this to false?
	from.boundary(false);
	Point lxy = cur.bv().getPos(from);
	Point rxy = cur.bv().getPos(to);
	dim.wid = abs(rxy.x_ - lxy.x_);

	// calculate position of word
	y = lxy.y_;
	x = min(rxy.x_, lxy.x_);

	//lyxerr << "wid=" << dim.width() << " x=" << x << " y=" << y << " lxy.x_=" << lxy.x_ << " rxy.x_=" << rxy.x_ << " word=" << word << std::endl;
	//lyxerr << " wordstart=" << wordStart << " bvcur=" << bvcur << " cur=" << cur << std::endl;
}

int defaultRowHeight()
{
	return int(theFontMetrics(sane_font).maxHeight() *  1.2);
}

} // namespace lyx
