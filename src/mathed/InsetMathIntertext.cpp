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

#include "Dimension.h"
#include "InsetMathIntertext.h"

#include "MathData.h"
#include "MathSupport.h"
#include "MetricsInfo.h"
#include "TextPainter.h"
#include "support/docstream.h"
#include "support/lstrings.h"

using lyx::support::escape;


namespace lyx {

InsetMathIntertext::InsetMathIntertext(Buffer * buf, docstring const & s)
	: InsetMathNest(buf, 1), name_(s)
{}


Inset * InsetMathIntertext::clone() const
{
	return new InsetMathIntertext(*this);
}


void InsetMathIntertext::metrics(MetricsInfo & mi, Dimension & dim) const
{
	cell(0).metrics(mi, dim);
	dim.asc += 30;
	dim.des += 2;
	dim.wid = 4;
	// mathed_string_dim(mi.base.font, str_, dim);
}


void InsetMathIntertext::draw(PainterInfo & pi, int x, int y) const
{
	cell(0).draw(pi, x-20, y-20);
	// pi.draw(x, y, str_);
}


void InsetMathIntertext::metricsT(TextMetricsInfo const &, Dimension & dim) const
{
	dim.wid = 10;
	dim.asc = 20;
	dim.des = -2;
}


void InsetMathIntertext::drawT(TextPainter & pain, int x, int y) const
{
	//LYXERR0("drawing text '" << to_string(char_) << "' code: " << char_);
	pain.draw(x, y, 'a');
}


docstring InsetMathIntertext::name() const
{
	return name_;
}


void InsetMathIntertext::infoize(odocstream & os) const
{
	os << "Intertext ";
}


} // namespace lyx
