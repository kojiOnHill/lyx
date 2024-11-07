/**
 * \file InsetMathHull.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathHull.h"
#include "InsetMathChar.h"
#include "InsetMathColor.h"
#include "InsetMathFrac.h"
#include "InsetMathNest.h"
#include "InsetMathScript.h"
#include "MathExtern.h"
#include "MathFactory.h"
#include "MathStream.h"
#include "MathSupport.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "ColorSet.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "Encoding.h"
#include "Exporter.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "Language.h"
#include "LaTeXFeatures.h"
#include "LyXRC.h"
#include "MacroTable.h"
#include "InsetMathMacro.h"
#include "InsetMathMacroTemplate.h"
#include "MetricsInfo.h"
#include "Paragraph.h"
#include "ParIterator.h"
#include "xml.h"
#include "TexRow.h"
#include "TextClass.h"
#include "TextPainter.h"
#include "TocBackend.h"

#include "insets/InsetLabel.h"
#include "insets/InsetRef.h"
#include "insets/RenderPreview.h"

#include "graphics/PreviewImage.h"
#include "graphics/PreviewLoader.h"

#include "frontends/alert.h"
#include "frontends/FontMetrics.h"
#include "frontends/Painter.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/gettext.h"
#include "support/filetools.h"
#include "support/lassert.h"
#include "support/lstrings.h"
#include "support/Changer.h"

#include <sstream>

using namespace std;
using namespace lyx::support;

namespace lyx {

using cap::grabAndEraseSelection;
using cap::reduceSelectionToOneCell;

namespace {

	int getCols(HullType type)
	{
		switch (type) {
		case hullEqnArray:
			return 3;
		case hullAlign:
		case hullFlAlign:
		case hullAlignAt:
		case hullXAlignAt:
		case hullXXAlignAt:
			return 2;
		case hullUnknown:
		case hullNone:
		case hullSimple:
		case hullEquation:
		case hullMultline:
		case hullGather:
		case hullRegexp:
			return 1;
		}
		// avoid warning
		return 0;
	}


	// returns position of first relation operator in the array
	// used for "intelligent splitting"
	size_t firstRelOp(MathData const & ar)
	{
		for (MathData::const_iterator it = ar.begin(); it != ar.end(); ++it)
			if ((*it)->mathClass() == MC_REL)
				return it - ar.begin();
		return ar.size();
	}


	char const * star(bool numbered)
	{
		return numbered ? "" : "*";
	}


	// writes a preamble for underlined or struck out math display
	void writeMathdisplayPreamble(TeXMathStream & os)
	{
		if (os.strikeoutMath())
			return;

		if (os.ulemCmd() == TeXMathStream::UNDERLINE)
			os << "\\raisebox{-\\belowdisplayshortskip}{"
			      "\\parbox[b]{\\linewidth}{";
		else if (os.ulemCmd() == TeXMathStream::STRIKEOUT)
			os << "\\parbox{\\linewidth}{";
	}


	// writes a postamble for underlined or struck out math display
	void writeMathdisplayPostamble(TeXMathStream & os)
	{
		if (os.strikeoutMath())
			return;

		if (os.ulemCmd() == TeXMathStream::UNDERLINE)
			os << "}}\\\\\n";
		else if (os.ulemCmd() == TeXMathStream::STRIKEOUT)
			os << "}\\\\\n";
	}

} // namespace


static InsetLabel * dummy_pointer = nullptr;

InsetMathHull::InsetMathHull(Buffer * buf)
	: InsetMathGrid(buf, 1, 1), type_(hullNone), numbered_(1, NONUMBER),
	  numbers_(1, empty_docstring()), label_(1, dummy_pointer),
	  preview_(new RenderPreview(this))
{
	//lyxerr << "sizeof InsetMath: " << sizeof(InsetMath) << endl;
	//lyxerr << "sizeof MetricsInfo: " << sizeof(MetricsInfo) << endl;
	//lyxerr << "sizeof InsetMathChar: " << sizeof(InsetMathChar) << endl;
	//lyxerr << "sizeof FontInfo: " << sizeof(FontInfo) << endl;
	buffer_ = buf;
	initMath();
	setDefaults();
}


InsetMathHull::InsetMathHull(Buffer * buf, HullType type)
	: InsetMathGrid(buf, getCols(type), 1), type_(type), numbered_(1, NONUMBER),
	  numbers_(1, empty_docstring()), label_(1, dummy_pointer),
	  preview_(new RenderPreview(this))
{
	buffer_ = buf;
	initMath();
	setDefaults();
}


InsetMathHull::InsetMathHull(InsetMathHull const & other) : InsetMathGrid(other)
{
	operator=(other);
}


InsetMathHull::~InsetMathHull()
{
	for (auto & i : label_)
		delete i;
}


Inset * InsetMathHull::clone() const
{
	return new InsetMathHull(*this);
}


InsetMathHull & InsetMathHull::operator=(InsetMathHull const & other)
{
	if (this == &other)
		return *this;
	InsetMathGrid::operator=(other);
	type_  = other.type_;
	numbered_ = other.numbered_;
	numbers_ = other.numbers_;
	buffer_ = other.buffer_;
	for (auto & i : label_)
		delete i;
	label_ = other.label_;
	for (size_t i = 0; i != label_.size(); ++i) {
		if (label_[i])
			label_[i] = new InsetLabel(*label_[i]);
	}
	preview_.reset(new RenderPreview(*other.preview_, this));

	return *this;
}


docstring InsetMathHull::layoutName() const
{
	return "Formula:" + hullName(type_);
}


void InsetMathHull::setBuffer(Buffer & buffer)
{
	InsetMathGrid::setBuffer(buffer);

	for (size_t i = 0; i != label_.size(); ++i) {
		if (label_[i])
			label_[i]->setBuffer(buffer);
	}
}


void InsetMathHull::updateBuffer(ParIterator const & it, UpdateType utype, bool const deleted)
{
	if (!buffer_) {
		//FIXME: buffer_ should be set at creation for this inset! Problem is
		// This inset is created at too many places (see Parser::parse1() in
		// MathParser.cpp).
		return;
	}

	// if any of the equations are numbered, then we want to save the values
	// of some of the counters.
	if (haveNumbers()) {
		BufferParams const & bp = buffer_->params();
		string const & lang = it->getParLanguage(bp)->code();
		Counters & cnts =
			buffer_->masterBuffer()->params().documentClass().counters();

		// this has to be done separately
		docstring const eqstr = from_ascii("equation");
		if (cnts.hasCounter(eqstr)) {
			for (size_t i = 0; i != label_.size(); ++i) {
				docstring const oldnumber = numbers_[i];
				if (numbered(i)) {
					Paragraph const & par = it.paragraph();
					if (!par.isDeleted(it.pos())) {
						cnts.step(eqstr, utype);
						numbers_[i] = cnts.theCounter(eqstr, lang);
					} else
						numbers_[i] = from_ascii("#");
				} else
					numbers_[i] = empty_docstring();
				// If the numbering has changed, trigger a new preview
				if (oldnumber != numbers_[i] && RenderPreview::previewMath()) {
					// Do we need to remove it first?
					//preview_->removePreview(*buffer_);
					preparePreview(it);
				}
			}
		}
	}

	// now the labels
	for (size_t i = 0; i != label_.size(); ++i) {
		if (label_[i])
			label_[i]->updateBuffer(it, utype, deleted);
	}
        // set up equation numbers

	// compute first and last item
	row_type first = nrows();
	for (row_type row = 0; row != nrows(); ++row)
		if (numbered(row)) {
			first = row;
			break;
		}
	if (first != nrows()) {
		row_type last = nrows() - 1;
		for (; last != 0; --last)
			if (numbered(last))
				break;

		for (row_type row = 0; row != nrows(); ++row) {
			if (!numbered(row))
				continue;
			if (label_[row]) {
				label_[row]->setCounterValue(numbers_[row]);
				label_[row]->setPrettyCounter("(" + numbers_[row] + ")");
				label_[row]->setFormattedCounter("(" + numbers_[row] + ")");
			}
		}
	}

	// pass down
	InsetMathGrid::updateBuffer(it, utype, deleted);
}


void InsetMathHull::addToToc(DocIterator const & pit, bool output_active,
							 UpdateType utype, TocBackend & backend) const
{
	if (!buffer_) {
		//FIXME: buffer_ should be set at creation for this inset! Problem is
		// This inset is created at too many places (see Parser::parse1() in
		// MathParser.cpp).
		return;
	}

	// compute first and last item
	row_type first = nrows();
	for (row_type row = 0; row != nrows(); ++row)
		if (numbered(row)) {
			first = row;
			break;
		}
	if (first == nrows())
		// no equation
		return;
	row_type last = nrows() - 1;
	for (; last != 0; --last)
		if (numbered(last))
            break;

	TocBuilder & b = backend.builder("equation");
        b.pushItem(pit, docstring(), output_active);
	if (first != last)
		b.argumentItem(bformat(from_ascii("(%1$s-%2$s)"),
		                       numbers_[first], numbers_[last]));

	odocstringstream ods;
	Encoding const * enc = encodings.fromLyXName("utf8");
	OutputParams ops(enc);
	ops.for_toc = true;
	int const max_length = 30;

	for (row_type row = 0; row != nrows(); ++row) {
		if (!numbered(row))
			continue;
		if (label_[row]) {
			label_[row]->addToToc(pit, output_active, utype, backend);
		}
		docstring label = nicelabel(row);
		if (first == last) {
			// this is the only equation
			plaintext(ods, ops, max_length);
			b.argumentItem(label + " " + ods.str());
		} else {
			// insert as sub-items
			otexrowstream ots(ods);
			TeXMathStream wi(ots, false, false, TeXMathStream::wsDefault, enc);
			docstring d;
			for (col_type c = 0; c < ncols(); ++c) {
				wi << cell(index(row, c));
				d = ods.str();
				if (d.size() > max_length) {
					d = d.substr(0, max_length - 1);
					break;
				}
			}
			b.pushItem(pit, label+ " " + d, output_active);
			// clear the stringstream
			odocstringstream().swap(ods);
			b.pop();
		}
	}
	b.pop();
}


Inset * InsetMathHull::editXY(Cursor & cur, int x, int y)
{
	if (previewState(&cur.bv())) {
		edit(cur, true);
		return this;
	}
	return InsetMathNest::editXY(cur, x, y);
}


InsetMath::mode_type InsetMathHull::currentMode() const
{
	switch (type_) {
	case hullNone:
		return UNDECIDED_MODE;

	// definitely math mode ...
	case hullUnknown:
	case hullSimple:
	case hullEquation:
	case hullMultline:
	case hullGather:
	case hullEqnArray:
	case hullAlign:
	case hullFlAlign:
	case hullAlignAt:
	case hullXAlignAt:
	case hullXXAlignAt:
	case hullRegexp:
		return MATH_MODE;
	}
	// avoid warning
	return MATH_MODE;
}


// FIXME: InsetMathGrid should be changed to let the real column alignment be
// given by a virtual method like displayColAlign, because the values produced
// by defaultColAlign can be invalidated by lfuns such as add-column. For the
// moment the values produced by defaultColAlign are not used, notably because
// alignment is not implemented in the LyXHTML output.
char InsetMathHull::defaultColAlign(col_type col)
{
	return colAlign(type_, col);
}


char InsetMathHull::displayColAlign(idx_type idx) const
{
	switch (type_) {
	case hullMultline: {
		row_type const r = row(idx);
		if (r == 0)
			return 'l';
		if (r == nrows() - 1)
			return 'r';
		return 'c';
	}
	case hullEqnArray:
	case hullGather:
	case hullAlign:
	case hullAlignAt:
	case hullXAlignAt:
	case hullXXAlignAt:
	case hullFlAlign:
		return colAlign(type_, col(idx));
	default:
		break;
	}
	return InsetMathGrid::displayColAlign(idx);
}


int InsetMathHull::displayColSpace(col_type col) const
{
	return colSpace(type_, col);
}


// FIXME: same comment as for defaultColAlign applies.
int InsetMathHull::defaultColSpace(col_type col)
{
	return colSpace(type_, col);
}


string InsetMathHull::standardFont() const
{
	switch (type_) {
	case hullRegexp:
		return "texttt";
	case hullNone:
		return "lyxnochange";
	default:
		return "mathnormal";
	}
}


ColorCode InsetMathHull::standardColor() const
{
	switch (type_) {
	case hullRegexp:
	case hullNone:
		return Color_foreground;

	default:
		return Color_math;
	}
}


bool InsetMathHull::previewState(const BufferView *const bv) const
{
	if (!editing(bv) && RenderPreview::previewMath()
	    && type_ != hullRegexp)
	{
		graphics::PreviewImage const * pimage =
			preview_->getPreviewImage(bv->buffer());
		return pimage && pimage->image();
	}
	return false;
}


namespace {
const int ERROR_FRAME_WIDTH = 2;

bool previewTooSmall(Dimension const & dim)
{
	return dim.width() <= 10 && dim.height() <= 10;
}
}


void InsetMathHull::metrics(MetricsInfo & mi, Dimension & dim) const
{
	/* Compute \(above|below)displayskip
	   true value in LaTeX is 10pt plus 2pt minus 5pt (in normal size at 10pt)
	   But we use a fixed number of pixels and scale them with zoom.
	*/
	int const bottom_display_margin = mi.base.bv->zoomedPixels(6);
	int top_display_margin = bottom_display_margin;
	// at start of paragraph, add an empty line
	if (mi.vmode)
		top_display_margin += theFontMetrics(mi.base.font).maxHeight() + 2;

	int const ind = indent(*mi.base.bv);
	mi.extrawidth = ind;

	if (previewState(mi.base.bv)) {
		preview_->metrics(mi, dim);
		if (previewTooSmall(dim)) {
			// preview image is too small
			dim.wid += 2 * ERROR_FRAME_WIDTH;
			dim.asc += 2 * ERROR_FRAME_WIDTH;
		} else {
			// insert a gap in front of the formula
			// value was hardcoded to 1 pixel
			dim.wid += mi.base.bv->zoomedPixels(1) ;
			if (display()) {
				dim.asc += top_display_margin;
				dim.des += bottom_display_margin;
			}
		}
		return;
	}

	{
		Changer dummy1 = mi.base.changeFontSet(standardFont());
		Changer dummy2 = mi.base.font.changeStyle(display() ? DISPLAY_STYLE
												  : TEXT_STYLE);

		// let the cells adjust themselves
		InsetMathGrid::metrics(mi, dim);
	}

	// Check whether the numbering interferes with the equations
	if (numberedType()) {
		BufferParams::MathNumber const math_number = buffer().params().getMathNumber();
		int extra_offset = 0;
		int max_nlwid = 0;
		for (row_type row = 0; row < nrows(); ++row) {
			rowinfo(row).offset[mi.base.bv] += extra_offset;
			docstring const nl = nicelabel(row);
			if (nl.empty())
				continue;
			Dimension dimnl;
			mathed_string_dim(mi.base.font, nl, dimnl);
			int const x = ind ? ind : (mi.base.textwidth - dim.wid) / 2;
			// for some reason metrics does not trigger at the
			// same point as draw, and therefore we use >= instead of >
			if ((math_number == BufferParams::LEFT && dimnl.wid >= x)
			    || (math_number == BufferParams::RIGHT
			        && dimnl.wid >= mi.base.textwidth - x - dim.wid)) {
				extra_offset += dimnl.height();
			} else if (dimnl.wid > max_nlwid)
				max_nlwid = dimnl.wid;
		}
		mi.extrawidth += max_nlwid;
		dim.des += extra_offset;
	}


	if (display()) {
		dim.asc += top_display_margin;
		dim.des += bottom_display_margin;
	}

	// reserve some space for marker.
	dim.wid += 2;
}


