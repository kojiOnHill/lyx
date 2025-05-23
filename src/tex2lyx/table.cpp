/**
 * \file table.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 * \author Jean-Marc Lasgouttes
 * \author Georg Baum
 *
 * Full author contact details are available in file CREDITS.
 */

// {[(

#include <config.h>

#include "tex2lyx.h"

#include "Context.h"
#include "Preamble.h"

#include "support/lassert.h"
#include "support/convert.h"
#include "support/lstrings.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

using namespace std;

namespace lyx {


namespace {

class ColInfo {
public:
	ColInfo() : align('n'), valign('n'), rightlines(0), leftlines(0),
		varwidth(false), decimal_point('\0'), vcolumn(false) {}
	/// column alignment
	char align;
	/// vertical alignment
	char valign;
	/// column width
	string width;
	/// special column alignment
	string special;
	/// number of lines on the right
	int rightlines;
	/// number of lines on the left
	int leftlines;
	/// varwidth column
	bool varwidth;
	/// decimal separator
	char decimal_point;
	/// V column type
	bool vcolumn;
	///
	string color;
	///
	string lcoloh;
	///
	string rcoloh;
};


/// row type for longtables
enum LTRowType
{
	/// normal row
	LT_NORMAL,
	/// part of head
	LT_HEAD,
	/// part of head on first page
	LT_FIRSTHEAD,
	/// part of foot
	LT_FOOT,
	/// part of foot on last page
	LT_LASTFOOT
};


class RowInfo {
public:
	RowInfo() : topline(false), bottomline(false), type(LT_NORMAL),
		    caption(false), newpage(false) {}
	/// Does this row have any special setting?
	bool special() const
	{
		return topline || bottomline || !top_space.empty() ||
			!bottom_space.empty() || !interline_space.empty() ||
			type != LT_NORMAL || caption || newpage;
	}
	/// horizontal line above
	bool topline;
	/// horizontal line below
	bool bottomline;
	/// Extra space between the top line and this row
	string top_space;
	/// Extra space between this row and the bottom line
	string bottom_space;
	/// Extra space between the bottom line and the next top line
	string interline_space;
	/// These are for longtabulars only
	/// row type (head, foot, firsthead etc.)
	LTRowType type;
	/// row for a caption
	bool caption;
	/// row for a newpage
	bool newpage;
	///
	string color;
	///
	string lcoloh;
	///
	string rcoloh;
};


/// the numeric values are part of the file format!
enum Multi {
	/// A normal cell
	CELL_NORMAL = 0,
	/// A multicolumn cell. The number of columns is <tt>1 + number
	/// of CELL_PART_OF_MULTICOLUMN cells</tt> that follow directly
	CELL_BEGIN_OF_MULTICOLUMN,
	/// This is a dummy cell (part of a multicolumn cell)
	CELL_PART_OF_MULTICOLUMN,
	///
	CELL_BEGIN_OF_MULTIROW,
	///
	CELL_PART_OF_MULTIROW
};


class CellInfo {
public:
	CellInfo() : multi(CELL_NORMAL), align('n'), valign('n'),
		     leftlines(0), rightlines(0), topline(false),
		     bottomline(false), topline_ltrim(false),
		     topline_rtrim(false), bottomline_ltrim(false),
		     bottomline_rtrim(false), rotate(0), mrxnum(0) {}
	/// cell content
	string content;
	/// multicolumn flag
	Multi multi;
	/// cell alignment
	char align;
	/// vertical cell alignment
	char valign;
	/// number of lines on the left
	int leftlines;
	/// number of lines on the right
	int rightlines;
	/// do we have a line above?
	bool topline;
	/// do we have a line below?
	bool bottomline;
	/// Left trimming of top line
	bool topline_ltrim;
	/// Right trimming of top line
	bool topline_rtrim;
	/// Left trimming of bottom line
	bool bottomline_ltrim;
	/// Right trimming of top line
	bool bottomline_rtrim;
	/// how is the cell rotated?
	int rotate;
	/// width for multicolumn cells
	string width;
	/// special formatting for multicolumn cells
	string special;
	/// multirow offset
	string mroffset;
	/// number of further multirows
	int mrxnum;
	/// color
	string color;
};


class ltType {
public:
	// constructor
	ltType() : set(false), topDL(false), bottomDL(false), empty(false) {}
	// we have this header type (is set in the getLT... functions)
	bool set;
	// double borders on top
	bool topDL;
	// double borders on bottom
	bool bottomDL;
	// used for FirstHeader & LastFooter and if this is true
	// all the rows marked as FirstHeader or LastFooter are
	// ignored in the output and it is set to be empty!
	bool empty;
};


/// translate a horizontal alignment (as stored in ColInfo and CellInfo) to LyX
inline char const * verbose_align(char c)
{
	switch (c) {
	case 'c':
		return "center";
	case 'r':
		return "right";
	case 'l':
		return "left";
	case 'd':
		return "decimal";
	default:
		return "none";
	}
}


/// translate a vertical alignment (as stored in ColInfo and CellInfo) to LyX
inline char const * verbose_valign(char c)
{
	// The default value for no special alignment is "top".
	switch (c) {
	case 'm':
		return "middle";
	case 'b':
		return "bottom";
	case 'p':
	default:
		return "top";
	}
}


// stripped down from tabluar.C. We use it currently only for bools and
// strings
string const write_attribute(string const & name, bool const & b)
{
	// we write only true attribute values so we remove a bit of the
	// file format bloat for tabulars.
	return b ? ' ' + name + "=\"true\"" : string();
}


string const write_attribute(string const & name, string const & s)
{
	return s.empty() ? string() : ' ' + name + "=\"" + s + '"';
}


string const write_attribute(string const & name, int const & i)
{
	// we write only true attribute values so we remove a bit of the
	// file format bloat for tabulars.
	return i ? write_attribute(name, convert<string>(i)) : string();
}


/*! rather brutish way to code table structure in a string:

\verbatim
  \begin{tabular}{ccc}
    1 & 2 & 3\\ \hline
    \multicolumn{2}{c}{4} & 5 //
    6 & 7 \\
    8 \endhead
  \end{tabular}
\endverbatim

 gets "translated" to:

\verbatim
	 HLINE 1                     TAB 2 TAB 3 HLINE          HLINE LINE
  \hline HLINE \multicolumn{2}{c}{4} TAB 5       HLINE          HLINE LINE
	 HLINE 6                     TAB 7       HLINE          HLINE LINE
	 HLINE 8                                 HLINE \endhead HLINE LINE
\endverbatim
 */

char const TAB   = '\001';
char const LINE  = '\002';
char const HLINE = '\004';


/*!
 * Move the information in leftlines, rightlines, align and valign to the
 * special field. This is necessary if the special field is not empty,
 * because LyX ignores leftlines > 1, rightlines > 1, align and valign in
 * this case.
 */
void ci2special(ColInfo & ci)
{
	if (ci.width.empty() && ci.align == 'n')
		// The alignment setting is already in special, since
		// handle_colalign() never stores ci with these settings
		// and ensures that leftlines == 0 and rightlines == 0 in
		// this case.
		return;

	if (ci.decimal_point != '\0') {
		// we only support decimal point natively
		// with 'l' alignment in or 'n' alignment
		// with width in second row
		if (ci.align != 'l' && ci.align != 'n') {
			ci.decimal_point = '\0';
			return;
		} else
			ci.special.clear();
	}

	if (!ci.width.empty()) {
		string arraybackslash;
		if (ci.varwidth)
			arraybackslash = "\\arraybackslash";
		switch (ci.align) {
		case 'l':
			ci.special += ">{\\raggedright" + arraybackslash + "}";
			break;
		case 'r':
			ci.special += ">{\\raggedleft" + arraybackslash + "}";
			break;
		case 'c':
			ci.special += ">{\\centering" + arraybackslash + "}";
			break;
		}
		if (ci.vcolumn)
			ci.special += 'V';
		else if (ci.varwidth)
			ci.special += 'X';
		else if (ci.valign == 'n')
			ci.special += 'p';
		else
			ci.special += ci.valign;
		ci.special += '{' + ci.width + '}';
		ci.width.erase();
	} else
		ci.special += ci.align;

	// LyX can only have one left and one right line.
	for (int i = 1; i < ci.leftlines; ++i)
		ci.special.insert(0, "|");
	for (int i = 1; i < ci.rightlines; ++i)
		ci.special += '|';
	ci.leftlines = min(ci.leftlines, 1);
	ci.rightlines = min(ci.rightlines, 1);
	ci.align = 'n';
	ci.valign = 'n';
}


/*!
 * Handle column specifications for tabulars and multicolumns.
 * The next token of the parser \p p must be an opening brace, and we read
 * everything until the matching closing brace.
 * The resulting column specifications are filled into \p colinfo. This is
 * in an intermediate form. fix_colalign() makes it suitable for LyX output.
 */
void handle_colalign(Parser & p, vector<ColInfo> & colinfo,
		     ColInfo const & start)
{
	if (p.get_token().cat() != catBegin)
		warning_message("Wrong syntax for table column alignment.\n"
			"Expected '{', got '" + p.curr_token().asInput()+ "'");

	ColInfo next = start;
	for (Token t = p.get_token(); p.good() && t.cat() != catEnd;
	     t = p.get_token()) {

		debug_message("t: " + t.asInput() + "  c: '" + t.character() + "'");

		// We cannot handle comments here
		if (t.cat() == catComment) {
			if (t.cs().empty()) {
				// "%\n" combination
				p.skip_spaces();
			} else
				warning_message("Ignoring comment: " + t.asInput());
			continue;
		}

		switch (t.character()) {
			case ' ':
				// whitespace, ignore.
				break;
			case 'c':
			case 'l':
			case 'r':
				// new column, horizontal aligned
				next.align = t.character();
				if (!next.special.empty()) {
					ci2special(next);
					// handle decimal separator
					if (next.decimal_point != '\0') {
						if (!colinfo.empty() && colinfo.back().align == 'r') {
							colinfo.back().align = 'd';
							colinfo.back().decimal_point = next.decimal_point;
						} else
							next.decimal_point = '\0';
					}
				}
				colinfo.push_back(next);
				next = ColInfo();
				break;
			case 'X':
				// varwidth column
				next.varwidth = true;
				if (!next.special.empty())
					ci2special(next);
				colinfo.push_back(next);
				next = ColInfo();
				break;
			case 'V': {
				// V column type (varwidth package)
				string const s = trimSpaceAndEol(p.verbatim_item());
				// V{\linewidth} is treated as a normal column
				// (which allows for line breaks). The V type is
				// automatically set by LyX in this case
				if (s != "\\linewidth" || !next.special.empty()) {
					next.vcolumn = true;
					next.width = s;
					ci2special(next);
				}
				colinfo.push_back(next);
				next = ColInfo();
				break;
			}
			case 'p':
			case 'b':
			case 'm':
				// new column, vertical aligned box
				next.valign = t.character();
				next.width = p.verbatim_item();
				if (!next.special.empty()) {
					ci2special(next);
					// handle decimal separator
					if (next.decimal_point != '\0') {
						if (!colinfo.empty() && colinfo.back().align == 'r') {
							colinfo.back().align = 'd';
							colinfo.back().decimal_point = next.decimal_point;
						} else
							next.decimal_point = '\0';
					}
				}
				colinfo.push_back(next);
				next = ColInfo();
				break;
			case '|':
				// vertical rule
				if (colinfo.empty()) {
					if (next.special.empty())
						++next.leftlines;
					else
						next.special += '|';
				} else if (colinfo.back().special.empty())
					++colinfo.back().rightlines;
				else if (next.special.empty() && p.next_token().cat() != catEnd)
					++next.leftlines;
				else
					colinfo.back().special += '|';
				break;
			case '>': {
				// text before the next column
				string const s = trimSpaceAndEol(p.verbatim_item());
				if (next.special.empty() && next.align == 'n') {
					// Maybe this can be converted to a
					// horizontal alignment setting for
					// fixed width columns
					if (s == "\\raggedleft" || s == "\\raggedleft\\arraybackslash")
						next.align = 'r';
					else if (s == "\\raggedright" || s == "\\raggedright\\arraybackslash")
						next.align = 'l';
					else if (s == "\\centering" || s == "\\centering\\arraybackslash")
						next.align = 'c';
					else if (support::prefixIs(s, "\\columncolor{")) {
						Parser ccparse(s);
						ccparse.get_token();
						string const color = ccparse.getArg('{', '}');
						string const lyxcolor = preamble.getLyXColor(color);
						if (!lyxcolor.empty())
							next.color = lyxcolor;
						if (ccparse.hasOpt()) {
							string const opt = ccparse.getArg('[', ']');
							next.lcoloh = opt;
						}
						if (ccparse.hasOpt()) {
							string const opt = ccparse.getArg('[', ']');
							next.rcoloh = opt;
						}
					} else
						next.special = ">{" + s + '}';
				} else
					next.special += ">{" + s + '}';
				break;
			}
			case '<': {
				// text after the last column
				string const s = trimSpaceAndEol(p.verbatim_item());
				if (colinfo.empty())
					// This is not possible in LaTeX.
					warning_message("Ignoring separator '<{" + s + "}'.");
				else {
					ColInfo & ci = colinfo.back();
					ci2special(ci);
					ci.special += "<{" + s + '}';
				}
				break;
			}
			case '*': {
				if (p.next_token().character() != '{')
					continue;
				// *{n}{arg} means 'n' columns of type 'arg'
				string const num = p.verbatim_item();
				string const arg = p.verbatim_item();
				size_t const n = convert<unsigned int>(num);
				if (!arg.empty() && n > 0) {
					string s("{");
					for (size_t i = 0; i < n; ++i)
						s += arg;
					s += '}';
					Parser p2(s);
					handle_colalign(p2, colinfo, next);
					next = ColInfo();
				} else {
					warning_message("Ignoring column specification"
							" '*{" + num + "}{" + arg + "}'.");
				}
				break;
			}
			case '@':
				// text instead of the column spacing
			case '!': {
				// text in addition to the column spacing
				string const arg =  p.verbatim_item();
				next.special += t.character();
				next.special += '{' + arg + '}';
				string const sarg = arg.size() > 2 ? arg.substr(0, arg.size() - 1) : string();
				if (t.character() == '@' && sarg == "\\extracolsep{0pt}")
					next.decimal_point = arg.back();
				break;
			}
			default: {
				// try user defined column types
				// unknown column types (nargs == -1) are
				// assumed to consume no arguments
				ci2special(next);
				next.special += t.character();
				int const nargs =
					preamble.getSpecialTableColumnArguments(t.character());
				for (int i = 0; i < nargs; ++i)
					next.special += '{' +
						p.verbatim_item() + '}';
				colinfo.push_back(next);
				next = ColInfo();
				break;
			}
			}
	}

	// Maybe we have some column separators that need to be added to the
	// last column?
	ci2special(next);
	if (!next.special.empty()) {
		ColInfo & ci = colinfo.back();
		ci2special(ci);
		ci.special += next.special;
		next.special.erase();
	}
}


/*!
 * Move the left and right lines and alignment settings of the column \p ci
 * to the special field if necessary.
 */
void fix_colalign(ColInfo & ci)
{
	if (ci.leftlines > 1 || ci.rightlines > 1)
		ci2special(ci);
}


/*!
 * LyX can't handle more than one vertical line at the left or right side
 * of a column.
 * This function moves the left and right lines and alignment settings of all
 * columns in \p colinfo to the special field if necessary.
 */
void fix_colalign(vector<ColInfo> & colinfo)
{
	// Try to move extra leftlines to the previous column.
	// We do this only if both special fields are empty, otherwise we
	// can't tell whether the result will be the same.
	for (size_t col = 0; col < colinfo.size(); ++col) {
		if (colinfo[col].leftlines > 1 &&
		    colinfo[col].special.empty() && col > 0 &&
		    colinfo[col - 1].rightlines == 0 &&
		    colinfo[col - 1].special.empty()) {
			++colinfo[col - 1].rightlines;
			--colinfo[col].leftlines;
		}
	}
	// Try to move extra rightlines to the next column
	for (size_t col = 0; col < colinfo.size(); ++col) {
		if (colinfo[col].rightlines > 1 &&
		    colinfo[col].special.empty() &&
		    col < colinfo.size() - 1 &&
		    colinfo[col + 1].leftlines == 0 &&
		    colinfo[col + 1].special.empty()) {
			++colinfo[col + 1].leftlines;
			--colinfo[col].rightlines;
		}
	}
	// Move the lines and alignment settings to the special field if
	// necessary
	for (auto & col : colinfo)
		fix_colalign(col);
}


/*!
 * Parse hlines and similar stuff.
 * \returns whether the token \p t was parsed
 */
bool parse_hlines(Parser & p, Token const & t, string & hlines,
		  bool is_long_tabular)
{
	LASSERT(t.cat() == catEscape, return false);

	if (t.cs() == "hline" || t.cs() == "toprule" || t.cs() == "midrule" ||
	    t.cs() == "bottomrule")
		hlines += '\\' + t.cs();

	else if (t.cs() == "cline")
		hlines += "\\cline{" + p.verbatim_item() + '}';

	else if (t.cs() == "cmidrule") {
		p.pushPosition();
		p.skip_spaces(true);
		// We do not support the optional height argument
		if (p.hasOpt())
			return false;
		// We support the \cmidrule(l){3-4} form but
		// not the trim length parameters (l{<with>}r{<width>})
		string const trim = p.getFullParentheseArg();
		string const range = p.verbatim_item();
		if (!trim.empty()) {
			if (support::contains(trim, "{"))
				return false;
			hlines += "\\cmidrule" + trim + "{" + range + "}";
		} else
			hlines += "\\cmidrule{" + range + '}';
	}

	else if (t.cs() == "addlinespace") {
		p.pushPosition();
		p.skip_spaces(true);
		bool const hasArgument(p.getFullArg('{', '}').first);
		p.popPosition();
		if (hasArgument)
			hlines += "\\addlinespace{" + p.verbatim_item() + '}';
		else
			hlines += "\\addlinespace";
	}

	else if (is_long_tabular && t.cs() == "newpage")
		hlines += "\\newpage";

	else
		return false;

	return true;
}


/// Position in a row
enum RowPosition {
	/// At the very beginning, before the first token
	ROW_START,
	/// After the first token and before any column token
	IN_HLINES_START,
	/// After the first column token. Comments and whitespace are only
	/// treated as tokens in this position
	IN_COLUMNS,
	/// After the first non-column token at the end
	IN_HLINES_END
};


/*!
 * Parse table structure.
 * We parse tables in a two-pass process: This function extracts the table
 * structure (rows, columns, hlines etc.), but does not change the cell
 * content. The cell content is parsed in a second step in handle_tabular().
 */
void parse_table(Parser & p, ostream & os, bool is_long_tabular,
		 RowPosition & pos, unsigned flags)
{
	// table structure commands such as \hline
	string hlines;

	// comments that occur at places where we can't handle them
	string comments;

	while (p.good()) {
		Token const & t = p.get_token();

#ifdef FILEDEBUG
		debugToken(cerr, t, flags);
#endif

		// comments and whitespace in hlines
		switch (pos) {
		case ROW_START:
		case IN_HLINES_START:
		case IN_HLINES_END:
			if (t.cat() == catComment) {
				if (t.cs().empty())
					// line continuation
					p.skip_spaces();
				else
					// We can't handle comments here,
					// store them for later use
					comments += t.asInput();
				continue;
			} else if (t.cat() == catSpace ||
				   t.cat() == catNewline) {
				// whitespace is irrelevant here, we
				// need to recognize hline stuff
				p.skip_spaces();
				continue;
			}
			break;
		case IN_COLUMNS:
			break;
		}

		// We need to handle structure stuff first in order to
		// determine whether we need to output a HLINE separator
		// before the row or not.
		if (t.cat() == catEscape) {
			if (parse_hlines(p, t, hlines, is_long_tabular)) {
				switch (pos) {
				case ROW_START:
					pos = IN_HLINES_START;
					break;
				case IN_COLUMNS:
					pos = IN_HLINES_END;
					break;
				case IN_HLINES_START:
				case IN_HLINES_END:
					break;
				}
				continue;
			}

			else if (t.cs() == "tabularnewline" ||
				 t.cs() == "\\" ||
				 t.cs() == "cr") {
				if (t.cs() == "cr")
					warning_message("Warning: Converting TeX "
							"'\\cr' to LaTeX '\\\\'.");
				// stuff before the line break
				os << comments << HLINE << hlines << HLINE
				   << LINE;
				//warning_message("hlines: " + hlines);
				hlines.erase();
				comments.erase();
				pos = ROW_START;
				continue;
			}

			else if (is_long_tabular &&
				 (t.cs() == "endhead" ||
				  t.cs() == "endfirsthead" ||
				  t.cs() == "endfoot" ||
				  t.cs() == "endlastfoot")) {
				hlines += t.asInput();
				switch (pos) {
				case IN_COLUMNS:
				case IN_HLINES_END:
					// these commands are implicit line
					// breaks
					os << comments << HLINE << hlines
					   << HLINE << LINE;
					hlines.erase();
					comments.erase();
					pos = ROW_START;
					break;
				case ROW_START:
					pos = IN_HLINES_START;
					break;
				case IN_HLINES_START:
					break;
				}
				continue;
			}
		}

		// We need a HLINE separator if we either have no hline
		// stuff at all and are just starting a row or if we just
		// got the first non-hline token.
		switch (pos) {
		case ROW_START:
			// no hline tokens exist, first token at row start
		case IN_HLINES_START:
			// hline tokens exist, first non-hline token at row
			// start
			os << hlines << HLINE << comments;
			hlines.erase();
			comments.erase();
			pos = IN_COLUMNS;
			break;
		case IN_HLINES_END:
			// Oops, there is still cell content or unsupported
			// booktabs commands after hline stuff. The latter are
			// moved to the cell, and the first does not work in
			// LaTeX, so we ignore the hlines.
			os << comments;
			comments.erase();
			if (support::contains(hlines, "\\hline") ||
			    support::contains(hlines, "\\cline") ||
			    support::contains(hlines, "\\newpage"))
				warning_message("Ignoring '" + hlines + "' in a cell");
			else
				os << hlines;
			hlines.erase();
			pos = IN_COLUMNS;
			break;
		case IN_COLUMNS:
			break;
		}

		// If we come here we have normal cell content
		//
		// cat codes
		//
		if (t.cat() == catMath) {
			// we are inside some text mode thingy, so opening new math is allowed
			Token const & n = p.get_token();
			if (n.cat() == catMath) {
				// TeX's $$...$$ syntax for displayed math
				os << "\\[";
				// This does only work because parse_math outputs TeX
				parse_math(p, os, FLAG_SIMPLE, MATH_MODE);
				os << "\\]";
				p.get_token(); // skip the second '$' token
			} else {
				// simple $...$  stuff
				p.putback();
				os << '$';
				// This does only work because parse_math outputs TeX
				parse_math(p, os, FLAG_SIMPLE, MATH_MODE);
				os << '$';
			}
		}

		else if (t.cat() == catSpace
			 || t.cat() == catNewline
			 || t.cat() == catLetter
			 || t.cat() == catSuper
			 || t.cat() == catSub
			 || t.cat() == catOther
			 || t.cat() == catActive
			 || t.cat() == catParameter)
			os << t.cs();

		else if (t.cat() == catBegin) {
			os << '{';
			parse_table(p, os, is_long_tabular, pos,
				    FLAG_BRACE_LAST);
			os << '}';
		}

		else if (t.cat() == catEnd) {
			if (flags & FLAG_BRACE_LAST)
				return;
			warning_message("unexpected '}'");
		}

		else if (t.cat() == catAlign) {
			os << TAB;
			p.skip_spaces();
		}

		else if (t.cat() == catComment)
			os << t.asInput();

		else if (t.cs() == "(") {
			os << "\\(";
			// This does only work because parse_math outputs TeX
			parse_math(p, os, FLAG_SIMPLE2, MATH_MODE);
			os << "\\)";
		}

		else if (t.cs() == "[") {
			os << "\\[";
			// This does only work because parse_math outputs TeX
			parse_math(p, os, FLAG_EQUATION, MATH_MODE);
			os << "\\]";
		}

		else if (t.cs() == "begin") {
			string const name = p.getArg('{', '}');
			active_environments.push_back(name);
			os << "\\begin{" << name << '}';
			// treat the nested environment as a block, don't
			// parse &, \\ etc, because they don't belong to our
			// table if they appear.
			os << p.ertEnvironment(name);
			os << "\\end{" << name << '}';
			active_environments.pop_back();
		}

		else if (t.cs() == "end") {
			if (flags & FLAG_END) {
				// eat environment name
				string const name = p.getArg('{', '}');
				if (name != active_environment())
					p.error("\\end{" + name + "} does not match \\begin{"
						+ active_environment() + "}");
				return;
			}
			p.error("found 'end' unexpectedly");
		}

		else
			os << t.asInput();
	}

	// We can have comments if the last line is incomplete
	os << comments;

	// We can have hline stuff if the last line is incomplete
	if (!hlines.empty()) {
		// this does not work in LaTeX, so we ignore it
		warning_message("Ignoring '" + hlines + "' at end of tabular");
	}
}


void handle_hline_above(RowInfo & ri, vector<CellInfo> & ci)
{
	ri.topline = true;
	for (auto & col : ci)
		col.topline = true;
}


void handle_hline_below(RowInfo & ri, vector<CellInfo> & ci)
{
	ri.bottomline = true;
	for (auto & col : ci)
		col.bottomline = true;
}


void parse_cell_content(ostringstream & os2, Parser & parse, unsigned int flags, Context & newcontext,
			vector< vector<CellInfo> > & cellinfo, vector<ColInfo> & colinfo,
			size_t const row, size_t const col)
{
	bool turn = false;
	int rotate = 0;
	bool varwidth = false;
	if (parse.next_token().cs() == "begin") {
		parse.pushPosition();
		parse.get_token();
		string const env = parse.getArg('{', '}');
		if (env == "sideways" || env == "turn") {
			string angle = "90";
			if (env == "turn") {
				turn = true;
				angle = parse.getArg('{', '}');
			}
			active_environments.push_back(env);
			parse.ertEnvironment(env);
			active_environments.pop_back();
			parse.skip_spaces();
			if (!parse.good() && support::isStrInt(angle))
				rotate = convert<int>(angle);
		} else if (env == "cellvarwidth") {
			active_environments.push_back(env);
			parse.ertEnvironment(env);
			active_environments.pop_back();
			parse.skip_spaces();
			varwidth = true;
		}
		parse.popPosition();
	}
	if (rotate != 0) {
		cellinfo[row][col].rotate = rotate;
		parse.get_token();
		active_environments.push_back(parse.getArg('{', '}'));
		if (turn)
			parse.getArg('{', '}');
		parse_text_in_inset(parse, os2, FLAG_END, false, newcontext);
		active_environments.pop_back();
		preamble.registerAutomaticallyLoadedPackage("rotating");
	} else if (varwidth) {
		parse.get_token();
		active_environments.push_back(parse.getArg('{', '}'));
		// valign arg
		if (parse.hasOpt())
			cellinfo[row][col].valign = parse.getArg('[', ']')[1];
		newcontext.in_table_cell = true;
		parse_text_in_inset(parse, os2, FLAG_END, false, newcontext);
		if (cellinfo[row][col].multi == CELL_NORMAL)
			colinfo[col].align = newcontext.cell_align;
		else
			cellinfo[row][col].align = newcontext.cell_align;
		active_environments.pop_back();
		preamble.registerAutomaticallyLoadedPackage("varwidth");
	} else {
		parse_text_in_inset(parse, os2, flags, false, newcontext);
	}
}


} // anonymous namespace


void handle_tabular(Parser & p, ostream & os, string const & name,
		    string const & tabularwidth, string const & halign,
		    Context const & context)
{
	Context newcontext = context;
	bool const is_long_tabular(name == "longtable" || name == "xltabular");
	bool booktabs = false;
	string tabularvalignment("middle");
	string posopts = p.getOpt();
	if (!posopts.empty()) {
		if (posopts == "[t]")
			tabularvalignment = "top";
		else if (posopts == "[b]")
			tabularvalignment = "bottom";
		else
			warning_message("vertical tabular positioning '"
					+ posopts + "' ignored");
	}

	vector<ColInfo> colinfo;

	// handle column formatting
	handle_colalign(p, colinfo, ColInfo());
	fix_colalign(colinfo);

	// first scan of cells
	// use table mode to keep it minimal-invasive
	// not exactly what's TeX doing...
	vector<string> lines;
	ostringstream ss;
	RowPosition rowpos = ROW_START;
	parse_table(p, ss, is_long_tabular, rowpos, FLAG_END);
	split(ss.str(), lines, LINE);

	vector< vector<CellInfo> > cellinfo(lines.size());
	vector<RowInfo> rowinfo(lines.size());
	ltType endfirsthead;
	ltType endhead;
	ltType endfoot;
	ltType endlastfoot;

	// split into rows
	//warning_message("// split into rows");
	for (size_t row = 0; row < rowinfo.size();) {

		// init row
		cellinfo[row].resize(colinfo.size());
		bool deletelastrow = false;

		// split row
		vector<string> dummy;
		//warning_message("########### LINE: " << lines[row] << "########");
		split(lines[row], dummy, HLINE);

		// handle horizontal line fragments
		// we do only expect this for a last line without '\\'
		if (dummy.size() != 3) {
			if ((dummy.size() != 1 && dummy.size() != 2) ||
			    row != rowinfo.size() - 1)
				warning_message("unexpected dummy size: " + convert<string>(dummy.size())
						+ " content: " + lines[row]);
			dummy.resize(3);
		}
		lines[row] = dummy[1];

		//warning_message("line: " + row + " above 0: " + dummy[0]);
		//warning_message("line: " + row + " below 2: " + dummy[2]);
		//warning_message("line: " + row + " cells 1: " + dummy[1]);

		for (int i = 0; i <= 2; i += 2) {
			//warning_message("   reading from line string '" + dummy[i]);
			Parser p1(dummy[size_type(i)]);
			while (p1.good()) {
				Token t = p1.get_token();
				//warning_message("read token: " + t);
				if (t.cs() == "hline" || t.cs() == "toprule" ||
				    t.cs() == "midrule" ||
				    t.cs() == "bottomrule") {
					if (t.cs() != "hline")
						booktabs = true;
					if (i == 0) {
						if (rowinfo[row].topline) {
							if (row > 0) // extra bottomline above
								handle_hline_below(rowinfo[row - 1], cellinfo[row - 1]);
							else
								warning_message("dropping extra " + t.cs());
							//warning_message("below row: " + row-1);
						} else {
							handle_hline_above(rowinfo[row], cellinfo[row]);
							//warning_message("above row: " + row);
						}
					} else {
						//warning_message("below row: " + row);
						handle_hline_below(rowinfo[row], cellinfo[row]);
					}
				} else if (t.cs() == "cline" || t.cs() == "cmidrule") {
					string trim;
					if (t.cs() == "cmidrule") {
						booktabs = true;
						trim = p1.getFullParentheseArg();
					}
					string arg = p1.verbatim_item();
					//warning_message("read " + t.cs() + " arg: '" + arg + "', trim: '" + trim);
					vector<string> cols;
					split(arg, cols, '-');
					cols.resize(2);
					size_t from = convert<unsigned int>(cols[0]);
					if (from == 0)
						warning_message("Could not parse " + t.cs() + " start column.");
					else
						// 1 based index -> 0 based
						--from;
					if (from >= colinfo.size()) {
						warning_message(t.cs() + " starts at "
							"non existing column " + convert<string>(from + 1));
						from = colinfo.size() - 1;
					}
					size_t to = convert<unsigned int>(cols[1]);
					if (to == 0)
						warning_message("Could not parse " + t.cs() + " end column.");
					else
						// 1 based index -> 0 based
						--to;
					if (to >= colinfo.size()) {
						warning_message(t.cs() + " ends at non existing column "
								+ convert<string>(to + 1));
						to = colinfo.size() - 1;
					}
					for (size_t col = from; col <= to; ++col) {
						//warning_message("row: " + row + " col: " + col + " i: " + i);
						if (i == 0) {
							rowinfo[row].topline = true;
							cellinfo[row][col].topline = true;
							if (support::contains(trim, 'l') && col == from) {
								//rowinfo[row].topline_ltrim = true;
								cellinfo[row][col].topline_ltrim = true;
							}
							else if (support::contains(trim, 'r') && col == to) {
								//rowinfo[row].topline_rtrim = true;
								cellinfo[row][col].topline_rtrim = true;
							}
						} else {
							rowinfo[row].bottomline = true;
							cellinfo[row][col].bottomline = true;
							if (support::contains(trim, 'l') && col == from) {
								//rowinfo[row].bottomline_ltrim = true;
								cellinfo[row][col].bottomline_ltrim = true;
							}
							else if (support::contains(trim, 'r') && col == to) {
								//rowinfo[row].bottomline_rtrim = true;
								cellinfo[row][col].bottomline_rtrim = true;
							}
						}
					}
				} else if (t.cs() == "addlinespace") {
					booktabs = true;
					string const opt = p.next_token().cat() == catBegin ?
							p.verbatim_item() : string();
					if (i == 0) {
						if (opt.empty())
							rowinfo[row].top_space = "default";
						else
							rowinfo[row].top_space = translate_len(opt);
					} else if (rowinfo[row].bottomline) {
						if (opt.empty())
							rowinfo[row].bottom_space = "default";
						else
							rowinfo[row].bottom_space = translate_len(opt);
					} else {
						if (opt.empty())
							rowinfo[row].interline_space = "default";
						else
							rowinfo[row].interline_space = translate_len(opt);
					}
				} else if (t.cs() == "endhead") {
					if (i == 0)
						endhead.empty = true;
					else
						rowinfo[row].type = LT_HEAD;
					for (int r = row - 1; r >= 0; --r) {
						if (rowinfo[r].type != LT_NORMAL)
							break;
						rowinfo[r].type = LT_HEAD;
						endhead.empty = false;
					}
					endhead.set = true;
				} else if (t.cs() == "endfirsthead") {
					if (i == 0)
						endfirsthead.empty = true;
					else
						rowinfo[row].type = LT_FIRSTHEAD;
					for (int r = row - 1; r >= 0; --r) {
						if (rowinfo[r].type != LT_NORMAL)
							break;
						rowinfo[r].type = LT_FIRSTHEAD;
						endfirsthead.empty = false;
					}
					endfirsthead.set = true;
				} else if (t.cs() == "endfoot") {
					if (i == 0)
						endfoot.empty = true;
					else
						rowinfo[row].type = LT_FOOT;
					for (int r = row - 1; r >= 0; --r) {
						if (rowinfo[r].type != LT_NORMAL)
							break;
						rowinfo[r].type = LT_FOOT;
						endfoot.empty = false;
					}
					endfoot.set = true;
				} else if (t.cs() == "endlastfoot") {
					if (i == 0)
						endlastfoot.empty = true;
					else
						rowinfo[row].type = LT_LASTFOOT;
					for (int r = row - 1; r >= 0; --r) {
						if (rowinfo[r].type != LT_NORMAL)
							break;
						rowinfo[r].type = LT_LASTFOOT;
						endlastfoot.empty = false;
					}
					endlastfoot.set = true;
				} else if (t.cs() == "newpage") {
					if (i == 0) {
						if (row > 0)
							rowinfo[row - 1].newpage = true;
						else
							// This does not work in LaTeX
							warning_message("Ignoring "
									"'\\newpage' "
									"before rows.");
					} else
						rowinfo[row].newpage = true;
				} else {
					warning_message("unexpected line token: " + t.cs());
				}
			}
		}

		// LyX ends headers and footers always with \tabularnewline.
		// This causes one additional row in the output.
		// If the last row of a header/footer is empty, we can work
		// around that by removing it.
		if (row > 1) {
			RowInfo test = rowinfo[row-1];
			test.type = LT_NORMAL;
			if (lines[row-1].empty() && !test.special()) {
				switch (rowinfo[row-1].type) {
				case LT_FIRSTHEAD:
					if (rowinfo[row].type != LT_FIRSTHEAD &&
					    rowinfo[row-2].type == LT_FIRSTHEAD)
						deletelastrow = true;
					break;
				case LT_HEAD:
					if (rowinfo[row].type != LT_HEAD &&
					    rowinfo[row-2].type == LT_HEAD)
						deletelastrow = true;
					break;
				case LT_FOOT:
					if (rowinfo[row].type != LT_FOOT &&
					    rowinfo[row-2].type == LT_FOOT)
						deletelastrow = true;
					break;
				case LT_LASTFOOT:
					if (rowinfo[row].type != LT_LASTFOOT &&
					    rowinfo[row-2].type == LT_LASTFOOT)
						deletelastrow = true;
					break;
				case LT_NORMAL:
					break;
				}
			}
		}

		if (deletelastrow) {
			lines.erase(lines.begin() + (row - 1));
			rowinfo.erase(rowinfo.begin() + (row - 1));
			cellinfo.erase(cellinfo.begin() + (row - 1));
			continue;
		}

		// split into cells
		vector<string> cells;
		split(lines[row], cells, TAB);
		for (size_t col = 0, cell = 0; cell < cells.size(); ++col, ++cell) {
			//warning_message("cell content: '" + cells[cell]);
			if (col >= colinfo.size()) {
				// This does not work in LaTeX
				warning_message("Ignoring extra cell '" + cells[cell] + "'.");
				continue;
			}
			string cellcont = cells[cell];
			// For decimal cells, add the content of the second one to the first one
			// of a pair.
			if (colinfo[col].decimal_point != '\0' && colinfo[col].align == 'd' && cell < cells.size() - 1)
				cellcont += colinfo[col].decimal_point + cells[cell + 1];
			Parser parse(cellcont);
			parse.skip_spaces();
			//cells[cell] << "'\n";
			while (parse.good()) {
				if (parse.next_token().cs() == "cellcolor") {
					parse.pushPosition();
					parse.get_token();
					string const color = parse.getArg('{', '}');
					string const lyxcolor = preamble.getLyXColor(color);
					if (!lyxcolor.empty())
						cellinfo[row][col].color = lyxcolor;
					else {
						// no known color, output TeX code
						parse.popPosition();
						ostringstream os2;
						parse_cell_content(os2, parse, FLAG_CELL, newcontext,
									cellinfo, colinfo,
									row, col);
						cellinfo[row][col].leftlines  = colinfo[col].leftlines;
						cellinfo[row][col].rightlines = colinfo[col].rightlines;
						cellinfo[row][col].align      = colinfo[col].align;
						cellinfo[row][col].content += os2.str();
					}
				}
				else if (parse.next_token().cs() == "rowcolor") {
					parse.pushPosition();
					parse.get_token();
					string const color = parse.getArg('{', '}');
					string const lyxcolor = preamble.getLyXColor(color);
					if (!lyxcolor.empty()) {
						rowinfo[row].color = lyxcolor;
						if (parse.hasOpt()) {
							string const opt = parse.getArg('[', ']');
							rowinfo[row].lcoloh = opt;
						}
						if (parse.hasOpt()) {
							string const opt = parse.getArg('[', ']');
							rowinfo[row].rcoloh = opt;
						}
					} else {
						// no known color, output TeX code
						parse.popPosition();
						ostringstream os2;
						parse_cell_content(os2, parse, FLAG_CELL, newcontext,
									cellinfo, colinfo,
									row, col);
						cellinfo[row][col].leftlines  = colinfo[col].leftlines;
						cellinfo[row][col].rightlines = colinfo[col].rightlines;
						cellinfo[row][col].align      = colinfo[col].align;
						cellinfo[row][col].content += os2.str();
					}
				}
				else if (parse.next_token().cs() == "multirow") {
					// We do not support the vpos arg yet.
					if (parse.hasOpt()) {
						string const vpos = parse.getArg('[', ']');
						parse.skip_spaces(true);
						warning_message("Ignoring multirow's vpos arg '"
						     + vpos + "'!");
					}
					// how many cells?
					parse.get_token();
					string nrows = parse.verbatim_item();
					// account for negative nrows value
					bool negative = false;
					if (support::prefixIs(nrows, "-")) {
						negative = true;
						nrows = support::ltrim(nrows, "-");
					}
					size_t const ncells =
						convert<unsigned int>(nrows);
					// We do not support the bigstrut arg yet.
					if (parse.hasOpt()) {
						string const bs = parse.getArg('[', ']');
						parse.skip_spaces(true);
						warning_message("Ignoring multirow's bigstrut arg '"
						     + bs + "'!");
					}
					// the width argument
					string const width = parse.getArg('{', '}');
					// the vmove arg
					string vmove;
					if (parse.hasOpt()) {
						vmove = parse.getArg('[', ']');
						parse.skip_spaces(true);
					}
	
					if (width != "*" && width != "=")
						colinfo[col].width = width;
					
					row_type mrow = row;
					if (negative) {
						if (ncells > row + 1)
							mrow = 0;
						else
							mrow = row + 1 - ncells;
					}
					if (!vmove.empty())
						cellinfo[mrow][col].mroffset = vmove;
					cellinfo[mrow][col].multi = CELL_BEGIN_OF_MULTIROW;
					cellinfo[row][col].leftlines  = colinfo[col].leftlines;
					cellinfo[row][col].rightlines = colinfo[col].rightlines;
					cellinfo[mrow][col].mrxnum = ncells - 1;
	
					ostringstream os2;
					parse.get_token();// skip {
					parse_cell_content(os2, parse, FLAG_BRACE_LAST, newcontext,
								cellinfo, colinfo,
								row, col);
					parse.get_token();// skip }
					if (negative)
						cellinfo[mrow][col].content = os2.str();
					else {
						if (!cellinfo[mrow][col].content.empty()) {
							// This may or may not work in LaTeX,
							// but it does not work in LyX.
							// FIXME: Handle it correctly!
							warning_message("Moving cell content '"
									+ cells[cell]
									+ "' into a multirow cell. "
									  "This will probably not work.");
						}
						cellinfo[mrow][col].content += os2.str();
					}
				} else if (parse.next_token().cs() == "multicolumn") {
					// how many cells?
					parse.get_token();
					size_t const ncells =
						convert<unsigned int>(parse.verbatim_item());
	
					// special cell properties alignment
					vector<ColInfo> t;
					handle_colalign(parse, t, ColInfo());
					parse.skip_spaces(true);
					ColInfo & ci = t.front();
	
					// The logic of LyX for multicolumn vertical
					// lines is too complicated to reproduce it
					// here (see LyXTabular::TeXCellPreamble()).
					// Therefore we simply put everything in the
					// special field.
					ci2special(ci);
	
					cellinfo[row][col].multi      = CELL_BEGIN_OF_MULTICOLUMN;
					cellinfo[row][col].align      = ci.align;
					cellinfo[row][col].special    = ci.special;
					cellinfo[row][col].leftlines  = ci.leftlines;
					cellinfo[row][col].rightlines = ci.rightlines;
					ostringstream os2;
					parse.get_token();// skip {
					parse_cell_content(os2, parse, FLAG_BRACE_LAST, newcontext,
								cellinfo, colinfo,
								row, col);
					parse.get_token();// skip }
					if (!cellinfo[row][col].content.empty()) {
						// This may or may not work in LaTeX,
						// but it does not work in LyX.
						// FIXME: Handle it correctly!
						warning_message("Moving cell content '"
								+ cells[cell]
								+ "' into a multicolumn cell. "
								  "This will probably not work.");
					}
					cellinfo[row][col].content += os2.str();
	
					// add dummy cells for multicol
					for (size_t i = 0; i < ncells - 1; ++i) {
						++col;
						if (col >= colinfo.size()) {
							warning_message("The cell '"
								+ cells[cell]
								+ "' specifies "
								+ convert<string>(ncells)
								+ " columns while the table has only "
								+ convert<string>(colinfo.size())
								+ " columns!"
								+ " Therefore the surplus columns will be ignored.");
							break;
						}
						cellinfo[row][col].multi = CELL_PART_OF_MULTICOLUMN;
						cellinfo[row][col].align = 'c';
					}
	
				} else if (col == 0 && colinfo.size() > 1 &&
					   is_long_tabular &&
					   parse.next_token().cs() == "caption") {
					// longtable caption support in LyX is a hack:
					// Captions require a row of their own with
					// the caption flag set to true, having only
					// one multicolumn cell. The contents of that
					// cell must contain exactly one caption inset
					// and nothing else.
					// Fortunately, the caption flag is only needed
					// for tables with more than one column.
					rowinfo[row].caption = true;
					for (size_t c = 1; c < cells.size(); ++c) {
						if (!cells[c].empty()) {
							warning_message("Moving cell content '"
							     + cells[c]
							     + "' into the caption cell. "
							       "This will probably not work.");
							cells[0] += cells[c];
						}
					}
					cells.resize(1);
					cellinfo[row][col].align      = colinfo[col].align;
					cellinfo[row][col].multi      = CELL_BEGIN_OF_MULTICOLUMN;
					ostringstream os2;
					parse_text_in_inset(parse, os2, FLAG_CELL, false, newcontext);
					cellinfo[row][col].content += os2.str();
					// add dummy multicolumn cells
					for (size_t c = 1; c < colinfo.size(); ++c)
						cellinfo[row][c].multi = CELL_PART_OF_MULTICOLUMN;
				} else if (col == 0 && colinfo.size() > 1 &&
					   is_long_tabular &&
					   parse.next_token().cs() == "caption") {
					// longtable caption support in LyX is a hack:
					// Captions require a row of their own with
					// the caption flag set to true, having only
					// one multicolumn cell. The contents of that
					// cell must contain exactly one caption inset
					// and nothing else.
					// Fortunately, the caption flag is only needed
					// for tables with more than one column.
					rowinfo[row].caption = true;
					for (size_t c = 1; c < cells.size(); ++c) {
						if (!cells[c].empty()) {
							warning_message("Moving cell content '"
							     + cells[c]
							     + "' into the caption cell. "
								"This will probably not work.");
							cells[0] += cells[c];
						}
					}
					cells.resize(1);
					cellinfo[row][col].align      = colinfo[col].align;
					cellinfo[row][col].multi      = CELL_BEGIN_OF_MULTICOLUMN;
					ostringstream os2;
					parse_text_in_inset(parse, os2, FLAG_CELL, false, newcontext);
					cellinfo[row][col].content += os2.str();
					// add dummy multicolumn cells
					for (size_t c = 1; c < colinfo.size(); ++c)
						cellinfo[row][c].multi = CELL_PART_OF_MULTICOLUMN;
				} else {
					ostringstream os2;
					parse_cell_content(os2, parse, FLAG_CELL, newcontext,
								cellinfo, colinfo,
								row, col);
					cellinfo[row][col].leftlines  = colinfo[col].leftlines;
					cellinfo[row][col].rightlines = colinfo[col].rightlines;
					cellinfo[row][col].align      = colinfo[col].align;
					cellinfo[row][col].content += os2.str();
				}
			}
		}

		//warning_message("//  handle almost empty last row what we have");
		// handle almost empty last row
		if (row && lines[row].empty() && row + 1 == rowinfo.size()) {
			//warning_message("remove empty last line");
			if (rowinfo[row].topline)
				rowinfo[row - 1].bottomline = true;
			for (size_t col = 0; col < colinfo.size(); ++col)
				if (cellinfo[row][col].topline)
					cellinfo[row - 1][col].bottomline = true;
			rowinfo.pop_back();
		}

		++row;
	}

	// Now we have the table structure and content in rowinfo, colinfo
	// and cellinfo.
	// Unfortunately LyX has some limitations that we need to work around.

	// Some post work
	for (size_t row = 0; row < rowinfo.size(); ++row) {
		for (size_t col = 0; col < cellinfo[row].size(); ++col) {
			// Convert cells with special content to multicolumn cells
			// (LyX ignores the special field for non-multicolumn cells).
			if (cellinfo[row][col].multi == CELL_NORMAL &&
			    !cellinfo[row][col].special.empty())
				cellinfo[row][col].multi = CELL_BEGIN_OF_MULTICOLUMN;
			// Add multirow dummy cells
			if (row > 0 && (cellinfo[row - 1][col].multi == CELL_PART_OF_MULTIROW
					|| cellinfo[row - 1][col].multi == CELL_BEGIN_OF_MULTIROW)
				    && cellinfo[row - 1][col].mrxnum > 0) {
				// add dummy cells for multirow
				cellinfo[row][col].multi = CELL_PART_OF_MULTIROW;
				cellinfo[row][col].align = 'c';
				cellinfo[row][col].mrxnum = cellinfo[row - 1][col].mrxnum - 1;
			}
		}
	}

	// Distribute lines from rows/columns to cells
	// The code was stolen from convert_tablines() in lyx2lyx/lyx_1_6.py.
	// Each standard cell inherits the settings of the corresponding
	// rowinfo/colinfo. This works because all cells with individual
	// settings were converted to multicolumn cells above.
	// Each multicolumn cell inherits the settings of the rowinfo/colinfo
	// corresponding to the first column of the multicolumn cell (default
	// of the multicol package). This works because the special field
	// overrides the line fields.
	for (size_t row = 0; row < rowinfo.size(); ++row) {
		for (size_t col = 0; col < cellinfo[row].size(); ++col) {
			if (cellinfo[row][col].multi == CELL_NORMAL) {
				cellinfo[row][col].topline = rowinfo[row].topline;
				cellinfo[row][col].bottomline = rowinfo[row].bottomline;
				cellinfo[row][col].leftlines = colinfo[col].leftlines;
				cellinfo[row][col].rightlines = colinfo[col].rightlines;
			} else if (cellinfo[row][col].multi == CELL_BEGIN_OF_MULTICOLUMN) {
				size_t s = col + 1;
				while (s < cellinfo[row].size() &&
				       cellinfo[row][s].multi == CELL_PART_OF_MULTICOLUMN)
					s++;
				if (s < cellinfo[row].size() &&
				    cellinfo[row][s].multi != CELL_BEGIN_OF_MULTICOLUMN)
					cellinfo[row][col].rightlines = colinfo[col].rightlines;
				if (col > 0 && cellinfo[row][col-1].multi == CELL_NORMAL)
					cellinfo[row][col].leftlines = colinfo[col].leftlines;
			} else if (cellinfo[row][col].multi == CELL_BEGIN_OF_MULTIROW) {
				size_t s = row + 1;
				while (s < rowinfo.size() &&
				       cellinfo[s][col].multi == CELL_PART_OF_MULTIROW)
					s++;
				if (s < cellinfo[row].size() &&
				    cellinfo[s][col].multi != CELL_BEGIN_OF_MULTIROW)
					cellinfo[row][col].bottomline = rowinfo[s - 1].bottomline;
				if (row > 0 && cellinfo[row - 1][col].multi == CELL_NORMAL)
					cellinfo[row][col].topline = rowinfo[row].topline;
			}
		}
	}

	if (booktabs)
		preamble.registerAutomaticallyLoadedPackage("booktabs");
	if (name == "longtable")
		preamble.registerAutomaticallyLoadedPackage("longtable");
	else if (name == "xltabular")
		preamble.registerAutomaticallyLoadedPackage("xltabular");
	else if (name == "tabularx")
		preamble.registerAutomaticallyLoadedPackage("tabularx");

	//warning_message("output what we have");
	// output what we have
	size_type cols = colinfo.size();
	for (auto const & col : colinfo) {
		if (col.decimal_point != '\0' && col.align != 'd')
			--cols;
	}
	os << "\n<lyxtabular version=\"3\" rows=\"" << rowinfo.size()
	   << "\" columns=\"" << cols << "\">\n";
	os << "<features"
	   << write_attribute("rotate", context.tablerotation)
	   << write_attribute("booktabs", booktabs)
	   << write_attribute("borderColor", context.current_table_bordercolor)
	   << write_attribute("oddRowsColor", context.current_table_odd_row_color)
	   << write_attribute("evenRowsColor", context.current_table_even_row_color)
	   << write_attribute("startAltRowColors", context.current_table_alt_row_colors_start)
	   << write_attribute("islongtable", is_long_tabular);
	if (is_long_tabular) {
		os << write_attribute("firstHeadTopDL", endfirsthead.topDL)
		   << write_attribute("firstHeadBottomDL", endfirsthead.bottomDL)
		   << write_attribute("firstHeadEmpty", endfirsthead.empty)
		   << write_attribute("headTopDL", endhead.topDL)
		   << write_attribute("headBottomDL", endhead.bottomDL)
		   << write_attribute("footTopDL", endfoot.topDL)
		   << write_attribute("footBottomDL", endfoot.bottomDL)
		   << write_attribute("lastFootTopDL", endlastfoot.topDL)
		   << write_attribute("lastFootBottomDL", endlastfoot.bottomDL)
		   << write_attribute("lastFootEmpty", endlastfoot.empty);
		if (!halign.empty())
			os << write_attribute("longtabularalignment", halign);
	} else
		os << write_attribute("tabularvalignment", tabularvalignment);
		   
	os << write_attribute("tabularwidth", tabularwidth) << ">\n";

	//warning_message("after header");
	for (auto const & col : colinfo) {
		if (col.decimal_point != '\0' && col.align != 'd')
			continue;
		os << "<column alignment=\""
			   << verbose_align(col.align) << "\"";
		if (col.decimal_point != '\0')
			os << " decimal_point=\"" << col.decimal_point << "\"";
		os << " valignment=\""
		   << verbose_valign(col.valign) << "\""
		   << write_attribute("width", translate_len(col.width))
		   << write_attribute("color", col.color)
		   << write_attribute("colorleftoverhang", col.lcoloh)
		   << write_attribute("colorrightoverhang", col.rcoloh)
		   << write_attribute("special", col.special)
		   << write_attribute("varwidth", col.varwidth)
		   << ">\n";
	}
	//warning_message("after cols");

	for (size_t row = 0; row < rowinfo.size(); ++row) {
		os << "<row"
		   << write_attribute("topspace", rowinfo[row].top_space)
		   << write_attribute("bottomspace", rowinfo[row].bottom_space)
		   << write_attribute("interlinespace", rowinfo[row].interline_space)
		   << write_attribute("endhead",
				      rowinfo[row].type == LT_HEAD)
		   << write_attribute("endfirsthead",
				      rowinfo[row].type == LT_FIRSTHEAD)
		   << write_attribute("endfoot",
				      rowinfo[row].type == LT_FOOT)
		   << write_attribute("endlastfoot",
				      rowinfo[row].type == LT_LASTFOOT)
		   << write_attribute("newpage", rowinfo[row].newpage)
		   << write_attribute("caption", rowinfo[row].caption)
		   << write_attribute("color", rowinfo[row].color)
		   << write_attribute("colorleftoverhang", rowinfo[row].lcoloh)
		   << write_attribute("colorrightoverhang", rowinfo[row].rcoloh)
		   << ">\n";
		for (size_t col = 0; col < colinfo.size(); ++col) {
			CellInfo const & cell = cellinfo[row][col];
			if (colinfo[col].decimal_point != '\0' && colinfo[col].align != 'd')
				// These are the second columns in a salign pair. Skip.
				continue;
			os << "<cell";
			if ((cell.multi == CELL_BEGIN_OF_MULTICOLUMN
				    || cell.multi == CELL_PART_OF_MULTICOLUMN)
				   && colinfo[col].align != 'd')
				os << " multicolumn=\"" << cell.multi << "\"";
			if (cell.multi == CELL_BEGIN_OF_MULTIROW
			    || cell.multi == CELL_PART_OF_MULTIROW)
				os << " multirow=\"" << cell.multi << "\"";
			os << " alignment=\"" << verbose_align(cell.align)
			   << "\""
			   << " valignment=\"" << verbose_valign(cell.valign)
			   << "\""
			   << write_attribute("topline", cell.topline)
			   << write_attribute("toplineltrim", cell.topline_ltrim)
			   << write_attribute("toplinertrim", cell.topline_rtrim)
			   << write_attribute("bottomline", cell.bottomline)
			   << write_attribute("bottomlineltrim", cell.bottomline_ltrim)
			   << write_attribute("bottomlinertrim", cell.bottomline_rtrim)
			   << write_attribute("leftline", cell.leftlines > 0)
			   << write_attribute("rightline", cell.rightlines > 0)
			   << write_attribute("color", cell.color)
			   << write_attribute("rotate", cell.rotate)
			   << write_attribute("mroffset", cell.mroffset);
			//warning_message("\nrow: " + row + " col: " + col);
			//if (cell.topline)
			//	warning_message(" topline=\"true\"");
			//if (cell.bottomline)
			//	warning_message(" bottomline=\"true\"");
			os << " usebox=\"none\""
			   << write_attribute("width", translate_len(cell.width));
			if (cell.multi != CELL_NORMAL)
				os << write_attribute("special", cell.special);
			os << ">"
			   << "\n\\begin_inset Text\n"
			   << cell.content
			   << "\n\\end_inset\n"
			   << "</cell>\n";
		}
		os << "</row>\n";
	}

	os << "</lyxtabular>\n";
}




// }])


} // namespace lyx
