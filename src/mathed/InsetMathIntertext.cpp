/**
 * \file InsetMathIntertext.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathIntertext.h"

#include "Buffer.h"
#include "BufferView.h"
#include "Dimension.h"
#include "LaTeXFeatures.h"
#include "MathData.h"
#include "MathFactory.h"
#include "MathStream.h"
#include "MathSupport.h"
#include "MetricsInfo.h"
#include "support/docstream.h"
#include "support/lstrings.h"

using lyx::support::escape;


namespace lyx {

InsetMathIntertext::InsetMathIntertext(Buffer * buf, docstring const & name)
	: InsetMathNest(buf, 1), name_(name)
{}


void InsetMathIntertext::write(TeXMathStream & os) const
{
	ModeSpecifier specifier(os, TEXT_MODE);
	os << '\\' << name_ << '{' << cell(0) << '}';
}


Inset * InsetMathIntertext::clone() const
{
	return new InsetMathIntertext(*this);
}


void InsetMathIntertext::metrics(MetricsInfo & mi, Dimension & dim) const
{
	Changer dummy = mi.base.changeFontSet("text");
	cell(0).metrics(mi, dim);
	dim.wid = 4;
	if (name_ == "shortintertext")
		dim.asc += dim.asc + dim.des + spacer_;
	else
		dim.asc += 2 * (dim.asc + dim.des + spacer_);
	dim.des += 2;
}


void InsetMathIntertext::draw(PainterInfo & pi, int x, int y) const
{
	Changer dummy = pi.base.changeFontSet("text");
	Dimension const dim = dimension(*pi.base.bv);
	int text_y;
	if (name_ == "shortintertext")
		// note that dim is already updated in metrics()
		text_y = y - dim.asc + dim.des + 2 * spacer_;
	else
		text_y = y - dim.asc + 2 * dim.des + 4 * spacer_;
	cell(0).draw(pi, (pi.leftx + x)/2, text_y);
}


docstring InsetMathIntertext::name() const
{
	return name_;
}


void InsetMathIntertext::infoize(odocstream & os) const
{
	os << "Intertext ";
}


void InsetMathIntertext::validate(LaTeXFeatures & features) const
{
	if (name_ == "shortintertext" && features.runparams().isLaTeX())
		features.require("mathtools");
	InsetMathNest::validate(features);
}

} // namespace lyx
