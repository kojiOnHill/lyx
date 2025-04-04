/**
 * \file InsetMathBoldSymbol.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathBoldSymbol.h"

#include "Dimension.h"
#include "MathStream.h"
#include "MathData.h"
#include "MetricsInfo.h"
#include "LaTeXFeatures.h"

#include <ostream>


namespace lyx {

InsetMathBoldSymbol::InsetMathBoldSymbol(Buffer * buf, Kind kind)
	: InsetMathNest(buf, 1), kind_(kind)
{}


Inset * InsetMathBoldSymbol::clone() const
{
	return new InsetMathBoldSymbol(*this);
}


docstring InsetMathBoldSymbol::name() const
{
	switch (kind_) {
	case AMS_BOLD:
		return from_ascii("boldsymbol");
	case BM_BOLD:
		return from_ascii("bm");
	case BM_HEAVY:
		return from_ascii("hm");
	}
	// avoid compiler warning
	return docstring();
}


void InsetMathBoldSymbol::metrics(MetricsInfo & mi, Dimension & dim) const
{
	Changer dummy = mi.base.changeEnsureMath();
	//Changer dummy = mi.base.changeFontSet("mathbf");
	cell(0).metrics(mi, dim);
	++dim.wid;  // for 'double stroke'
}


void InsetMathBoldSymbol::draw(PainterInfo & pi, int x, int y) const
{
	Changer dummy = pi.base.changeEnsureMath();
	//Changer dummy = pi.base.changeFontSet("mathbf");
	cell(0).draw(pi, x, y);
	cell(0).draw(pi, x + 1, y);
}


void InsetMathBoldSymbol::metricsT(TextMetricsInfo const & mi, Dimension & /*dim*/) const
{
	// FIXME: BROKEN!
	Dimension dim;
	cell(0).metricsT(mi, dim);
}


void InsetMathBoldSymbol::drawT(TextPainter & pain, int x, int y) const
{
	cell(0).drawT(pain, x, y);
}


void InsetMathBoldSymbol::validate(LaTeXFeatures & features) const
{
	InsetMathNest::validate(features);
	if (kind_ == AMS_BOLD)
		features.require("amsbsy");
	else
		features.require("bm");
}


void InsetMathBoldSymbol::write(TeXMathStream & os) const
{
	MathEnsurer ensurer(os);
	switch (kind_) {
	case AMS_BOLD:
		os << "\\boldsymbol{" << cell(0) << "}";
		break;
	case BM_BOLD:
		os << "\\bm{" << cell(0) << "}";
		break;
	case BM_HEAVY:
		os << "\\hm{" << cell(0) << "}";
		break;
	}
}


void InsetMathBoldSymbol::mathmlize(MathMLStream & ms) const
{
	if (ms.version() == MathMLVersion::mathmlCore) {
		// All three kinds have the same meaning (and are recognised in
		// MathFontInfo::fromMacro).
		MathFontInfo old_font = ms.fontInfo().mergeWith(MathFontInfo::fromMacro(from_ascii("boldsymbol")));
		ms << cell(0);
		ms.fontInfo() = old_font;
	} else {
		ms << MTagInline("mstyle", "mathvariant='bold'")
		   << cell(0)
		   << ETagInline("mstyle");
	}
}


void InsetMathBoldSymbol::htmlize(HtmlStream & os) const
{
	os << MTag("b") << cell(0) << ETag("b");
}


void InsetMathBoldSymbol::infoize(odocstream & os) const
{
	switch (kind_) {
	case AMS_BOLD:
		os << "Boldsymbol ";
		break;
	case BM_BOLD:
		os << "Boldsymbol (bm)";
		break;
	case BM_HEAVY:
		os << "Heavysymbol (bm)";
		break;
	}
}


} // namespace lyx
