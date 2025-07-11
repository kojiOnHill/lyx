// -*- C++ -*-
/**
 * \file InsetIPAMacro.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef INSET_IPAMACRO_H
#define INSET_IPAMACRO_H


#include "Inset.h"
#include "InsetCollapsible.h"


namespace lyx {

class LaTeXFeatures;

class InsetIPADecoParams
{
public:
	enum Type {
		Toptiebar,
		Bottomtiebar
	};
	///
	InsetIPADecoParams();
	///
	void write(std::ostream & os) const;
	///
	void read(support::Lexer & lex);
	///
	Type type;
};

/////////////////////////////////////////////////////////////////////////
//
// InsetIPADeco
//
/////////////////////////////////////////////////////////////////////////

/// Used to insert IPA decorations
class InsetIPADeco : public InsetCollapsible
{
public:
	///
	InsetIPADeco(Buffer *, std::string const &);
	///
	static std::string params2string(InsetIPADecoParams const &);
	///
	static void string2params(std::string const &, InsetIPADecoParams &);
	///
	InsetIPADecoParams const & params() const { return params_; }
private:
	///
	InsetCode lyxCode() const override { return IPADECO_CODE; }
	///
	docstring layoutName() const override;
	///
	void metrics(MetricsInfo &, Dimension &) const override;
	///
	void draw(PainterInfo & pi, int x, int y) const override;
	///
	void write(std::ostream &) const override;
	///
	void read(support::Lexer & lex) override;
	///
	bool neverIndent() const override { return true; }
	///
	void latex(otexstream &, OutputParams const &) const override;
	///
	int plaintext(odocstringstream & ods, OutputParams const & op,
	              size_t max_length = INT_MAX) const override;
	///
	void docbook(XMLStream &, OutputParams const &) const override;
	///
	docstring xhtml(XMLStream &, OutputParams const &) const override;
	///
	bool getStatus(Cursor &, FuncRequest const &, FuncStatus &) const override;
	///
	void doDispatch(Cursor & cur, FuncRequest & cmd) override;
	///
	void validate(LaTeXFeatures & features) const override;
	///
	bool allowSpellCheck() const override { return false; }
	///
	bool insetAllowed(InsetCode code) const override;
	///
	docstring toolTip(BufferView const & bv, int x, int y) const override;
	///
	Inset * clone() const override { return new InsetIPADeco(*this); }
	/// used by the constructors
	void init();
	///
	friend class InsetIPADecoParams;

	///
	InsetIPADecoParams params_;
};


/////////////////////////////////////////////////////////////////////////
//
// InsetIPAChar
//
/////////////////////////////////////////////////////////////////////////

///  Used to insert special IPA chars that are not available in unicode
class InsetIPAChar : public Inset {
public:

	/// The different kinds of special chars we support
	enum Kind {
		/// falling tone mark
		TONE_FALLING,
		/// rising tone mark
		TONE_RISING,
		/// high-rising tone mark
		TONE_HIGH_RISING,
		/// low-rising tone mark
		TONE_LOW_RISING,
		/// high rising-falling tone mark
		TONE_HIGH_RISING_FALLING
	};

	///
	InsetIPAChar() : Inset(0), kind_(TONE_FALLING) {}
	///
	explicit InsetIPAChar(Kind k);
	///
	Kind kind() const;
	///
	void metrics(MetricsInfo &, Dimension &) const override;
	///
	void draw(PainterInfo & pi, int x, int y) const override;
	///
	void write(std::ostream &) const override;
	/// Will not be used when lyxf3
	void read(support::Lexer & lex) override;
	///
	void latex(otexstream &, OutputParams const &) const override;
	///
	int plaintext(odocstringstream & ods, OutputParams const & op,
	              size_t max_length = INT_MAX) const override;
	///
	void docbook(XMLStream &, OutputParams const &) const override;
	///
	docstring xhtml(XMLStream &, OutputParams const &) const override;
	///
	bool findUsesToString() const override { return true; }
	///
	void toString(odocstream &) const override;
	///
	void forOutliner(docstring &, size_t const, bool const) const override;
	///
	InsetCode lyxCode() const override { return IPACHAR_CODE; }
	/// We don't need \begin_inset and \end_inset
	bool directWrite() const override { return true; }
	///
	void validate(LaTeXFeatures &) const override;

	/// should this inset be handled like a normal character?
	bool isChar() const override { return true; }
	/// is this equivalent to a letter?
	bool isLetter() const override { return true; }
private:
	Inset * clone() const override { return new InsetIPAChar(*this); }

	/// And which kind is this?
	Kind kind_;
};


} // namespace lyx

#endif
