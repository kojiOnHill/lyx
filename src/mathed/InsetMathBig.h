// -*- C++ -*-
/**
 * \file InsetMathBig.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef MATH_BIGINSET_H
#define MATH_BIGINSET_H

#include "InsetMath.h"


namespace lyx {

/// Inset for \\bigl & Co.
class InsetMathBig : public InsetMath {
public:
	///
	InsetMathBig(Buffer * buf, docstring const & name, docstring const & delim);
	///
	docstring name() const override;
	/// class is different for l(eft), r(ight) and m(iddle)
	MathClass mathClass() const override;
	///
	void metrics(MetricsInfo & mi, Dimension & dim) const override;
	///
	void draw(PainterInfo & pi, int x, int y) const override;
	///
	void write(TeXMathStream & os) const override;
	///
	void normalize(NormalStream & os) const override;
	///
	void mathmlize(MathMLStream &) const override;
	///
	void htmlize(HtmlStream &) const override;
	///
	void infoize2(odocstream & os) const override;
	///
	static bool isBigInsetDelim(docstring const &);
	///
	InsetCode lyxCode() const override { return MATH_BIG_CODE; }
	///
	void validate(LaTeXFeatures &) const override;
private:
	Inset * clone() const override;
	///
	size_type size() const;
	///
	double increase() const;
	/// name with leading backslash stripped
	docstring word() const;

	/// \\bigl or what?
	docstring const name_;
	/// ( or [ or \\Vert...
	docstring const delim_;
};


} // namespace lyx

#endif
