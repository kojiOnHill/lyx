/**
 * \file InsetMathTextsize.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Enrico Forestieri
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathTextsize.h"

#include "MathData.h"
#include "MathParser.h"
#include "MathStream.h"
#include "MathSupport.h"
#include "MetricsInfo.h"

#include "support/gettext.h"
#include "support/lassert.h"
#include "support/lstrings.h"

#include <ostream>

using namespace lyx::support;

namespace lyx {

InsetMathTextsize::InsetMathTextsize(Buffer * buf, latexkeys const * key)
	: InsetMathNest(buf, 1), key_(key), current_mode_(TEXT_MODE)
{
}


Inset * InsetMathTextsize::clone() const
{
	return new InsetMathTextsize(*this);
}


void InsetMathTextsize::metrics(MetricsInfo & mi, Dimension & dim) const
{
	current_mode_ = isTextFont(mi.base.fontname) ? TEXT_MODE : MATH_MODE;

	// size changing commands are noops in math mode
	bool mathmode = current_mode_ == MATH_MODE;

	Changer dummy = mi.base.changeFontSize(to_ascii(key_->name), mathmode);
	cell(0).metrics(mi, dim);
}


void InsetMathTextsize::draw(PainterInfo & pi, int x, int y) const
{
	current_mode_ = isTextFont(pi.base.fontname) ? TEXT_MODE : MATH_MODE;

	// size changing commands are noops in math mode
	bool mathmode = current_mode_ == MATH_MODE;

	Changer dummy = pi.base.changeFontSize(to_ascii(key_->name), mathmode);
	cell(0).draw(pi, x, y);
}


void InsetMathTextsize::metricsT(TextMetricsInfo const & mi, Dimension & dim) const
{
	cell(0).metricsT(mi, dim);
}


void InsetMathTextsize::drawT(TextPainter & pain, int x, int y) const
{
	cell(0).drawT(pain, x, y);
}


void InsetMathTextsize::writeMath(TeXMathStream & os) const
{
	os << "{\\" << key_->name << ' ' << cell(0) << '}';
}


void InsetMathTextsize::normalize(NormalStream & os) const
{
	os << "[size " << key_->name << ' ' << cell(0) << ']';
}


void InsetMathTextsize::infoize(odocstream & os) const
{
	os << bformat(_("Size: %1$s"), key_->name);
}


} // namespace lyx