ColorCode InsetMathHull::backgroundColor(PainterInfo const & pi) const
{
	BufferView const * const bv = pi.base.bv;
	if (previewState(bv)) {
		Dimension const dim = dimension(*pi.base.bv);
		if (previewTooSmall(dim))
			return Color_error;
		return graphics::PreviewLoader::backgroundColor();
	}
	return Color_mathbg;
}


void InsetMathHull::drawMarkers(PainterInfo & pi, int x, int y) const
{
	ColorCode pen_color = mouseHovered(pi.base.bv) || editing(pi.base.bv)?
		Color_mathframe : Color_mathcorners;
	// If the corners have the same color as the background, do not paint them.
	if (lcolor.getX11HexName(Color_mathbg) == lcolor.getX11HexName(pen_color))
		return;

	Inset::drawMarkers(pi, x, y);
	Dimension const dim = dimension(*pi.base.bv);
	int const t = x + dim.width() - 1;
	int const a = y - dim.ascent();
	pi.pain.line(x, a + 3, x, a, pen_color);
	pi.pain.line(t, a + 3, t, a, pen_color);
	pi.pain.line(x, a, x + 3, a, pen_color);
	pi.pain.line(t - 3, a, t, a, pen_color);
}


void InsetMathHull::drawBackground(PainterInfo & pi, int x, int y) const
{
	Dimension const dim = dimension(*pi.base.bv);
	if (previewTooSmall(dim)) {
		pi.pain.fillRectangle(x, y - 2 * ERROR_FRAME_WIDTH,
		    dim.wid, dim.asc + dim.des, backgroundColor(pi));
		return;
	}

	pi.pain.fillRectangle(x + 1, y - dim.asc + 1, dim.wid - 2,
	                      dim.height() - 1, pi.backgroundColor(this));
}


void InsetMathHull::draw(PainterInfo & pi, int x, int y) const
{
	BufferView const * const bv = pi.base.bv;
	Dimension const dim = dimension(*bv);

	if (type_ == hullRegexp)
		pi.pain.rectangle(x + 2, y - dim.ascent() + 1,
		                  dim.width() - 3, dim.height() - 2, Color_regexpframe);

	if (previewState(bv)) {
		// Do not draw change tracking cue if taken care of by RowPainter
		// already.
		Changer dummy = !canPaintChange(*bv) ? changeVar(pi.change, Change())
			: noChange();
		if (previewTooSmall(dim)) {
			// we have an extra frame
			preview_->draw(pi, x + ERROR_FRAME_WIDTH, y);
		} else {
			// one pixel gap in front
			// value was hardcoded to 1 pixel
			int const gap = pi.base.bv->zoomedPixels(1) ;
			preview_->draw(pi, x + gap, y);
		}
		return;
	}

	// First draw the numbers
	if (numberedType()) {
		BufferParams::MathNumber const math_number = buffer().params().getMathNumber();
		for (row_type row = 0; row < nrows(); ++row) {
			int yy = y + rowinfo(row).offset[bv];
			docstring const nl = nicelabel(row);
			Dimension dimnl;
			mathed_string_dim(pi.base.font, nl, dimnl);
			if (math_number == BufferParams::LEFT) {
				ColorCode const col = pi.selected_left
					? Color_selectiontext
					: pi.base.font.color();
				Changer dummy0 = pi.base.font.changeColor(col);
				if (dimnl.wid > x - pi.leftx)
					yy += rowinfo(row).descent + dimnl.asc;
				pi.pain.fillRectangle(pi.leftx, yy - dimnl.asc,
					dimnl.width(), dimnl.height(),
					pi.selected_left ? Color_selection : pi.background_color);
				pi.draw(pi.leftx, yy, nl);
			} else {
				ColorCode const col = pi.selected_right
					? Color_selectiontext
					: pi.base.font.color();
				Changer dummy0 = pi.base.font.changeColor(col);
				if (dimnl.wid > pi.rightx - x - dim.wid)
					yy += rowinfo(row).descent + dimnl.asc;
				pi.pain.fillRectangle(pi.rightx - dimnl.wid, yy - dimnl.asc,
					dimnl.width(), dimnl.height(),
					pi.selected_right ? Color_selection : pi.background_color);
				pi.draw(pi.rightx - dimnl.wid, yy, nl);
			}
		}
	}

	// Then the equations
	ColorCode color = pi.selected ? Color_selectionmath : standardColor();
	bool const really_change_color = pi.base.font.color() == Color_none;
	Changer dummy0 = really_change_color ? pi.base.font.changeColor(color)
		: noChange();
	Changer dummy1 = pi.base.changeFontSet(standardFont());
	Changer dummy2 = pi.base.font.changeStyle(display() ? DISPLAY_STYLE
	                                                    : TEXT_STYLE);
	InsetMathGrid::draw(pi, x + 1, y);
	drawMarkers(pi, x, y);

	// drawing change line
	if (canPaintChange(*bv)) {
		// like in metrics()
		int const display_margin = display() ? pi.base.inPixels(Length(12, Length::PT)) : 0;
		pi.change.paintCue(pi, x + 1, y + 1 - dim.asc + display_margin,
		                    x + dim.wid, y + dim.des - display_margin);
	}
}


