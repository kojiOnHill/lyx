// -*- C++ -*-
/**
 * \file InsetMathIntertext.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef MATH_INTERTEXTINSET_H
#define MATH_INTERTEXTINSET_H

#include "InsetMathNest.h"

#include "support/docstring.h"


namespace lyx {


/// Support for LaTeX's \\intertext command

class InsetMathIntertext : public InsetMathNest {
public:
	///
	explicit InsetMathIntertext(Buffer * buf, docstring const & s);
	///
	mode_type currentMode() const override { return MATH_MODE; }
	///
	docstring name() const override;
	///
	void metrics(MetricsInfo & mi, Dimension & dim) const override;
	///
	void draw(PainterInfo & pi, int x, int y) const override;
	///
	void metricsT(TextMetricsInfo const &, Dimension & dim) const override;
	///
	void drawT(TextPainter & pain, int x, int y) const override;
	// docstring str() const { return str_; }
	// ///
	// InsetMathIntertext * asStringInset() override { return this; }
	// ///
	// InsetMathIntertext const * asStringInset() const override { return this; }
	///
	void infoize(odocstream & os) const override;
	///
	//InsetCode lyxCode() const override { return MATH_INTERTEXT_CODE; }

private:
	Inset * clone() const override;
	/// the string
	docstring name_;
};


} // namespace lyx
#endif
