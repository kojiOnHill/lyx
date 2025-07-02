/**
 * \file InsetMathColor.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "ColorSet.h"

#include "InsetMathColor.h"
#include "LaTeXColors.h"
#include "LaTeXFeatures.h"
#include "MathData.h"
#include "MathStream.h"
#include "MathSupport.h"
#include "MetricsInfo.h"

#include "support/gettext.h"
#include "support/lstrings.h"

#include <ostream>

using namespace std;
using namespace lyx::support;

namespace lyx {

InsetMathColor::InsetMathColor(Buffer * buf, bool oldstyle, ColorCode color)
	: InsetMathNest(buf, 1), oldstyle_(oldstyle),
	  color_(from_utf8(lcolor.getLaTeXName(color))),
	  current_mode_(UNDECIDED_MODE)
{}


InsetMathColor::InsetMathColor(Buffer * buf, bool oldstyle,
		docstring const & color)
	: InsetMathNest(buf, 1), oldstyle_(oldstyle), color_(color),
	  current_mode_(UNDECIDED_MODE)
{
	initLaTeXColor(to_ascii(color_));
}


Inset * InsetMathColor::clone() const
{
	return new InsetMathColor(*this);
}


void InsetMathColor::metrics(MetricsInfo & mi, Dimension & dim) const
{
	current_mode_ = isTextFont(mi.base.fontname) ? TEXT_MODE : MATH_MODE;
	Changer dummy = mi.base.changeEnsureMath(current_mode_);

	cell(0).metrics(mi, dim);
}


void InsetMathColor::draw(PainterInfo & pi, int x, int y) const
{
	current_mode_ = isTextFont(pi.base.fontname) ? TEXT_MODE : MATH_MODE;
	Changer dummy = pi.base.changeEnsureMath(current_mode_);

	ColorCode origcol = pi.base.font.color();
	ColorCode col = lcolor.getFromLaTeXName(to_utf8(color_), false);
	pi.base.font.setColor(col != Color_none ? col : lcolor.getFromLyXName(
		  theLaTeXColors().getFromLaTeXColor(to_utf8(color_)), false));
	cell(0).draw(pi, x, y);
	pi.base.font.setColor(origcol);
}


/// color "none" (reset to default) needs special treatment
static bool normalcolor(docstring const & color)
{
	return color == "none";
}


void InsetMathColor::validate(LaTeXFeatures & features) const
{
	InsetMathNest::validate(features);
	if (!normalcolor(color_)) {
		string const col = theLaTeXColors().getFromLaTeXColor(to_utf8(color_));
		if (theLaTeXColors().isLaTeXColor(col)) {
			LaTeXColor const lc = theLaTeXColors().getLaTeXColor(col);
			for (auto const & r : lc.req())
				features.require(r);
			features.require("color");
			if (!lc.model().empty()) {
				features.require("xcolor");
				features.require("xcolor:" + lc.model());
			}
		}
	}
}


void InsetMathColor::writeMath(TeXMathStream & os) const
{
	// We have to ensure correct spacing when the front and/or back
	// atoms are not ordinary ones (bug 11827).
	docstring const frontclass = class_to_string(cell(0).firstMathClass());
	docstring const backclass = class_to_string(cell(0).lastMathClass());
	bool adjchk = os.latex() && !os.inMathClass() && (normalcolor(color_) || oldstyle_);
	bool adjust_front = frontclass != "mathord" && adjchk;
	bool adjust_back = backclass != "mathord" && adjchk;
	docstring const colswitch = normalcolor(color_)
			? from_ascii("{\\normalcolor ")
			: from_ascii("{\\color{") + color_ + from_ascii("}");

	if (adjust_front && adjust_back) {
		os << '\\' << frontclass << colswitch << cell(0).front() << '}';
		if (cell(0).size() > 2) {
			os << colswitch;
			for (size_t i = 1; i < cell(0).size() - 1; ++i)
				os << cell(0)[i];
			os << '}';
		}
		if (cell(0).size() > 1)
			os << '\\' << backclass << colswitch << cell(0).back() << '}';
	} else if (adjust_front) {
		os << '\\' << frontclass << colswitch << cell(0).front() << '}';
		if (cell(0).size() > 1) {
			os << colswitch;
			for (size_t i = 1; i < cell(0).size(); ++i)
				os << cell(0)[i];
			os << '}';
		}
	} else if (adjust_back) {
		os << colswitch;
		for (size_t i = 0; i < cell(0).size() - 1; ++i)
			os << cell(0)[i];
		os << '}' << '\\' << backclass << colswitch << cell(0).back()
		   << '}';
	} else if (normalcolor(color_))
		// reset to default color inside another color inset
		os << "{\\normalcolor " << cell(0) << '}';
	else if (oldstyle_)
		os << "{\\color{" << color_ << '}' << cell(0) << '}';
	else
		os << "\\textcolor{" << color_ << "}{" << cell(0) << '}';
}


void InsetMathColor::normalize(NormalStream & os) const
{
	os << "[color " << color_ << ' ' << cell(0) << ']';
}


void InsetMathColor::infoize(odocstream & os) const
{
	os << bformat(_("Color: %1$s"), color_);
}

void InsetMathColor::initLaTeXColor(string const & col) const
{
	ColorCode cc = lcolor.getFromLaTeXName(col, false);
	if (cc != Color_none)
		return;
	string const lyxname = theLaTeXColors().getFromLaTeXColor(col);
	if (!lyxname.empty()) {
		LaTeXColor const lc = theLaTeXColors().getLaTeXColor(lyxname);
		lcolor.setColor(lyxname, lc.hexname());
		lcolor.setLaTeXName(lyxname, lc.latex());
		lcolor.setGUIName(lyxname, to_ascii(lc.guiname()));
	}
}


} // namespace lyx