void InsetMathHull::metricsT(TextMetricsInfo const & mi, Dimension & dim) const
{
	if (display()) {
		InsetMathGrid::metricsT(mi, dim);
	} else {
		odocstringstream os;
		otexrowstream ots(os);
		TeXMathStream wi(ots, false, true, TeXMathStream::wsDefault);
		write(wi);
		dim.wid = os.str().size();
		dim.asc = 1;
		dim.des = 0;
	}
}


void InsetMathHull::drawT(TextPainter & pain, int x, int y) const
{
	if (display()) {
		InsetMathGrid::drawT(pain, x, y);
	} else {
		odocstringstream os;
		otexrowstream ots(os);
		TeXMathStream wi(ots, false, true, TeXMathStream::wsDefault);
		write(wi);
		pain.draw(x, y, os.str().c_str());
	}
}


static docstring latexString(InsetMathHull const & inset)
{
	odocstringstream ls;
	// This has to be static, because a preview snippet or a math
	// macro containing math in text mode (such as $\text{$\phi$}$ or
	// \newcommand{\xxx}{\text{$\phi$}}) gets processed twice. The
	// first time as a whole, and the second time only the inner math.
	// In this last case inset.buffer() would be invalid.
	static Encoding const * encoding = nullptr;
	if (inset.isBufferValid())
		encoding = &(inset.buffer().params().encoding());
	otexrowstream ots(ls);
	TeXMathStream wi(ots, false, true, TeXMathStream::wsPreview, encoding);
	inset.write(wi);
	return ls.str();
}


void InsetMathHull::initUnicodeMath() const
{
	// Trigger classification of the unicode symbols in this inset
	docstring const dummy = latexString(*this);
}


void InsetMathHull::addPreview(DocIterator const & inset_pos,
	graphics::PreviewLoader & /*ploader*/) const
{
	if (RenderPreview::previewMath()) {
		preparePreview(inset_pos);
	}
}


void InsetMathHull::usedMacros(MathData const & md, DocIterator const & pos,
                               MacroNameSet & macros, MacroNameSet & defs) const
{
	MacroNameSet::iterator const end = macros.end();

	for (size_t i = 0; i < md.size(); ++i) {
		InsetMathMacro const * mi = md[i].nucleus()->asMacro();
		InsetMathMacroTemplate const * mt = md[i].nucleus()->asMacroTemplate();
		InsetMathScript const * si = md[i].nucleus()->asScriptInset();
		InsetMathFracBase const * fi = md[i].nucleus()->asFracBaseInset();
		InsetMathGrid const * gi = md[i].nucleus()->asGridInset();
		InsetMathNest const * ni = md[i].nucleus()->asNestInset();
		if (mi) {
			// Look for macros in the arguments of this macro.
			for (idx_type idx = 0; idx < mi->nargs(); ++idx)
				usedMacros(mi->cell(idx), pos, macros, defs);
			// Make sure this is a macro defined in the document
			// (as we also spot the macros in the symbols file)
			// or that we have not already accounted for it.
			docstring const name = mi->name();
			if (macros.find(name) == end)
				continue;
			macros.erase(name);
			// Look for macros in the definition of this macro.
			MathData ar(pos.buffer());
			MacroData const * data =
				pos.buffer()->getMacro(name, pos, true);
			if (data) {
				odocstringstream macro_def;
				data->write(macro_def, true);
				// Empty lines will generate full rows
				// with legacy preview and tightpage (#13120).
				// So remove all preceding and trailing line breaks
				// and re-add one at the end of the snippet
				defs.insert(trim(macro_def.str(), "\n") + "\n");
				asArray(data->definition(), ar);
			}
			usedMacros(ar, pos, macros, defs);
		} else if (mt) {
			MathData ar(pos.buffer());
			asArray(mt->definition(), ar);
			usedMacros(ar, pos, macros, defs);
		} else if (si) {
			if (!si->nuc().empty())
				usedMacros(si->nuc(), pos, macros, defs);
			if (si->hasDown())
				usedMacros(si->down(), pos, macros, defs);
			if (si->hasUp())
				usedMacros(si->up(), pos, macros, defs);
		} else if (fi || gi) {
			idx_type nidx = fi ? fi->nargs() : gi->nargs();
			for (idx_type idx = 0; idx < nidx; ++idx)
				usedMacros(fi ? fi->cell(idx) : gi->cell(idx),
				           pos, macros, defs);
		} else if (ni) {
			usedMacros(ni->cell(0), pos, macros, defs);
		}
	}
}


void InsetMathHull::preparePreview(DocIterator const & pos,
                                   bool forexport) const
{
	// there is no need to do all the macro stuff if we're not
	// actually going to generate the preview.
	if (!RenderPreview::previewMath() && !forexport)
		return;

	Buffer const * buffer = pos.buffer();

	// collect macros at this position
	MacroNameSet macros;
	buffer->listMacroNames(macros);

	// collect definitions only for the macros used in this inset
	MacroNameSet defs;
	for (idx_type idx = 0; idx < nargs(); ++idx)
		usedMacros(cell(idx), pos, macros, defs);

	docstring macro_preamble;
	for (auto const & defvar : defs)
		macro_preamble.append(defvar);

	// set the font series and size for this snippet
	DocIterator dit = pos.getInnerText();
	Paragraph const & par = dit.paragraph();
	Font font = par.getFontSettings(buffer->params(), dit.pos());
	font.fontInfo().realize(par.layout().font);
	string const lsize = font.latexSize();
	docstring setfont;
	docstring endfont;
	if (font.fontInfo().series() == BOLD_SERIES) {
		setfont += from_ascii("\\textbf{");
		endfont += '}';
	}
	if (lsize != "normalsize" && !prefixIs(lsize, "error"))
		setfont += from_ascii("\\" + lsize + '\n');

	docstring const snippet = macro_preamble + setfont + latexString(*this) + endfont;
	LYXERR(Debug::MACROS, "Preview snippet: " << snippet);
	preview_->addPreview(snippet, *buffer, forexport);
}


void InsetMathHull::reloadPreview(DocIterator const & pos) const
{
	preparePreview(pos);
	preview_->startLoading(*pos.buffer());
}


void InsetMathHull::loadPreview(DocIterator const & pos) const
{
	bool const forexport = true;
	preparePreview(pos, forexport);
	preview_->startLoading(*pos.buffer(), forexport);
}


bool InsetMathHull::notifyCursorLeaves(Cursor const & old, Cursor & cur)
{
	if (RenderPreview::previewMath()) {
		reloadPreview(old);
		cur.screenUpdateFlags(Update::Force);
	}
	return false;
}


docstring InsetMathHull::label(row_type row) const
{
	LASSERT(row < nrows(), return docstring());
	if (InsetLabel * il = label_[row])
		return il->screenLabel();
	return docstring();
}


void InsetMathHull::label(row_type row, docstring const & label)
{
	//lyxerr << "setting label '" << label << "' for row " << row << endl;
	if (label_[row]) {
		if (label.empty()) {
			delete label_[row];
			label_[row] = dummy_pointer;
		} else {
			if (buffer_)
				label_[row]->updateLabelAndRefs(label);
			else
				label_[row]->setParam("name", label);
		}
		return;
	}
	InsetCommandParams p(LABEL_CODE);
	p["name"] = label;
	label_[row] = new InsetLabel(buffer_, p);
	if (buffer_)
		label_[row]->setBuffer(buffer());
}


void InsetMathHull::numbered(row_type row, Numbered num)
{
	numbered_[row] = num;
}


bool InsetMathHull::numbered(row_type row) const
{
	return numbered_[row] == NUMBER;
}


bool InsetMathHull::ams() const
{
	switch (type_) {
	case hullAlign:
	case hullFlAlign:
	case hullMultline:
	case hullGather:
	case hullAlignAt:
	case hullXAlignAt:
	case hullXXAlignAt:
		return true;
	case hullUnknown:
	case hullRegexp:
		return false;
	case hullNone:
	case hullSimple:
	case hullEquation:
	case hullEqnArray:
		break;
	}
	for (auto const & row : numbered_)
		if (row == NOTAG)
			return true;
	return false;
}


bool InsetMathHull::outerDisplay() const
{
	switch (type_) {
	case hullEquation:
	case hullEqnArray:
	case hullAlign:
	case hullFlAlign:
	case hullGather:
	case hullMultline:
		return true;
	case hullNone:
	case hullSimple:
	case hullAlignAt:
	case hullXAlignAt:
	case hullXXAlignAt:
	case hullUnknown:
	case hullRegexp:
		break;
	}
	return false;
}


int InsetMathHull::rowFlags() const
{
	switch (type_) {
	case hullUnknown:
	case hullSimple:
	case hullNone:
	case hullRegexp:
		return Inline;
	case hullEqnArray:
	case hullAlign:
	case hullFlAlign:
	case hullAlignAt:
	case hullXAlignAt:
	case hullXXAlignAt:
	case hullEquation:
	case hullMultline:
	case hullGather:
		if (buffer().params().is_math_indent)
			return Display | AlignLeft;
		else
			return Display;
	}
	// avoid warning
	return Display;
}


