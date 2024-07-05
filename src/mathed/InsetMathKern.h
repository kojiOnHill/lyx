// -*- C++ -*-
/**
 * \file InsetMathKern.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef MATH_CHEATINSET_H
#define MATH_CHEATINSET_H

#include "InsetMath.h"

#include "support/Length.h"


namespace lyx {


/// The \kern primitive
/// Some hack for visual effects

class InsetMathKern : public InsetMath {
public:
	///
	InsetMathKern(Buffer * buf);
	///
	explicit InsetMathKern(Buffer * buf, Length const & wid);
	///
	explicit InsetMathKern(Buffer * buf, docstring const & wid);
	///
	void metrics(MetricsInfo & mi, Dimension & dim) const override;
	///
	void draw(PainterInfo & pi, int x, int y) const override;
	///
	void write(TeXMathStream & os) const override;
	///
	void normalize(NormalStream & ns) const override;
	///
	void mathmlize(MathMLStream &) const override { }
	///
	void htmlize(HtmlStream &) const override { }
	///
	void infoize2(odocstream & os) const override;
	///
	InsetCode lyxCode() const override { return MATH_KERN_CODE; }

private:
	Inset * clone() const override;
	/// width in em
	Length wid_;
};


} // namespace lyx
#endif
