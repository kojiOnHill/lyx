// -*- C++ -*-
/**
 * \file MathParser.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alejandro Aguilar Sierra
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef MATH_PARSER_H
#define MATH_PARSER_H

#include "MathParser_flags.h"

#include "support/docstring.h"


namespace lyx {

class Buffer;
class MathAtom;
class MathData;
class InsetMathGrid;

namespace support { class Lexer; }

///
class latexkeys {
public:
	///
	latexkeys() : hidden(false) {}
	///
	std::string mathml_type() const;
	/// name of the macro or primitive
	docstring name;
	/// name of an inset that handles that macro
	std::string inset;
	/**
	 * The string or symbol to draw.
	 * This is a string of length 1 if \p name is a known symbol, and
	 * the corresponding font is available. In this case it is
	 * NO UCS4 STRING! The only "character" of the string simply denotes
	 * the code point of the symbol in the font. Therefore you have to
	 * be very careful if you pass \c draw to any function that takes a
	 * docstring argument.
	 * If \p name is a known symbol, but the corresponding font is not
	 * available, or if it is a function name, then \c draw contains a
	 * regular UCS4 string (actually \c draw == \c name) that is painted
	 * on screen.
	 */
	docstring draw;
	/// the same thing, but as an alternative in display mode
	// Useful for \sum operator, for example
	docstring dsp_draw;
	/// operator/..., fontname e
	docstring extra;
	/// how is this called as XML entity in MathML?
	docstring xmlname;
	/// required LaTeXFeatures
	std::string required;
	/// Should this macro be hidden from autocompletion (since it requires
	/// user preamble code)?
	bool hidden;
};


///
class UnicodeVariants {
public:
	/// Unicode base character (Latin/Greek letter, number).
	docstring character;
	/// The same Unicode character, in bold.
	docstring bold;
	/// The same Unicode character, in italic.
	docstring italic;
	/// The same Unicode character, in bold italic.
	docstring bold_italic;
	/// The same Unicode character, in script.
	docstring script;
	/// The same Unicode character, in bold script.
	docstring bold_script;
	/// The same Unicode character, in fraktur.
	docstring fraktur;
	/// The same Unicode character, in bold fraktur.
	docstring bold_fraktur;
	/// The same Unicode character, double-struck.
	docstring double_struck;
	/// The same Unicode character, in sans-serif.
	docstring sans;
	/// The same Unicode character, in bold sans-serif.
	docstring bold_sans;
	/// The same Unicode character, in italic sans-serif.
	docstring italic_sans;
	/// The same Unicode character, in bold italic sans-serif.
	docstring bold_italic_sans;
	/// The same Unicode character, in monospace.
	docstring monospace;
};


/// check whether this is a well-known (La)TeX macro or primitive
latexkeys const * in_word_set(docstring const & str);

/// parse formula from a string
bool mathed_parse_normal(Buffer * buf, MathAtom &, docstring const &,
		Parse::flags f = Parse::NORMAL);

/// parse formula from the LyX lexxer
bool mathed_parse_normal(Buffer * buf, MathAtom &, support::Lexer &,
		Parse::flags f = Parse::NORMAL);

/// parse formula from a string into a grid
bool mathed_parse_normal(InsetMathGrid &, docstring const &,
		Parse::flags f = Parse::NORMAL);

/// parse a single cell from a string
bool mathed_parse_cell(MathData & md, docstring const &,
		Parse::flags f = Parse::NORMAL);

/// parse a single cell from a stream. Only use this for reading from .lyx
/// file format, for the reason see Parser::tokenize(std::istream &).
bool mathed_parse_cell(MathData & md, std::istream &,
		Parse::flags f = Parse::NORMAL);

void initParser();


} // namespace lyx

#endif
