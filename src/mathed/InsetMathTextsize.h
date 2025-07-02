// -*- C++ -*-
/**
 * \file InsetMathTextsize.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Enrico Forestieri
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef MATH_TEXTSIZEINSET_H
#define MATH_TEXTSIZEINSET_H

#include "InsetMathNest.h"


namespace lyx {


class latexkeys;

/// text-in-math font size changes
class InsetMathTextsize : public InsetMathNest {
public:
	///
	explicit InsetMathTextsize(Buffer * buf, latexkeys const * key);
	/// we inherit the mode
	mode_type currentMode() const override { return current_mode_; }
	/// we write extra braces in any case...
	bool extraBraces() const override { return true; }
	///
	void metrics(MetricsInfo & mi, Dimension & dim) const override;
	///
	void draw(PainterInfo & pi, int x, int y) const override;
	///
	void metricsT(TextMetricsInfo const & mi, Dimension & dim) const override;
	///
	void drawT(TextPainter & pi, int x, int y) const override;
	///
	void writeMath(TeXMathStream & os) const override;
	///
	void normalize(NormalStream &) const override;
	///
	void infoize(odocstream & os) const override;
	///
	int kerning(BufferView const * bv) const override { return cell(0).kerning(bv); }
	///
	InsetCode lyxCode() const override { return MATH_TEXTSIZE_CODE; }

private:
	Inset * clone() const override;
	/// the text-in-math font size to be used on screen
	latexkeys const * key_;
	/// the inherited mode
	mutable mode_type current_mode_;
};


} // namespace lyx
#endif