int InsetMathHull::indent(BufferView const & bv) const
{
	// FIXME: set this in the textclass. This value is what the article class uses.
	static Length default_indent(2.5, Length::EM);
	if (display() && buffer().params().is_math_indent) {
		Length const & len = buffer().params().getMathIndent();
		if (len.empty())
			return bv.inPixels(default_indent);
		else
			return bv.inPixels(len);
	} else
		return Inset::indent(bv);
}


bool InsetMathHull::numberedType() const
{
	switch (type_) {
	case hullUnknown:
	case hullNone:
	case hullSimple:
	case hullXXAlignAt:
	case hullRegexp:
		return false;
	case hullEqnArray:
	case hullAlign:
	case hullFlAlign:
	case hullAlignAt:
	case hullXAlignAt:
	case hullEquation:
	case hullMultline:
	case hullGather:
		break;
	}
	for (row_type row = 0; row < nrows(); ++row)
		if (numbered(row))
			return true;
	return false;
}


void InsetMathHull::validate(LaTeXFeatures & features) const
{
	if (features.runparams().isLaTeX()) {
		if (ams())
			features.require("amsmath");

		if (type_ == hullRegexp) {
			features.require("color");
			docstring frcol = from_utf8(lcolor.getLaTeXName(Color_regexpframe));
			docstring bgcol = from_ascii("white");
			features.addPreambleSnippet(
				"\\newcommand{\\regexp}[1]{\\fcolorbox{"
				+ frcol + "}{"
				+ bgcol + "}{\\ensuremath{\\mathtt{#1}}}}");
			features.addPreambleSnippet(
				from_ascii("\\newcommand{\\endregexp}{}"));
		} else if (outerDisplay() && features.inDeletedInset()) {
				features.require("tikz");
				features.require("ct-tikz-object-sout");
		}

		// Validation is necessary only if not using AMS math.
		// To be safe, we will always run mathedvalidate.
		//if (features.amsstyle)
		//  return;

		//features.binom      = true;
	} else if (features.runparams().math_flavor == OutputParams::MathAsHTML) {
		// it would be better to do this elsewhere, but we can't validate in
		// InsetMathMatrix and we have no way, outside MathExtern, to know if
		// we even have any matrices.
				features.addCSSSnippet(
					"table.matrix{display: inline-block; vertical-align: middle; text-align:center;}\n"
					"table.matrix td{padding: 0.25px;}\n"
					"td.ldelim{width: 0.5ex; border: thin solid black; border-right: none;}\n"
					"td.rdelim{width: 0.5ex; border: thin solid black; border-left: none;}");
	}
	InsetMathGrid::validate(features);
}


CtObject InsetMathHull::getCtObject(OutputParams const & runparams) const
{
	CtObject res = CtObject::Normal;
	switch(type_) {
	case hullNone:
	case hullSimple:
	case hullAlignAt:
	case hullXAlignAt:
	case hullXXAlignAt:
	case hullRegexp:
	case hullUnknown:
		break;

	case hullEquation:
	case hullEqnArray:
	case hullAlign:
	case hullFlAlign:
	case hullGather:
	case hullMultline: {
		if (runparams.inulemcmd
		    && (!runparams.local_font || runparams.local_font->fontInfo().strikeout() != FONT_ON))
			res = CtObject::UDisplayObject;
		else
			res = CtObject::DisplayObject;
		break;
		}
	}
	return res;
}


void InsetMathHull::header_write(TeXMathStream & os) const
{
	bool n = numberedType();

	switch(type_) {
	case hullNone:
		break;

	case hullSimple:
		if (os.ulemCmd())
			os << "\\mbox{";
		os << '$';
		os.startOuterRow();
		if (cell(0).empty())
			os << ' ';
		break;

	case hullEquation:
		writeMathdisplayPreamble(os);
		os << "\n";
		os.startOuterRow();
		if (n)
			os << "\\begin{equation" << star(n) << "}\n";
		else
			os << "\\[\n";
		break;

	case hullEqnArray:
	case hullAlign:
	case hullFlAlign:
	case hullGather:
	case hullMultline:
		writeMathdisplayPreamble(os);
		os << "\n";
		os.startOuterRow();
		os << "\\begin{" << hullName(type_) << star(n) << "}\n";
		break;

	case hullAlignAt:
	case hullXAlignAt:
		os << "\n";
		os.startOuterRow();
		os << "\\begin{" << hullName(type_) << star(n) << '}'
		  << '{' << static_cast<unsigned int>((ncols() + 1)/2) << "}\n";
		break;

	case hullXXAlignAt:
		os << "\n";
		os.startOuterRow();
		os << "\\begin{" << hullName(type_) << '}'
		  << '{' << static_cast<unsigned int>((ncols() + 1)/2) << "}\n";
		break;

	case hullRegexp:
		os << "\\regexp{";
		break;

	case hullUnknown:
		os << "\n";
		os.startOuterRow();
		os << "\\begin{unknown" << star(n) << "}\n";
		break;
	}
}


void InsetMathHull::footer_write(TeXMathStream & os) const
{
	bool n = numberedType();

	switch(type_) {
	case hullNone:
		os << "\n";
		break;

	case hullSimple:
		os << '$';
		if (os.ulemCmd())
			os << "}";
		break;

	case hullEquation:
		os << "\n";
		os.startOuterRow();
		if (n)
			os << "\\end{equation" << star(n) << "}\n";
		else
			os << "\\]\n";
		writeMathdisplayPostamble(os);
		break;

	case hullEqnArray:
	case hullAlign:
	case hullFlAlign:
	case hullGather:
	case hullMultline:
		os << "\n";
		os.startOuterRow();
		os << "\\end{" << hullName(type_) << star(n) << "}\n";
		writeMathdisplayPostamble(os);
		break;

	case hullAlignAt:
	case hullXAlignAt:
		os << "\n";
		os.startOuterRow();
		os << "\\end{" << hullName(type_) << star(n) << "}\n";
		break;

	case hullXXAlignAt:
		os << "\n";
		os.startOuterRow();
		os << "\\end{" << hullName(type_) << "}\n";
		break;

	case hullRegexp:
		// Only used as a heuristic to find the regexp termination, when searching in ignore-format mode
		os << "\\endregexp{}}";
		break;

	case hullUnknown:
		os << "\n";
		os.startOuterRow();
		os << "\\end{unknown" << star(n) << "}\n";
		break;
	}
}


bool InsetMathHull::allowsTabularFeatures() const
{
	switch (type_) {
	case hullEqnArray:
	case hullAlign:
	case hullAlignAt:
	case hullXAlignAt:
	case hullXXAlignAt:
	case hullFlAlign:
	case hullMultline:
	case hullGather:
		return true;
	case hullNone:
	case hullSimple:
	case hullEquation:
	case hullRegexp:
	case hullUnknown:
		break;
	}
	return false;
}


bool InsetMathHull::rowChangeOK() const
{
	return
		type_ == hullEqnArray || type_ == hullAlign ||
		type_ == hullFlAlign || type_ == hullAlignAt ||
		type_ == hullXAlignAt || type_ == hullXXAlignAt ||
		type_ == hullGather || type_ == hullMultline;
}


bool InsetMathHull::colChangeOK() const
{
	return
		type_ == hullAlign || type_ == hullFlAlign ||type_ == hullAlignAt ||
		type_ == hullXAlignAt || type_ == hullXXAlignAt;
}


void InsetMathHull::addRow(row_type row)
{
	if (!rowChangeOK())
		return;

	bool numbered = numberedType();
	// Move the number and raw pointer, do not call label() (bug 7511)
	InsetLabel * label = dummy_pointer;
	docstring number = empty_docstring();
	if (type_ == hullMultline) {
		if (row + 1 == nrows())  {
			numbered_[row] = NONUMBER;
			swap(label, label_[row]);
			swap(number, numbers_[row]);
		} else
			numbered = false;
	}

	numbered_.insert(numbered_.begin() + row + 1, numbered ? NUMBER : NONUMBER);
	numbers_.insert(numbers_.begin() + row + 1, number);
	label_.insert(label_.begin() + row + 1, label);
	InsetMathGrid::addRow(row);
}


void InsetMathHull::swapRow(row_type row)
{
	if (nrows() <= 1)
		return;
	if (row + 1 == nrows())
		--row;
	swap(numbered_[row], numbered_[row + 1]);
	swap(numbers_[row], numbers_[row + 1]);
	swap(label_[row], label_[row + 1]);
	InsetMathGrid::swapRow(row);
}


void InsetMathHull::delRow(row_type row)
{
	if (nrows() <= 1 || !rowChangeOK())
		return;
	if (row + 1 == nrows() && type_ == hullMultline) {
		swap(numbered_[row - 1], numbered_[row]);
		swap(numbers_[row - 1], numbers_[row]);
		swap(label_[row - 1], label_[row]);
		InsetMathGrid::delRow(row);
		return;
	}
	InsetMathGrid::delRow(row);
	// The last dummy row has no number info nor a label.
	// Test nrows() + 1 because we have already erased the row.
	if (row == nrows() + 1)
		row--;
	numbered_.erase(numbered_.begin() + row);
	numbers_.erase(numbers_.begin() + row);
	delete label_[row];
	label_.erase(label_.begin() + row);
}


void InsetMathHull::addCol(col_type col)
{
	if (!colChangeOK())
		return;
	InsetMathGrid::addCol(col);
}


void InsetMathHull::delCol(col_type col)
{
	if (ncols() <= 1 || !colChangeOK())
		return;
	InsetMathGrid::delCol(col);
}


docstring InsetMathHull::nicelabel(row_type row) const
{
	if (!numbered(row)) {
		if (!label_[row])
			return docstring();
		return '[' + label_[row]->screenLabel() + ']';
	}
	docstring const & val = numbers_[row];
	if (!label_[row])
		return '(' + val + ')';
	return '(' + val + ',' + label_[row]->screenLabel() + ')';
}


void InsetMathHull::glueall(HullType type)
{
	MathData ar(buffer_);
	for (idx_type i = 0; i < nargs(); ++i)
		ar.append(cell(i));
	InsetLabel * label = nullptr;
	if (type == hullEquation) {
		// preserve first non-empty label
		for (row_type row = 0; row < nrows(); ++row) {
			if (label_[row]) {
				label = label_[row];
				label_[row] = 0;
				break;
			}
		}
	}
	*this = InsetMathHull(buffer_, hullSimple);
	label_[0] = label;
	cell(0) = ar;
	setDefaults();
}


