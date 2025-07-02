/**
 * \file InsetMathPar.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathPar.h"

#include "MathData.h"
#include "MathStream.h"

#include "MetricsInfo.h"

#include <ostream>

namespace lyx {

InsetMathPar::InsetMathPar(Buffer * buf, MathData const & md)
	: InsetMathHull(buf)
{
	cells_[0] = md;
}


void InsetMathPar::metrics(MetricsInfo & mi, Dimension & dim) const
{
	Changer dummy = mi.base.changeFontSet("textnormal");
	InsetMathGrid::metrics(mi, dim);
}


void InsetMathPar::draw(PainterInfo & pi, int x, int y) const
{
	Changer dummy = pi.base.changeFontSet("textnormal");
	InsetMathGrid::draw(pi, x, y);
}


void InsetMathPar::writeMath(TeXMathStream & os) const
{
	for (idx_type i = 0; i < nargs(); ++i)
		os << cell(i) << "\n";
}


void InsetMathPar::infoize(odocstream & os) const
{
	os << "Type: Paragraph ";
}


Inset * InsetMathPar::clone() const
{
	return new InsetMathPar(*this);
}


} // namespace lyx