void InsetMathHull::splitTo2Cols()
{
	LASSERT(ncols() == 1, return);
	InsetMathGrid::addCol(1);
	for (row_type row = 0; row < nrows(); ++row) {
		idx_type const i = 2 * row;
		pos_type pos = firstRelOp(cell(i));
		cell(i + 1) = MathData(buffer_, cell(i).begin() + pos, cell(i).end());
		cell(i).erase(pos, cell(i).size());
	}
}


void InsetMathHull::splitTo3Cols()
{
	LASSERT(ncols() < 3, return);
	if (ncols() < 2)
		splitTo2Cols();
	InsetMathGrid::addCol(2);
	for (row_type row = 0; row < nrows(); ++row) {
		idx_type const i = 3 * row + 1;
		if (!cell(i).empty()) {
			cell(i + 1) = MathData(buffer_, cell(i).begin() + 1, cell(i).end());
			cell(i).erase(1, cell(i).size());
		}
	}
}


void InsetMathHull::changeCols(col_type cols)
{
	if (ncols() == cols)
		return;
	else if (ncols() < cols) {
		// split columns
		if (cols < 3)
			splitTo2Cols();
		else {
			splitTo3Cols();
			while (ncols() < cols)
				InsetMathGrid::addCol(ncols());
		}
		return;
	}

	// combine columns
	for (row_type row = 0; row < nrows(); ++row) {
		idx_type const i = row * ncols();
		for (col_type col = cols; col < ncols(); ++col) {
			cell(i + cols - 1).append(cell(i + col));
		}
	}
	// delete columns
	while (ncols() > cols) {
		InsetMathGrid::delCol(ncols() - 1);
	}
}


HullType InsetMathHull::getType() const
{
	return type_;
}


void InsetMathHull::setType(HullType type)
{
	type_ = type;
	setDefaults();
}


bool InsetMathHull::isMutable(HullType type)
{
	switch (type) {
	case hullNone:
	case hullSimple:
	case hullEquation:
	case hullEqnArray:
	case hullAlign:
	case hullFlAlign:
	case hullAlignAt:
	case hullXAlignAt:
	case hullXXAlignAt:
	case hullMultline:
	case hullGather:
		return true;
	case hullUnknown:
	case hullRegexp:
		return false;
	}
	// avoid warning
	return false;
}


void InsetMathHull::mutate(HullType newtype)
{
	//lyxerr << "mutating from '" << type_ << "' to '" << newtype << "'" << endl;

  	if (newtype == type_)
		return;

	// This guards the algorithm below it, which is designed with certain types
	// in mind.
	if (!isMutable(newtype) || !isMutable(type_)) {
		lyxerr << "mutation from '" << to_utf8(hullName(type_))
		       << "' to '" << to_utf8(hullName(newtype))
		       << "' not implemented" << endl;
		return;
	}

	// we try to move along the chain
	// none <-> simple <-> equation <-> eqnarray -> *align* -> multline, gather -+
	//                                     ^                                     |
	//                                     +-------------------------------------+
	// we use eqnarray as intermediate type for mutations that are not
	// directly supported because it handles labels and numbering for
	// "down mutation".

	switch (type_) {
	case hullNone:
		setType(hullSimple);
		numbered(0, false);
		mutate(newtype);
		break;

	case hullSimple:
		if (newtype == hullNone) {
			setType(hullNone);
			numbered(0, false);
		} else {
			setType(hullEquation);
			numbered(0, label_[0] != nullptr);
			mutate(newtype);
		}
		break;

	case hullEquation:
		switch (newtype) {
		case hullNone:
		case hullSimple:
			setType(hullSimple);
			numbered(0, false);
			mutate(newtype);
			break;
		case hullEqnArray:
			// split it "nicely" on the first relop
			splitTo3Cols();
			setType(hullEqnArray);
			break;
		case hullMultline:
		case hullGather:
			setType(newtype);
			break;
		default:
			// *align*
			// split it "nicely"
			splitTo2Cols();
			setType(hullAlign);
			mutate(newtype);
			break;
		}
		break;

	case hullEqnArray:
		switch (newtype) {
		case hullNone:
		case hullSimple:
		case hullEquation:
			glueall(newtype);
			mutate(newtype);
			break;
		default:
			// align & Co.
			changeCols(2);
			setType(hullAlign);
			mutate(newtype);
			break;
		}
		break;

	case hullAlign:
	case hullAlignAt:
	case hullXAlignAt:
	case hullFlAlign:
		switch (newtype) {
		case hullNone:
		case hullSimple:
		case hullEquation:
		case hullEqnArray:
			changeCols(3);
			setType(hullEqnArray);
			mutate(newtype);
			break;
		case hullGather:
		case hullMultline:
			changeCols(1);
			setType(newtype);
			break;
		case hullXXAlignAt:
			for (row_type row = 0; row < nrows(); ++row)
				numbered(row, false);
			setType(newtype);
			break;
		default:
			setType(newtype);
			break;
		}
		break;

	case hullXXAlignAt:
		for (row_type row = 0; row < nrows(); ++row)
			numbered(row, false);
		switch (newtype) {
		case hullNone:
		case hullSimple:
		case hullEquation:
		case hullEqnArray:
			changeCols(3);
			setType(hullEqnArray);
			mutate(newtype);
			break;
		case hullGather:
		case hullMultline:
			changeCols(1);
			setType(newtype);
			break;
		default:
			setType(newtype);
			break;
		}
		break;

	case hullMultline:
	case hullGather:
		switch (newtype) {
		case hullGather:
		case hullMultline:
			setType(newtype);
			break;
		case hullAlign:
		case hullFlAlign:
		case hullAlignAt:
		case hullXAlignAt:
			splitTo2Cols();
			setType(newtype);
			break;
		case hullXXAlignAt:
			splitTo2Cols();
			for (row_type row = 0; row < nrows(); ++row)
				numbered(row, false);
			setType(newtype);
			break;
		default:
			// first we mutate to EqnArray
			splitTo3Cols();
			setType(hullEqnArray);
			mutate(newtype);
			break;
		}
		break;

	default:
		// we passed the guard so we should not be here
		LYXERR0("Mutation not implemented, but should have been.");
		LASSERT(false, return);
		break;
	}// switch
}


void InsetMathHull::eol(TeXMathStream & os, row_type row, bool fragile, bool latex,
                        bool last_eoln) const
{
	if (numberedType()) {
		bool const for_preview =
			(os.output() == TeXMathStream::wsPreview);
		if (label_[row]) {
			// Use utf8 strings for previewed labels when possible
			bool use_utf8 = for_preview &&
				(buffer().params().useNonTeXFonts ||
				 buffer().params().encoding().package() == Encoding::japanese);
			docstring const name = (latex && !use_utf8)
				? escape(label_[row]->getParam("name"))
				: label_[row]->getParam("name");
			os << "\\label{" + name + '}';
		}
		if (type_ != hullMultline) {
			if (numbered_[row]  == NONUMBER)
				os << "\\nonumber ";
			else if (numbered_[row]  == NOTAG)
				os<< "\\notag ";
		}
		if (for_preview && !numbers_[row].empty()) {
			os << "\\global\\def\\theequation{" << numbers_[row] << "}\n";
		}

	}
	// Never add \\ on the last empty line of eqnarray and friends
	last_eoln = false;
	InsetMathGrid::eol(os, row, fragile, latex, last_eoln);
}

void InsetMathHull::write(TeXMathStream & os) const
{
	ModeSpecifier specifier(os, MATH_MODE);
	header_write(os);
	InsetMathGrid::write(os);
	footer_write(os);
}


void InsetMathHull::normalize(NormalStream & os) const
{
	os << "[formula " << hullName(type_) << ' ';
	InsetMathGrid::normalize(os);
	os << "] ";
}


void InsetMathHull::infoize(odocstream & os) const
{
	os << bformat(_("Type: %1$s"), hullName(type_));
}


void InsetMathHull::check() const
{
	LATTEST(numbered_.size() == nrows());
	LATTEST(numbers_.size() == nrows());
	LATTEST(label_.size() == nrows());
}


void InsetMathHull::doExtern(Cursor & cur, FuncRequest & func)
{
	//FIXME: sort out whether we want std::string or docstring for those
	string const lang = func.getArg(0);
	docstring extra = from_utf8(func.getArg(1));
	if (extra.empty())
		extra = from_ascii("noextra");

	// replace selection with result of computation
	if (reduceSelectionToOneCell(cur)) {
		MathData ar(buffer_);
		asArray(grabAndEraseSelection(cur), ar);
		lyxerr << "use selection: " << ar << endl;
		cur.insert(pipeThroughExtern(lang, extra, ar));
		return;
	}

	// only inline, display or eqnarray math is allowed
	switch (getType()) {
	case hullSimple:
	case hullEquation:
	case hullEqnArray:
		break;
	default:
		frontend::Alert::warning(_("Bad math environment"),
				_("Computation cannot be performed for AMS "
				  "math environments.\nChange the math "
				  "formula type and try again."));
		return;
	}

	MathData eq(buffer_);
	eq.push_back(MathAtom(new InsetMathChar(buffer_, '=')));

	// go to first item in line
	cur.idx() -= cur.idx() % ncols();
	cur.pos() = 0;

	if (getType() == hullSimple) {
		size_type pos = cur.cell().find_last(eq);
		MathData ar(buffer_);
		if (pos == cur.cell().size()) {
			ar = cur.cell();
			lyxerr << "use whole cell: " << ar << endl;
		} else {
			ar = MathData(buffer_, cur.cell().begin() + pos + 1, cur.cell().end());
			lyxerr << "use partial cell form pos: " << pos << endl;
		}
		cur.cell().append(eq);
		cur.cell().append(pipeThroughExtern(lang, extra, ar));
		cur.pos() = cur.lastpos();
		return;
	}

	if (getType() == hullEquation) {
		lyxerr << "use equation inset" << endl;
		mutate(hullEqnArray);
		MathData & ar = cur.cell();
		lyxerr << "use cell: " << ar << endl;
		++cur.idx();
		cur.cell() = eq;
		++cur.idx();
		cur.cell() = pipeThroughExtern(lang, extra, ar);
		// move to end of line
		cur.pos() = cur.lastpos();
		return;
	}

	{
		lyxerr << "use eqnarray" << endl;
		cur.idx() += 2 - cur.idx() % ncols();
		cur.pos() = 0;
		MathData ar = cur.cell();
		lyxerr << "use cell: " << ar << endl;
		// FIXME: temporarily disabled
		addRow(cur.row());
		++cur.idx();
		++cur.idx();
		cur.cell() = eq;
		++cur.idx();
		cur.cell() = pipeThroughExtern(lang, extra, ar);
		cur.pos() = cur.lastpos();
	}
}


void InsetMathHull::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	//lyxerr << "action: " << cmd.action() << endl;
	switch (cmd.action()) {

	case LFUN_FINISHED_BACKWARD:
	case LFUN_FINISHED_FORWARD:
	case LFUN_FINISHED_RIGHT:
	case LFUN_FINISHED_LEFT:
		//lyxerr << "action: " << cmd.action() << endl;
		InsetMathGrid::doDispatch(cur, cmd);
		break;

	case LFUN_PARAGRAPH_BREAK:
		// just swallow this
		break;

	case LFUN_NEWLINE_INSERT:
		// some magic for the common case
		if (type_ == hullSimple || type_ == hullEquation) {
			cur.recordUndoInset();
			bool const align =
				cur.bv().buffer().params().use_package("amsmath") != BufferParams::package_off;
			mutate(align ? hullAlign : hullEqnArray);
			// mutate() may change labels and such.
			cur.forceBufferUpdate();
			cur.idx() = nrows() * ncols() - 1;
			cur.pos() = cur.lastpos();
		}
		InsetMathGrid::doDispatch(cur, cmd);
		break;

	case LFUN_MATH_NUMBER_TOGGLE: {
		//lyxerr << "toggling all numbers" << endl;
		cur.recordUndoInset();
		bool old = numberedType();
		if (type_ == hullMultline) {
			row_type row = nrows() - 1;
			numbered(row, !old);
			if (old && label_[row]) {
				delete label_[row];
				label_[row] = 0;
			}
		} else {
			for (row_type row = 0; row < nrows(); ++row) {
				numbered(row, !old);
				if (old && label_[row]) {
					delete label_[row];
					label_[row] = 0;
				}
			}
		}

		cur.message(old ? _("No number") : _("Number"));
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_MATH_NUMBER_LINE_TOGGLE: {
		cur.recordUndoInset();
		row_type r = (type_ == hullMultline) ? nrows() - 1 : cur.row();
		bool old = numbered(r);
		cur.message(old ? _("No number") : _("Number"));
		numbered(r, !old);
		if (old && label_[r]) {
			delete label_[r];
			label_[r] = 0;
		}
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_LABEL_INSERT: {
		row_type r = (type_ == hullMultline) ? nrows() - 1 : cur.row();
		docstring old_label = label(r);
		docstring const default_label = from_ascii("eq:");
		if (old_label.empty())
			old_label = default_label;

		InsetCommandParams p(LABEL_CODE);
		p["name"] = cmd.argument().empty() ? old_label : cmd.argument();
		string const data = InsetCommand::params2string(p);

		if (cmd.argument().empty())
			cur.bv().showDialog("label", data);
		else {
			FuncRequest fr(LFUN_INSET_INSERT, data);
			dispatch(cur, fr);
		}
		break;
	}

	case LFUN_LABEL_COPY_AS_REFERENCE: {
		row_type row;
		if (cmd.argument().empty() && &cur.inset() == this)
			// if there is no argument and we're inside math, we retrieve
			// the row number from the cursor position.
			row = (type_ == hullMultline) ? nrows() - 1 : cur.row();
		else {
			// if there is an argument, find the corresponding label, else
			// check whether there is at least one label.
			for (row = 0; row != nrows(); ++row)
				if (label_[row]
					  && (cmd.argument().empty() || label(row) == cmd.argument()))
					break;
		}

		if (row == nrows())
			break;

		InsetCommandParams p(REF_CODE, "ref");
		p["reference"] = label(row);
		cap::clearSelection();
		cap::copyInset(cur, new InsetRef(buffer_, p), label(row));
		break;
	}

	case LFUN_WORD_DELETE_FORWARD:
	case LFUN_CHAR_DELETE_FORWARD:
		if (col(cur.idx()) + 1 == ncols()
		    && cur.pos() == cur.lastpos()
		    && !cur.selection()) {
			if (!label(row(cur.idx())).empty()) {
				cur.recordUndoInset();
				label(row(cur.idx()), docstring());
			} else if (numbered(row(cur.idx()))) {
				cur.recordUndoInset();
				numbered(row(cur.idx()), false);
				cur.forceBufferUpdate();
			} else {
				InsetMathGrid::doDispatch(cur, cmd);
				return;
			}
		} else {
			InsetMathGrid::doDispatch(cur, cmd);
			return;
		}
		break;

	case LFUN_INSET_INSERT: {
		//lyxerr << "arg: " << to_utf8(cmd.argument()) << endl;
		// FIXME: this should be cleaned up to use InsetLabel methods directly.
		string const name = cmd.getArg(0);
		if (name == "label") {
			InsetCommandParams p(LABEL_CODE);
			InsetCommand::string2params(to_utf8(cmd.argument()), p);
			docstring str = p["name"];
			cur.recordUndoInset();
			row_type const r = (type_ == hullMultline) ? nrows() - 1 : cur.row();
			str = trim(str);
			if (!str.empty())
				numbered(r, true);
			docstring old = label(r);
			if (str != old) {
				if (label_[r])
					// The label will take care of the reference update.
					label(r, str);
				else {
					label(r, str);
					// Newly created inset so initialize it.
					label_[r]->initView();
				}
			}
			cur.forceBufferUpdate();
			break;
		}
		InsetMathGrid::doDispatch(cur, cmd);
		return;
	}

	case LFUN_MATH_EXTERN:
		cur.recordUndoInset();
		doExtern(cur, cmd);
		break;

	case LFUN_MATH_MUTATE: {
		cur.recordUndoInset();
		row_type row = cur.row();
		col_type col = cur.col();
		mutate(hullType(cmd.argument()));
		cur.idx() = row * ncols() + col;
		if (cur.idx() > cur.lastidx()) {
			cur.idx() = cur.lastidx();
			cur.pos() = cur.lastpos();
		}
		if (cur.pos() > cur.lastpos())
			cur.pos() = cur.lastpos();

		cur.forceBufferUpdate();
		// FIXME: find some more clever handling of the selection,
		// i.e. preserve it.
		cur.clearSelection();
		//cur.dispatched(FINISHED);
		break;
	}

	case LFUN_MATH_DISPLAY: {
		cur.recordUndoInset();
		mutate(type_ == hullSimple ? hullEquation : hullSimple);
		// if the cursor is in a cell that got merged, move it to
		// start of the hull inset.
		if (cur.idx() > 0) {
			cur.idx() = 0;
			cur.pos() = 0;
		}
		if (cur.pos() > cur.lastpos())
			cur.pos() = cur.lastpos();

		break;
	}

	case LFUN_TABULAR_FEATURE:
		if (!allowsTabularFeatures())
			cur.undispatched();
		else
			InsetMathGrid::doDispatch(cur, cmd);
		break;

	default:
		InsetMathGrid::doDispatch(cur, cmd);
		break;
	}
}


namespace {

bool allowDisplayMath(Cursor const & cur)
{
	LATTEST(cur.depth() > 1);
	Cursor tmpcur = cur;
	tmpcur.pop();
	FuncStatus status;
	FuncRequest cmd(LFUN_MATH_DISPLAY);
	return tmpcur.getStatus(cmd, status) && status.enabled();
}

} // namespace


bool InsetMathHull::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & status) const
{
	switch (cmd.action()) {
	case LFUN_FINISHED_BACKWARD:
	case LFUN_FINISHED_FORWARD:
	case LFUN_FINISHED_RIGHT:
	case LFUN_FINISHED_LEFT:
	case LFUN_UP:
	case LFUN_DOWN:
	case LFUN_NEWLINE_INSERT:
	case LFUN_MATH_EXTERN:
		// we handle these
		status.setEnabled(true);
		return true;

	// we never allow this in math, and we want to bind enter
	// to another actions in command-alternatives
	case LFUN_PARAGRAPH_BREAK:
		status.setEnabled(false);
		return true;
	case LFUN_MATH_MUTATE: {
		HullType const ht = hullType(cmd.argument());
		status.setOnOff(type_ == ht);
		status.setEnabled(isMutable(ht) && isMutable(type_));

		if (ht != hullSimple && status.enabled())
			status.setEnabled(allowDisplayMath(cur));
		return true;
	}
	case LFUN_MATH_DISPLAY: {
		status.setEnabled(display() || allowDisplayMath(cur));
		status.setOnOff(display());
		return true;
	}

	case LFUN_MATH_NUMBER_TOGGLE:
		// FIXME: what is the right test, this or the one of
		// LABEL_INSERT?
		status.setEnabled(display());
		status.setOnOff(numberedType());
		return true;

	case LFUN_MATH_NUMBER_LINE_TOGGLE: {
		// FIXME: what is the right test, this or the one of
		// LABEL_INSERT?
		bool const enable = (type_ == hullMultline)
			? (nrows() - 1 == cur.row())
			: display();
		row_type const r = (type_ == hullMultline) ? nrows() - 1 : cur.row();
		status.setEnabled(enable);
		status.setOnOff(enable && numbered(r));
		return true;
	}

	case LFUN_LABEL_INSERT:
		status.setEnabled(type_ != hullSimple);
		return true;

	case LFUN_LABEL_COPY_AS_REFERENCE: {
		bool enabled = false;
		if (cmd.argument().empty() && &cur.inset() == this) {
			// if there is no argument and we're inside math, we retrieve
			// the row number from the cursor position.
			row_type row = (type_ == hullMultline) ? nrows() - 1 : cur.row();
			enabled = numberedType() && label_[row];
		} else {
			// if there is an argument, find the corresponding label, else
			// check whether there is at least one label.
			for (row_type row = 0; row != nrows(); ++row) {
				if (label_[row] &&
					(cmd.argument().empty() || label(row) == cmd.argument())) {
						enabled = true;
						break;
				}
			}
		}
		status.setEnabled(enabled);
		return true;
	}

	case LFUN_INSET_INSERT:
		if (cmd.getArg(0) == "label") {
			status.setEnabled(type_ != hullSimple);
			return true;
		}
		return InsetMathGrid::getStatus(cur, cmd, status);

	case LFUN_TABULAR_FEATURE: {
		if (!allowsTabularFeatures())
			return false;
		string s = cmd.getArg(0);
		if (!rowChangeOK()
		    && (s == "append-row"
			|| s == "delete-row"
			|| s == "copy-row")) {
			status.message(bformat(
				from_utf8(N_("Can't change number of rows in '%1$s'")),
				hullName(type_)));
			status.setEnabled(false);
			return true;
		}
		if (!colChangeOK()
		    && (s == "append-column"
			|| s == "delete-column"
			|| s == "copy-column")) {
			status.message(bformat(
				from_utf8(N_("Can't change number of columns in '%1$s'")),
				hullName(type_)));
			status.setEnabled(false);
			return true;
		}
		if (s == "add-vline-left" || s == "add-vline-right") {
			status.message(bformat(
				from_utf8(N_("Can't add vertical grid lines in '%1$s'")),
				hullName(type_)));
			status.setEnabled(false);
			return true;
		}
		if (s == "valign-top" || s == "valign-middle"
		 || s == "valign-bottom" || s == "align-left"
		 || s == "align-center" || s == "align-right") {
			status.setEnabled(false);
			return true;
		}
		return InsetMathGrid::getStatus(cur, cmd, status);
	}

	default:
		return InsetMathGrid::getStatus(cur, cmd, status);
	}
}


int InsetMathHull::leftMargin() const
{
	return (getType() == hullSimple) ? 0 : InsetMathGrid::leftMargin();
}


int InsetMathHull::rightMargin() const
{
	return (getType() == hullSimple) ? 0 : InsetMathGrid::rightMargin();
}


int InsetMathHull::border() const
{
	return (getType() == hullSimple) ? 0 : InsetMathGrid::border();
}


/////////////////////////////////////////////////////////////////////



void InsetMathHull::handleFont(Cursor & cur, docstring const & arg,
	docstring const & font)
{
	// this whole function is a hack and won't work for incremental font
	// changes...
	cur.recordUndo();
	if (cur.inset().asInsetMath()->name() == font)
		cur.handleFont(to_utf8(font));
	else {
		cur.handleNest(createInsetMath(font, cur.buffer()));
		cur.insert(arg);
	}
}


void InsetMathHull::handleFont2(Cursor & cur, docstring const & arg)
{
	cur.recordUndo();
	Font font;
	bool b;
	font.fromString(to_utf8(arg), b);
	if (font.fontInfo().color() != Color_inherit) {
		MathAtom at = MathAtom(new InsetMathColor(buffer_, true, font.fontInfo().color()));
		cur.handleNest(at);
	}
}


void InsetMathHull::edit(Cursor & cur, bool front, EntryDirection entry_from)
{
	cur.push(*this);
	bool enter_front = (entry_from == Inset::ENTRY_DIRECTION_LEFT ||
		(entry_from == Inset::ENTRY_DIRECTION_IGNORE && front));
	enter_front ? idxFirst(cur) : idxLast(cur);
	// The inset formula dimension is not necessarily the same as the
	// one of the instant preview image, so we have to indicate to the
	// BufferView that a metrics update is needed.
	cur.screenUpdateFlags(Update::Force);
}


/////////////////////////////////////////////////////////////////////


void InsetMathHull::write(ostream & os) const
{
	odocstringstream oss;
	otexrowstream ots(oss);
	TeXMathStream wi(ots, false, false, TeXMathStream::wsDefault);
	oss << "Formula ";
	write(wi);
	os << to_utf8(oss.str());
}


void InsetMathHull::read(Lexer & lex)
{
	MathAtom at;
	mathed_parse_normal(buffer_, at, lex, Parse::TRACKMACRO);
	operator=(*at->asHullInset());
}


bool InsetMathHull::readQuiet(Lexer & lex)
{
	MathAtom at;
	bool success = mathed_parse_normal(buffer_, at, lex, Parse::QUIET);
	if (success)
		operator=(*at->asHullInset());
	return success;
}


int InsetMathHull::plaintext(odocstringstream & os,
        OutputParams const & op, size_t max_length) const
{
	// Try enabling this now that there is a flag as requested at #2275.
	if (buffer().isExporting() && display()) {
		Dimension dim;
		TextMetricsInfo mi;
		metricsT(mi, dim);
		TextPainter tpain(dim.width(), dim.height());
		drawT(tpain, 0, dim.ascent());
		tpain.show(os, 3);
		// reset metrics cache to "real" values
		//metrics();
		return tpain.textheight();
	}

	odocstringstream oss;
	otexrowstream ots(oss);
	Encoding const * const enc = encodings.fromLyXName("utf8");

	TeXMathStream::OutputType ot;
	if (op.find_effective())
		ot = TeXMathStream::wsSearchAdv;
	else
		ot = TeXMathStream::wsDefault;
	// Fix Bug #6139
	if (type_ == hullRegexp) {
		TeXMathStream wi(ots, false, true, ot, enc);
		write(wi);
	}
	else {
		TeXMathStream wi(ots, false, true, ot, enc);
		for (row_type r = 0; r < nrows(); ++r) {
			for (col_type c = 0; c < ncols(); ++c)
				wi << (c == 0 ? "" : "\t") << cell(index(r, c));
			// if it's for the TOC, we write just the first line
			// and do not include the newline.
			if (op.for_toc || op.for_tooltip || oss.str().size() >= max_length)
				break;
			if (r < nrows() - 1)
				wi << "\n";
		}
	}
	docstring const str = oss.str();
	os << str;
	return str.size();
}


void InsetMathHull::docbook(XMLStream & xs, OutputParams const & runparams) const {
	// Choose the tag around the MathML equation.
	docstring name;
	bool doCR = false;
	if (getType() == hullSimple)
		name = from_ascii("inlineequation");
	else {
		doCR = true; // This is a block equation, always have <informalequation> on its own line.
		name = from_ascii("informalequation");
	}

	// DocBook also has <equation>, but it comes with a title.
	// TODO: recognise \tag from amsmath? This would allow having <equation> with a proper title.

	docstring attr;

	bool mathmlNamespaceInline = buffer().params().docbook_mathml_prefix == BufferParams::NoPrefix;
	if (mathmlNamespaceInline)
		attr += "xmlns=\"http://www.w3.org/1998/Math/MathML\"";

	for (row_type i = 0; i < nrows(); ++i) {
		if (!label(i).empty()) {
			if (!attr.empty())
				attr += " ";

			attr += "xml:id=\"" + xml::cleanID(label(i)) + "\"";
			break;
		}
	}

	if (doCR)
		if (!xs.isLastTagCR())
			xs << xml::CR();

	xs << xml::StartTag(name, attr);
	xs << xml::CR();

	// With DocBook 5, MathML must be within its own namespace (defined in Buffer.cpp::writeDocBookSource, except when
	// it should be inlined).
	// Output everything in a separate stream so that this does not interfere with the standard flow of DocBook tags.
	std::string mathmlNamespacePrefix;
	if (!mathmlNamespaceInline) {
		if (buffer().params().docbook_mathml_prefix == BufferParams::MPrefix)
			mathmlNamespacePrefix = "m";
		else if (buffer().params().docbook_mathml_prefix == BufferParams::MMLPrefix)
			mathmlNamespacePrefix = "mml";
	}

	odocstringstream osmath;
	MathMLStream ms(osmath, mathmlNamespacePrefix);

	// Output the MathML subtree.
	// TeX transcription. Avoid MTag/ETag so that there are no extraneous spaces.
	ms << "<" << from_ascii("alt") << " role='tex'" << ">";
	// Workaround for db2latex: db2latex always includes equations with
	// \ensuremath{} or \begin{display}\end{display}
	// so we strip LyX' math environment
	odocstringstream ls;
	otexstream ols(ls);
	TeXMathStream wi(ols, false, false, TeXMathStream::wsDefault, runparams.encoding);
	InsetMathGrid::write(wi);
	ms << from_utf8(subst(subst(to_utf8(ls.str()), "&", "&amp;"), "<", "&lt;"));
	ms << "</" << from_ascii("alt") << ">";

	// Actual transformation of the formula into MathML. This translation may fail (for example, due to custom macros).
	// The new output stream is required to deal with the errors: first write completely the formula into this
	// temporary stream; then, if it is possible without error, then copy it back to the "real" stream. Otherwise,
	// some incomplete tags might be put into the real stream.
	try {
		// First, generate the MathML expression. If there is an error in the generation, this block is not fully
		// executed, and the formula is not output to the DocBook stream.
		odocstringstream ostmp;
		MathMLStream mstmp(ostmp, ms.xmlns());
		mathmlize(mstmp);

		// Choose the display style for the formula, to be output as an attribute near the formula root.
		std::string mathmlAttr;
		if (getType() == hullSimple)
			mathmlAttr = "display=\"inline\"";
		else
			mathmlAttr = "display=\"block\"";

		// Then, output the formula.
		ms << MTag("math", mathmlAttr);
		ms.cr();
		osmath << ostmp.str(); // osmath is not a XMLStream, so no need for XMLStream::ESCAPE_NONE.
		ms << ETag("math");
	} catch (MathExportException const &) {
		ms.cr();
		osmath << "<mathphrase>MathML export failed. Please report this as a bug to the LyX developers: "
			"https://www.lyx.org/trac.</mathphrase>";
	}

	// Output the complete formula to the DocBook stream.
	xs << XMLStream::ESCAPE_NONE << osmath.str();
	xs << xml::CR();
	xs << xml::EndTag(name);
	if (doCR)
		xs << xml::CR();
}


bool InsetMathHull::haveNumbers() const
{
	bool havenumbers = false;
	// inline formulas are never numbered (bug 7351 part 3)
	if (getType() == hullSimple)
		return havenumbers;
	for (size_t i = 0; i != numbered_.size(); ++i) {
		if (numbered(i)) {
			havenumbers = true;
			break;
		}
	}
	return havenumbers;
}


// FIXME XHTML
// We need to do something about alignment here.
//
// This duplicates code from InsetMathGrid, but
// we need access here to number information,
// and we simply do not have that in InsetMathGrid.
void InsetMathHull::htmlize(HtmlStream & os) const
{
	bool const havenumbers = haveNumbers();
	bool const havetable = havenumbers || nrows() > 1 || ncols() > 1;

	if (!havetable) {
		os << cell(index(0, 0));
		return;
	}

	os << MTag("table", "class='mathtable'");
	for (row_type row = 0; row < nrows(); ++row) {
		os << MTag("tr");
		for (col_type col = 0; col < ncols(); ++col) {
			os << MTag("td");
			os << cell(index(row, col));
			os << ETag("td");
		}
		if (havenumbers) {
			os << MTag("td");
			docstring const & num = numbers_[row];
			if (!num.empty())
				os << '(' << num << ')';
		  os << ETag("td");
		}
		os << ETag("tr");
	}
	os << ETag("table");
}


// this duplicates code from InsetMathGrid, but
// we need access here to number information,
// and we simply do not have that in InsetMathGrid.
void InsetMathHull::mathmlize(MathMLStream & ms) const
{
	bool const havetable = haveNumbers() || nrows() > 1 || ncols() > 1;

	// Simplest case: single row, single column, no numbering.
	if (!havetable) {
		ms << cell(index(0, 0));
		return;
	}

	// More complex case: wrap elements in a table.
    if (getType() == hullSimple) {
        ms << MTag("mtable");
    } else if (getType() >= hullAlign && getType() <= hullXXAlignAt) {
		// hullAlign, hullAlignAt, hullXAlignAt, hullXXAlignAt
        string alignment;
        for (col_type col = 0; col < ncols(); ++col) {
            alignment += (col % 2) ? "left " : "right ";
        }
        ms << MTag("mtable", "displaystyle='true' columnalign='" + alignment + "'");
    } else {
        ms << MTag("mtable", "displaystyle='true'");
    }

	for (row_type row = 0; row < nrows(); ++row) {
		// No Wed browser supports mlabeledtr in 2023, even though it has been introduced in
		// MathML 2 (2003). Moreover, it is not supported in MathML 4 Core.
		ms << MTag("mtr");

		for (col_type col = 0; col < ncols(); ++col) {
			ms << MTag("mtd")
			   << cell(index(row, col))
			   << ETag("mtd");
		}

		// fleqn?
		if (haveNumbers()) {
			ms << MTag("mtd");
			docstring const & num = numbers_[row];
			if (!num.empty()) {
				SetMode textmode(ms, true);
				ms << '(' << num << ')';
			}
		    ms << ETag("mtd");
		}

		ms << ETag("mtr");
	}

	ms << ETag("mtable");
}


docstring InsetMathHull::mathAsLatex() const
{
	bool const havenumbers = haveNumbers();
	bool const havetable = havenumbers || nrows() > 1 || ncols() > 1;

	if (!havetable) {
		odocstringstream ls;
		otexrowstream ots(ls);
		TeXMathStream os(ots, false, true, TeXMathStream::wsPreview);
		ModeSpecifier specifier(os, MATH_MODE);
		MathEnsurer ensurer(os, false);

		os << cell(index(0, 0));
		return ls.str();
	}

	odocstringstream ods;
	XMLStream xs(ods);

	xs << xml::StartTag("table", "class='mathtable'");
	for (row_type row = 0; row < nrows(); ++row) {
		xs << xml::StartTag("tr");
		for (col_type col = 0; col < ncols(); ++col) {
			xs << xml::StartTag("td", "class='math'");

			odocstringstream ls;
			otexrowstream ots(ls);
			TeXMathStream os(ots, false, true, TeXMathStream::wsPreview);
			ModeSpecifier specifier(os, MATH_MODE);
			MathEnsurer ensurer(os, false);

			os << cell(index(0, 0));
			// ls.str() contains a raw LaTeX string, which might require some encoding before being valid XML.
			xs << ls.str();

			xs << xml::EndTag("td");
		}
		if (havenumbers) {
			xs << xml::StartTag("td");
			docstring const & num = numbers_[row];
			if (!num.empty()) {
				xs << '(' << num << ')';
			}
			xs << xml::EndTag("td");
		}
		xs << xml::EndTag("tr");
	}
	xs << xml::EndTag("table");

	return ods.str();
}


docstring InsetMathHull::xhtml(XMLStream & xs, OutputParams const & op) const
{
	BufferParams::MathOutput const mathtype =
		buffer().masterBuffer()->params().html_math_output;

	bool success = false;

	// we output all the labels just at the beginning of the equation.
	// this should be fine.
	for (size_t i = 0; i != label_.size(); ++i) {
		InsetLabel const * const il = label_[i];
		if (!il)
			continue;
		il->xhtml(xs, op);
	}

	// FIXME Eventually we would like to do this inset by inset.
	if (mathtype == BufferParams::MathML) {
		odocstringstream os;
		MathMLStream ms(os);
		try {
			mathmlize(ms);
			success = true;
		} catch (MathExportException const &) {}
		if (success) {
			string const name_space_declaration = "xmlns='http://www.w3.org/1998/Math/MathML'";
			if (getType() == hullSimple)
			    xs << xml::StartTag("math", name_space_declaration, true);
			else {
	            if (!xs.isLastTagCR())
	                 xs << xml::CR();
	            xs << xml::StartTag("math", name_space_declaration + " display='block'", true);
		    }

			xs << XMLStream::ESCAPE_NONE
			   << os.str();

			if (!xs.isLastTagCR())
				xs << xml::CR();
			xs << xml::EndTag("math") << xml::CR();
		}
		// In case of failure, generate an image.
	} else if (mathtype == BufferParams::HTML) {
		odocstringstream os;
		HtmlStream ms(os);
		try {
			htmlize(ms);
			success = true;
		} catch (MathExportException const &) {}
		if (success) {
			string const tag = (getType() == hullSimple) ? "span" : "div";
			xs << xml::StartTag(tag, "class='formula'", true)
			   << XMLStream::ESCAPE_NONE
			   << os.str()
			   << xml::EndTag(tag);
		}
		// In case of failure, generate an image.
	}

	// what we actually want is this:
	// if (
	//     ((mathtype == BufferParams::MathML || mathtype == BufferParams::HTML)
	//       && !success)
	//     || mathtype == BufferParams::Images
	//    )
	// but what follows is equivalent, since we'll enter only if either (a) we
	// tried and failed with MathML or HTML or (b) didn't try yet at all but
	// aren't doing LaTeX.
	//
	// so this is for Images.
	if (!success && mathtype != BufferParams::LaTeX) {
		graphics::PreviewImage const * pimage = nullptr;
		if (!op.dryrun) {
			loadPreview(docit_);
			pimage = preview_->getPreviewImage(buffer());
			// FIXME Do we always have png?
		}

		if (pimage || op.dryrun) {
			string const filename = pimage ? pimage->filename().onlyFileName()
			                               : "previewimage.png";
			if (pimage) {
				// if we are not in the master buffer, then we need to see that the
				// generated image is copied there; otherwise, preview fails.
				Buffer const * mbuf = buffer().masterBuffer();
				if (mbuf != &buffer()) {
					string mbtmp = mbuf->temppath();
					FileName const mbufimg(support::addName(mbtmp, filename));
					pimage->filename().copyTo(mbufimg);
				}
				// add the file to the list of files to be exported
				op.exportdata->addExternalFile("xhtml", pimage->filename());
			}

			string const tag = (getType() == hullSimple) ? "span" : "div";
			xs << xml::CR()
			   << xml::StartTag(tag, "style = \"text-align: center;\"")
			   << xml::CompTag("img", "src=\"" + filename + R"(" alt="Mathematical Equation")")
			   << xml::EndTag(tag)
			   << xml::CR();
			success = true;
		}
	}

	// so we'll pass this test if we've failed everything else, or
	// if mathtype was LaTeX, since we won't have entered any of the
	// earlier branches
	if (!success /* || mathtype != BufferParams::LaTeX */) {
		// Unfortunately, we cannot use latexString() because we do not want
		// $...$ or whatever.
		// The returned value already has the correct escaping for HTML.
		docstring const latex = mathAsLatex();

		// class='math' allows for use of jsMath
		// http://www.math.union.edu/~dpvc/jsMath/
		// FIXME XHTML
		// probably should allow for some kind of customization here
		string const tag = (getType() == hullSimple) ? "span" : "div";
		xs << xml::StartTag(tag, "class='math'")
		   << XMLStream::ESCAPE_NONE << latex // Don't escape anything: latex might contain XML.
		   << xml::EndTag(tag)
		   << xml::CR();
	}
	return docstring();
}


void InsetMathHull::toString(odocstream & os) const
{
	odocstringstream ods;
	plaintext(ods, OutputParams(nullptr));
	os << ods.str();
}


void InsetMathHull::forOutliner(docstring & os, size_t const, bool const) const
{
	odocstringstream ods;
	OutputParams op(nullptr);
	op.for_toc = true;
	// FIXME: this results in spilling TeX into the LyXHTML output since the
	// outliner is used to generate the LyXHTML list of figures/etc.
	plaintext(ods, op);
	os += ods.str();
}


string InsetMathHull::contextMenuName() const
{
	return "context-math";
}


void InsetMathHull::recordLocation(DocIterator const & di)
{
	docit_ = di;
}


bool InsetMathHull::canPaintChange(BufferView const &) const
{
	// We let RowPainter do it seamlessly for inline insets
	return display();
}


} // namespace lyx
