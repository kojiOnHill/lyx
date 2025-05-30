/**
 * \file MathSupport.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alejandro Aguilar Sierra
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "MathSupport.h"

#include "InsetMathFont.h"
#include "InsetMathSymbol.h"
#include "MathData.h"
#include "MathFactory.h"
#include "MathParser.h"
#include "MathStream.h"

#include "Dimension.h"
#include "Encoding.h"
#include "LaTeXFeatures.h"
#include "MetricsInfo.h"

#include "frontends/FontLoader.h"
#include "frontends/FontMetrics.h"
#include "frontends/Painter.h"

#include "support/Changer.h"
#include "support/debug.h"
#include "support/docstream.h"
#include "support/lassert.h"
#include "support/Length.h"
#include "support/textutils.h"

#include <map>
#include <algorithm>

using namespace std;

namespace lyx {

using frontend::Painter;


///
class Matrix {
public:
	///
	Matrix(int, double, double);
	///
	void transform(double &, double &);
private:
	///
	double m_[2][2];
};


Matrix::Matrix(int code, double x, double y)
{
	double const cs = (code & 1) ? 0 : (1 - code);
	double const sn = (code & 1) ? (2 - code) : 0;
	m_[0][0] =  cs * x;
	m_[0][1] =  sn * x;
	m_[1][0] = -sn * y;
	m_[1][1] =  cs * y;
}


void Matrix::transform(double & x, double & y)
{
	double xx = m_[0][0] * x + m_[0][1] * y;
	double yy = m_[1][0] * x + m_[1][1] * y;
	x = xx;
	y = yy;
}



namespace {

/*
 * Internal struct of a drawing: code n x1 y1 ... xn yn, where code is:
 * 0 = end, 1 = line, 2 = polyline, 3 = square line, 4 = square polyline,
 * 5 = ellipse with given center and horizontal and vertical radii,
 * 6 = shifted square polyline drawn at the other end
 */


double const parenthHigh[] = {
	2, 13,
	0.9840, 0.0014, 0.7143, 0.0323, 0.4603, 0.0772,
	0.2540, 0.1278, 0.1746, 0.1966, 0.0952, 0.3300,
	0.0950, 0.5000, 0.0952, 0.6700, 0.1746, 0.8034,
	0.2540, 0.8722, 0.4603, 0.9228, 0.7143, 0.9677,
	0.9840, 0.9986,
	0
};


double const parenth[] = {
	2, 13,
	0.9930, 0.0071, 0.7324, 0.0578, 0.5141, 0.1126,
	0.3380, 0.1714, 0.2183, 0.2333, 0.0634, 0.3621,
	0.0141, 0.5000, 0.0634, 0.6379, 0.2183, 0.7667,
	0.3380, 0.8286, 0.5141, 0.8874, 0.7324, 0.9422,
	0.9930, 0.9929,
	0
};


double const breve[] = {
	2, 8,
	0.100, 0.400,  0.125, 0.550,  0.200, 0.700,  0.400, 0.800,
	0.600, 0.800,  0.800, 0.700,  0.875, 0.550,  0.900, 0.400,
	0
};


double const brace[] = {
	2, 21,
	0.9492, 0.0020, 0.9379, 0.0020, 0.7458, 0.0243,
	0.5819, 0.0527, 0.4859, 0.0892, 0.4463, 0.1278,
	0.4463, 0.3732, 0.4011, 0.4199, 0.2712, 0.4615,
	0.0734, 0.4919, 0.0113, 0.5000, 0.0734, 0.5081,
	0.2712, 0.5385, 0.4011, 0.5801, 0.4463, 0.6268,
	0.4463, 0.8722, 0.4859, 0.9108, 0.5819, 0.9473,
	0.7458, 0.9757, 0.9379, 0.9980, 0.9492, 0.9980,
	0
};


double const mapsto[] = {
	2, 3,
	0.75, 0.015, 0.95, 0.5, 0.75, 0.985,
	1, 0.015, 0.475, 0.945, 0.475,
	1, 0.015, 0.015, 0.015, 0.985,
	0
};


double const lhook[] = {
	4, 7,
	1.40, -0.30, 1.10, 0.00, 0.60, 0.35,
	0.00,  0.60, 0.60, 0.85, 1.10, 1.20,
	1.40,  1.50,
	3, 0.05, 0.6, 1.0, 0.6,
	6, -0.5, 0.0, 6,
	0.65, -0.40, 0.95, -0.35, 1.15, -0.10,
	1.15,  0.25, 0.95,  0.50, 0.65,  0.60,
	0
};


double const rhook[] = {
	4, 6,
	0.50, -0.40, 0.20, -0.35, 0.00, -0.10,
	0.00,  0.25, 0.20,  0.50, 0.50, 0.60,
	3, 0.55, 0.60, 1.00, 0.60,
	6, -0.8, 0.0, 7,
	0.00, -0.30, 0.30, 0.00, 0.80, 0.35, 1.40, 0.60,
	0.80,  0.85, 0.30, 1.20, 0.00, 1.50,
	0
};


double const LRArrow[] = {
	4, 7,
	1.300, -0.300, 1.100, 0.000, 0.600, 0.350,
	0.000,  0.600, 0.600, 0.850, 1.100, 1.200,
	1.300,  1.500,
	6, -0.1, 0.0, 7,
	-0.300, -0.300, -0.100, 0.000,  0.400, 0.350,
	 1.000,  0.600,  0.400, 0.850, -0.100, 1.200,
	-0.300,  1.500,
	3, 0.85, 1.0, 1.0, 1.0,
	3, 0.85, 0.2, 1.0, 0.2,
	0
};


double const LArrow[] = {
	4, 7,
	1.300, -0.300, 1.100, 0.000, 0.600, 0.350,
	0.000,  0.600, 0.600, 0.850, 1.100, 1.200,
	1.300,  1.500,
	3, 0.85, 1.0, 1.0, 1.0,
	3, 0.85, 0.2, 1.0, 0.2,
	0
};


double const lharpoondown[] = {
	4, 4,
	0.0, 0.6, 0.6, 0.85, 1.1, 1.2, 1.4, 1.5,
	3, 0.05, 0.6, 1.0, 0.6,
	0
};


double const lharpoonup[] = {
	4, 4,
	0.0, 0.6, 0.6, 0.35, 1.1, 0.0, 1.4, -0.3,
	3, 0.05, 0.6, 1.0, 0.6,
	0
};


double const lrharpoons[] = {
	4, 4,
	0.0, 0.6, 0.6, 0.35, 1.1, 0.0, 1.4, -0.3,
	3, 0.05, 0.6, 1.0, 0.6,
	3, 0.05, 1.2, 1.0, 1.2,
	6, -1.0, 0.0, 4,
	1.1, 1.3, 0.4, 1.55, -0.1, 1.9, -0.4, 2.2,
	0
};


double const rlharpoons[] = {
	6, -1.0, 0.0, 4,
	-0.4, -0.4, -0.1, -0.1, 0.4, 0.25, 1.0, 0.5,
	3, 0.05, 0.6, 1.0, 0.6,
	3, 0.05, 1.2, 1.0, 1.2,
	4, 4,
	0.0, 1.2, 0.6, 1.45, 1.1, 1.8, 1.4, 2.1,
	0
};


double const vec[] = {
	4, 7,
	0.2000, 0.5000, 0.3000, 0.4000, 0.4000, 0.2500,
	0.5000, 0.0000, 0.6000, 0.2500, 0.7000, 0.4000,
	0.8000, 0.5000,
	3, 0.5000, 0.1000, 0.5000, 0.9500,
	0
};


double const arrow[] = {
	4, 7,
	0.0500, 0.7500, 0.2000, 0.6000, 0.3500, 0.3500,
	0.5000, 0.0500, 0.6500, 0.3500, 0.8000, 0.6000,
	0.9500, 0.7500,
	3, 0.5000, 0.1500, 0.5000, 1.0000,
	0
};


double const Arrow[] = {
	4, 7,
	0.0000, 0.7500, 0.1500, 0.6000, 0.3500, 0.3500,
	0.5000, 0.0500, 0.6500, 0.3500, 0.8500, 0.6000,
	1.0000, 0.7500,
	3, 0.3000, 0.4500, 0.3000, 1.0000,
	3, 0.7000, 0.4500, 0.7000, 1.0000,
	0
};


double const udarrow[] = {
	4, 7,
	0.0500,  0.6500, 0.2000, 0.5000, 0.3500, 0.2500,
	0.5000, -0.0500, 0.6500, 0.2500, 0.8000, 0.5000,
	0.9500, 0.6500,
	6, 0.0, -1.0, 7,
	0.0500,  0.2500, 0.2000, 0.4000, 0.3500, 0.6500,
	0.5000,  0.9500, 0.6500, 0.6500, 0.8000, 0.4000,
	0.9500, 0.2500,
	3, 0.5, 0.0,  0.5, 1.0,
	0
};


double const Udarrow[] = {
	4, 7,
	0.0000, 0.7500, 0.1500, 0.6000, 0.3500, 0.3500,
	0.5000, 0.0500, 0.6500, 0.3500, 0.8500, 0.6000,
	1.0000, 0.7500,
	6, 0.0, -1.0, 7,
	0.0000, 0.2500, 0.1500, 0.4000, 0.3500, 0.6500,
	0.5000, 0.9500, 0.6500, 0.6500, 0.8500, 0.4000,
	1.0000, 0.2500,
	3, 0.3000, 0.4500, 0.3000, 0.9500,
	3, 0.7000, 0.4500, 0.7000, 0.9500,
	0
};


double const brack[] = {
	2, 4,
	0.95, 0.05,  0.05, 0.05,  0.05, 0.95,  0.95, 0.95,
	0
};


double const dbrack[] = {
	2, 4,
	0.95, 0.05,  0.05, 0.05,  0.05, 0.95,  0.95, 0.95,
	2, 2,
	0.50, 0.05,  0.50, 0.95,
	0
};


double const corner[] = {
	2, 3,
	0.95, 0.05,  0.05, 0.05,  0.05, 0.95,
	0
};


double const angle[] = {
	2, 3,
	0.9, 0.05,  0.05, 0.5,  0.9, 0.95,
	0
};


double const slash[] = {
	1, 0.95, 0.05, 0.05, 0.95,
	0
};


double const hline[] = {
	1, 0.00, 0.5, 1.0, 0.5,
	0
};


double const hline2[] = {
	1, 0.00, 0.2, 1.0, 0.2,
	1, 0.00, 0.5, 1.0, 0.5,
	0
};


double const dot[] = {
	5, 0.5, 0.5, 0.1, 0.1,
	0
};


double const ddot[] = {
	5, 0.2, 0.5, 0.1, 0.1,
	5, 0.7, 0.5, 0.1, 0.1,
	0
};


double const dddot[] = {
	5, 0.1, 0.5, 0.1, 0.1,
	5, 0.5, 0.5, 0.1, 0.1,
	5, 0.9, 0.5, 0.1, 0.1,
	0
};


double const ddddot[] = {
	5, -0.1, 0.5, 0.1, 0.1,
	5,  0.3, 0.5, 0.1, 0.1,
	5,  0.7, 0.5, 0.1, 0.1,
	5,  1.1, 0.5, 0.1, 0.1,
	0
};


double const hline3[] = {
	5, 0.15, 0.0, 0.0625, 0.0625,
	5, 0.50, 0.0, 0.0625, 0.0625,
	5, 0.85, 0.0, 0.0625, 0.0625,
	0
};


double const dline3[] = {
	5, 0.25, 0.225, 0.0625, 0.0625,
	5, 0.50, 0.475, 0.0625, 0.0625,
	5, 0.75, 0.725, 0.0625, 0.0625,
	0
};


double const ring[] = {
	2, 13,
	0.5000, 0.7750,  0.6375, 0.7375,  0.7375, 0.6375,  0.7750, 0.5000,
	0.7375, 0.3625,  0.6375, 0.2625,  0.5000, 0.2250,  0.3625, 0.2625,
	0.2625, 0.3625,  0.2250, 0.5000,  0.2625, 0.6375,  0.3625, 0.7375,
	0.5000, 0.7750,
	0
};


double const vert[] = {
	1, 0.5, 0.05,  0.5, 0.95,
	0
};


double const  Vert[] = {
	1, 0.3, 0.05,  0.3, 0.95,
	1, 0.7, 0.05,  0.7, 0.95,
	0
};


double const tilde[] = {
	2, 10,
	0.000, 0.625, 0.050, 0.500, 0.150, 0.350, 0.275, 0.275, 0.400, 0.350,
	0.575, 0.650, 0.700, 0.725, 0.825, 0.650, 0.925, 0.500, 0.975, 0.375,
	0
};


double const wave[] = {
	2, 61,
	0.00, 0.40,
	0.01, 0.39, 0.04, 0.21, 0.05, 0.20, 0.06, 0.21, 0.09, 0.39, 0.10, 0.40,
	0.11, 0.39, 0.14, 0.21, 0.15, 0.20, 0.16, 0.21, 0.19, 0.39, 0.20, 0.40,
	0.21, 0.39, 0.24, 0.21, 0.25, 0.20, 0.26, 0.21, 0.29, 0.39, 0.30, 0.40,
	0.31, 0.39, 0.34, 0.21, 0.35, 0.20, 0.36, 0.21, 0.39, 0.39, 0.40, 0.40,
	0.41, 0.39, 0.44, 0.21, 0.45, 0.20, 0.46, 0.21, 0.49, 0.39, 0.50, 0.40,
	0.51, 0.39, 0.54, 0.21, 0.55, 0.20, 0.56, 0.21, 0.59, 0.39, 0.60, 0.40,
	0.61, 0.39, 0.64, 0.21, 0.65, 0.20, 0.66, 0.21, 0.69, 0.39, 0.70, 0.40,
	0.71, 0.39, 0.74, 0.21, 0.75, 0.20, 0.76, 0.21, 0.79, 0.39, 0.80, 0.40,
	0.81, 0.39, 0.84, 0.21, 0.85, 0.20, 0.86, 0.21, 0.89, 0.39, 0.90, 0.40,
	0.91, 0.39, 0.94, 0.21, 0.95, 0.20, 0.96, 0.21, 0.99, 0.39, 1.00, 0.40,
	0
};


struct deco_struct {
	double const * data;
	int angle;
};

struct named_deco_struct {
	char const * name;
	double const * data;
	int angle;
};

named_deco_struct deco_table[] = {
	// Decorations
	{"widehat",             angle,        3 },
	{"widetilde",           tilde,        0 },
	{"underbar",            hline,        0 },
	{"underline",           hline,        0 },
	{"uline",               hline,        0 },
	{"uuline",              hline2,       0 },
	{"uwave",               wave,         0 },
	{"overline",            hline,        0 },
	{"underbrace",          brace,        1 },
	{"overbrace",           brace,        3 },
	{"overleftarrow",       arrow,        1 },
	{"overrightarrow",      arrow,        3 },
	{"overleftrightarrow",  udarrow,      1 },
	{"xhookleftarrow",      lhook,        0 },
	{"xhookrightarrow",     rhook,        0 },
	{"xleftarrow",          arrow,        1 },
	{"xLeftarrow",          LArrow,       0 },
	{"xleftharpoondown",    lharpoondown, 0 },
	{"xleftharpoonup",      lharpoonup,   0 },
	{"xleftrightharpoons",  lrharpoons,   0 },
	{"xleftrightarrow",     udarrow,      1 },
	{"xLeftrightarrow",     LRArrow,      0 },
	{"xmapsto",             mapsto,       0 },
	{"xrightarrow",         arrow,        3 },
	{"xRightarrow",         LArrow,       2 },
	{"xrightharpoondown",   lharpoonup,   2 },
	{"xrightharpoonup",     lharpoondown, 2 },
	{"xrightleftharpoons",  rlharpoons,   0 },
	{"underleftarrow",      arrow,        1 },
	{"underrightarrow",     arrow,        3 },
	{"underleftrightarrow", udarrow,      1 },
	{"undertilde",          tilde,        0 },
	{"utilde",              tilde,        0 },

	// Delimiters
	{"(",              parenth,    0 },
	{")",              parenth,    2 },
	{"{",              brace,      0 },
	{"}",              brace,      2 },
	{"lbrace",         brace,      0 },
	{"rbrace",         brace,      2 },
	{"[",              brack,      0 },
	{"]",              brack,      2 },
	{"llbracket",      dbrack,     0 },
	{"rrbracket",      dbrack,     2 },
	{"|",              vert,       0 },
	{"/",              slash,      0 },
	{"slash",          slash,      0 },
	{"vert",           vert,       0 },
	{"lvert",          vert,       0 },
	{"rvert",          vert,       0 },
	{"Vert",           Vert,       0 },
	{"lVert",          Vert,       0 },
	{"rVert",          Vert,       0 },
	{"'",              slash,      1 },
	{"<",              angle,      0 },
	{">",              angle,      2 },
	{"\\",             slash,      1 },
	{"backslash",      slash,      1 },
	{"langle",         angle,      0 },
	{"lceil",          corner,     0 },
	{"lfloor",         corner,     1 },
	{"rangle",         angle,      2 },
	{"rceil",          corner,     3 },
	{"rfloor",         corner,     2 },
	{"downarrow",      arrow,      2 },
	{"Downarrow",      Arrow,      2 },
	{"uparrow",        arrow,      0 },
	{"Uparrow",        Arrow,      0 },
	{"updownarrow",    udarrow,    0 },
	{"Updownarrow",    Udarrow,    0 },

	// Accents
	{"ddot",           ddot,       0 },
	{"dddot",          dddot,      0 },
	{"ddddot",         ddddot,     0 },
	{"hat",            angle,      3 },
	{"grave",          slash,      1 },
	{"acute",          slash,      0 },
	{"tilde",          tilde,      0 },
	{"bar",            hline,      0 },
	{"dot",            dot,        0 },
	{"check",          angle,      1 },
	{"breve",          breve,      0 },
	{"vec",            vec,        3 },
	{"mathring",       ring,       0 },

	// Dots
	{"dots",           hline3,     0 },
	{"ldots",          hline3,     0 },
	{"cdots",          hline3,     0 },
	{"vdots",          hline3,     1 },
	{"ddots",          dline3,     0 },
	{"adots",          dline3,     1 },
	{"iddots",         dline3,     1 },
	{"dotsb",          hline3,     0 },
	{"dotsc",          hline3,     0 },
	{"dotsi",          hline3,     0 },
	{"dotsm",          hline3,     0 },
	{"dotso",          hline3,     0 }
};


map<docstring, deco_struct> deco_list;

// sort the table on startup
class init_deco_table {
public:
	init_deco_table() {
		unsigned const n = sizeof(deco_table) / sizeof(deco_table[0]);
		for (named_deco_struct * p = deco_table; p != deco_table + n; ++p) {
			deco_struct d;
			d.data  = p->data;
			d.angle = p->angle;
			deco_list[from_ascii(p->name)] = d;
		}
	}
};

static init_deco_table dummy_deco_table;


deco_struct const * search_deco(docstring const & name)
{
	map<docstring, deco_struct>::const_iterator p = deco_list.find(name);
	return p == deco_list.end() ? 0 : &(p->second);
}


} // namespace


int mathed_font_em(FontInfo const & font)
{
	return theFontMetrics(font).em();
}


int mathed_font_x_height(FontInfo const & font)
{
	return theFontMetrics(font).xHeight();
}

/* The math units. Quoting TeX by Topic, p.205:
 *
 * Spacing around mathematical objects is measured in mu units. A mu
 * is 1/18th part of \fontdimen6 of the font in family 2 in the
 * current style, the ‘quad’ value of the symbol font.
 *
 * A \thickmuskip (default value in plain TeX: 5mu plus 5mu) is
 * inserted around (binary) relations, except where these are preceded
 * or followed by other relations or punctuation, and except if they
 * follow an open, or precede a close symbol.
 *
 * A \medmuskip (default value in plain TeX: 4mu plus 2mu minus 4mu)
 * is put around binary operators.
 *
 * A \thinmuskip (default value in plain TeX: 3mu) follows after
 * punctuation, and is put around inner objects, except where these
 * are followed by a close or preceded by an open symbol, and except
 * if the other object is a large operator or a binary relation.
 *
 * See the file MathClass.cpp for a formal implementation of the rules
 * above.
 */

int mathed_mu(FontInfo const & font, double mu)
{
	MetricsBase mb(nullptr, font);
	return mb.inPixels(Length(mu, Length::MU));
}

int mathed_thinmuskip(FontInfo const & font) { return mathed_mu(font, 3.0); }
int mathed_medmuskip(FontInfo const & font) { return mathed_mu(font, 4.0); }
int mathed_thickmuskip(FontInfo const & font) { return mathed_mu(font, 5.0); }


int mathed_char_width(FontInfo const & font, char_type c)
{
	return theFontMetrics(font).width(c);
}


int mathed_char_kerning(FontInfo const & font, char_type c)
{
	frontend::FontMetrics const & fm = theFontMetrics(font);
	return max(0, fm.rbearing(c) - fm.width(c));
}


double mathed_char_slope(MetricsBase const & mb, char_type c)
{
	bool slanted = isAlphaASCII(c) || Encodings::isMathAlpha(c);
	if (slanted && mb.fontname == "mathnormal")
		return theFontMetrics(mb.font).italicSlope();
	return 0.0;
}


void mathed_string_dim(FontInfo const & font,
		       docstring const & s,
		       Dimension & dim)
{
	frontend::FontMetrics const & fm = theFontMetrics(font);
	dim.asc = 0;
	dim.des = 0;
	for (char_type const c : s) {
		dim.asc = max(dim.asc, fm.ascent(c));
		dim.des = max(dim.des, fm.descent(c));
	}
	dim.wid = fm.width(s);
}


int mathed_string_width(FontInfo const & font, docstring const & s)
{
	return theFontMetrics(font).width(s);
}


void mathed_draw_deco(PainterInfo & pi, int x, int y, int w, int h,
	docstring const & name)
{
	int const lw = pi.base.solidLineThickness();

	if (name == ".") {
		pi.pain.line(x + w/2, y, x + w/2, y + h,
			  Color_cursor, Painter::line_onoffdash, lw);
		return;
	}

	deco_struct const * mds = search_deco(name);
	if (!mds) {
		lyxerr << "Deco was not found. Programming error?" << endl;
		lyxerr << "name: '" << to_utf8(name) << "'" << endl;
		return;
	}

	int const n = (w < h) ? w : h;
	int const r = mds->angle;
	double const * d = mds->data;

	if (h > 70 && (name == "(" || name == ")"))
		d = parenthHigh;

	Matrix mt(r, w, h);
	Matrix sqmt(r, n, n);

	if (r > 0 && r < 3)
		y += h;

	if (r >= 2)
		x += w;

	for (int i = 0; d[i]; ) {
		int code = int(d[i++]);
		if (code == 1 || code == 3) {
			double xx = d[i++];
			double yy = d[i++];
			double x2 = d[i++];
			double y2 = d[i++];
			if (code == 3)
				sqmt.transform(xx, yy);
			else
				mt.transform(xx, yy);
			mt.transform(x2, y2);
			pi.pain.line(
				int(x + xx + 0.5), int(y + yy + 0.5),
				int(x + x2 + 0.5), int(y + y2 + 0.5),
				pi.base.font.color(), Painter::line_solid, lw);
		} else if (code == 5) {
			double xx = d[i++];
			double yy = d[i++];
			double x2 = xx + d[i++];
			double y2 = yy + d[i++];
			mt.transform(xx, yy);
			mt.transform(x2, y2);
			double const xc = x + xx;
			double const yc = y + yy;
			double const rx = x2 - xx;
			double const ry = y2 - yy;
			pi.pain.ellipse(xc, yc, rx, ry,
				pi.base.font.color(), Painter::fill_winding);
		} else {
			int xp[64];
			int yp[64];
			double xshift = (code == 6 ? d[i++] : 0.0);
			double yshift = (code == 6 ? d[i++] : 0.0);
			int const n2 = int(d[i++]);
			for (int j = 0; j < n2; ++j) {
				double xx = d[i++] + xshift;
				double yy = d[i++] + yshift;
//	     lyxerr << ' ' << xx << ' ' << yy << ' ';
				if (code == 4 || code == 6) {
					sqmt.transform(xx, yy);
					if (code == 6) {
						if (r == 0 && xshift == 0.0)
							yy += h;
						else
							xx += w;
					}
				} else
					mt.transform(xx, yy);
				xp[j] = int(x + xx + 0.5);
				yp[j] = int(y + yy + 0.5);
				//  lyxerr << "P[" << j ' ' << xx << ' ' << yy << ' ' << x << ' ' << y << ']';
			}
			pi.pain.lines(xp, yp, n2, pi.base.font.color(),
				Painter::fill_none, Painter::line_solid, lw);
		}
	}
}


docstring const &  mathedSymbol(MetricsBase & mb, latexkeys const * sym)
{
	return (mb.font.style() == DISPLAY_STYLE && !sym->dsp_draw.empty()) ?
		sym->dsp_draw : sym->draw;
}


int mathedSymbolDim(MetricsBase & mb, Dimension & dim, latexkeys const * sym)
{
	LASSERT((bool)sym, return 0);
	//lyxerr << "metrics: symbol: '" << sym->name
	//	<< "' in font: '" << sym->inset
	//	<< "' drawn as: '" << sym->draw
	//	<< "'" << endl;

	bool const italic_upcase_greek = sym->inset == "cmr" &&
		sym->extra == "mathalpha" &&
		mb.fontname == "mathit";
	std::string const font = italic_upcase_greek ? "cmm" : sym->inset;
	bool const change_font = font != "cmr" ||
				(mb.fontname != "mathbb" &&
				 mb.fontname != "mathds" &&
				 mb.fontname != "mathfrak" &&
				 mb.fontname != "mathcal" &&
				 mb.fontname != "mathscr");
	Changer dummy = change_font ? mb.changeFontSet(font) : noChange();
	mathed_string_dim(mb.font, mathedSymbol(mb, sym), dim);
	return mathed_char_kerning(mb.font, mathedSymbol(mb, sym).back());
}


void mathedSymbolDraw(PainterInfo & pi, int x, int y, latexkeys const * sym)
{
	LASSERT((bool)sym, return);
	//lyxerr << "drawing: symbol: '" << sym->name
	//	<< "' in font: '" << sym->inset
	//	<< "' drawn as: '" << sym->draw
	//	<< "'" << endl;

	bool const upcase_greek =
		sym->inset == "cmr" && sym->extra == "mathalpha";
	bool const bold_upcase_greek =
		upcase_greek && pi.base.fontname == "mathbf";
	bool const italic_upcase_greek =
		upcase_greek && pi.base.fontname == "mathit";
	std::string const font = italic_upcase_greek ? "cmm" : sym->inset;
	bool const change_font = font != "cmr" ||
				(pi.base.fontname != "mathbb" &&
				 pi.base.fontname != "mathds" &&
				 pi.base.fontname != "mathfrak" &&
				 pi.base.fontname != "mathcal" &&
				 pi.base.fontname != "mathscr");
	Changer dummy = change_font ? pi.base.changeFontSet(font) : noChange();
	pi.draw(x, y, mathedSymbol(pi.base, sym));
	if (bold_upcase_greek)
		pi.draw(x + 1, y, mathedSymbol(pi.base, sym));
}


void metricsStrRedBlack(MetricsInfo & mi, Dimension & dim, docstring const & str)
{
	FontInfo font = mi.base.font;
	augmentFont(font, "mathnormal");
	mathed_string_dim(font, str, dim);
}


void drawStrRed(PainterInfo & pi, int x, int y, docstring const & str)
{
	FontInfo f = pi.base.font;
	augmentFont(f, "mathnormal");
	f.setColor(Color_latex);
	pi.pain.text(x, y, str, f, Painter::LtR);
}


void drawStrBlack(PainterInfo & pi, int x, int y, docstring const & str)
{
	FontInfo f = pi.base.font;
	augmentFont(f, "mathnormal");
	f.setColor(Color_foreground);
	pi.pain.text(x, y, str, f, Painter::LtR);
}


void math_font_max_dim(FontInfo const & font, int & asc, int & des)
{
	frontend::FontMetrics const & fm = theFontMetrics(font);
	asc = fm.maxAscent();
	des = fm.maxDescent();
}


struct fontinfo {
	string cmd_;
	FontFamily family_;
	FontSeries series_;
	FontShape  shape_;
	ColorCode        color_;
};


FontFamily const inh_family = INHERIT_FAMILY;
FontSeries const inh_series = INHERIT_SERIES;
FontShape  const inh_shape  = INHERIT_SHAPE;


// mathnormal should be the first, otherwise the fallback further down
// does not work
fontinfo fontinfos[] = {
	// math fonts
	// Color_math determines which fonts are math (see isMathFont)
	{"mathnormal",    ROMAN_FAMILY, MEDIUM_SERIES,
			  ITALIC_SHAPE, Color_math},
	{"mathbf",        inh_family, BOLD_SERIES,
			  inh_shape, Color_math},
	{"mathcal",       CMSY_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"mathfrak",      EUFRAK_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"mathrm",        ROMAN_FAMILY, inh_series,
			  UP_SHAPE, Color_math},
	{"mathsf",        SANS_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"mathbb",        MSB_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"mathds",        DS_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"mathtt",        TYPEWRITER_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"mathit",        inh_family, inh_series,
			  ITALIC_SHAPE, Color_math},
	{"mathscr",       RSFS_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"cmex",          CMEX_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"cmm",           CMM_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"cmr",           CMR_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"cmsy",          CMSY_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"eufrak",        EUFRAK_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"msa",           MSA_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"msb",           MSB_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"stmry",         STMARY_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"wasy",          WASY_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"esint",         ESINT_FAMILY, inh_series,
			  inh_shape, Color_math},

	// Text fonts
	{"text",          inh_family, inh_series,
			  inh_shape, Color_foreground},
	{"textbf",        inh_family, BOLD_SERIES,
			  inh_shape, Color_foreground},
	{"textit",        inh_family, inh_series,
			  ITALIC_SHAPE, Color_foreground},
	{"textmd",        inh_family, MEDIUM_SERIES,
			  inh_shape, Color_foreground},
	{"textnormal",    inh_family, inh_series,
			  UP_SHAPE, Color_foreground},
	{"textrm",        ROMAN_FAMILY,
			  inh_series, UP_SHAPE,Color_foreground},
	{"textsc",        inh_family, inh_series,
			  SMALLCAPS_SHAPE, Color_foreground},
	{"textsf",        SANS_FAMILY, inh_series,
			  inh_shape, Color_foreground},
	{"textsl",        inh_family, inh_series,
			  SLANTED_SHAPE, Color_foreground},
	{"texttt",        TYPEWRITER_FAMILY, inh_series,
			  inh_shape, Color_foreground},
	{"textup",        inh_family, inh_series,
			  UP_SHAPE, Color_foreground},

	// TIPA support
	{"textipa",       inh_family, inh_series,
			  inh_shape, Color_foreground},

	// mhchem support
	{"ce",            inh_family, inh_series,
			  inh_shape, Color_foreground},
	{"cf",            inh_family, inh_series,
			  inh_shape, Color_foreground},

	// LyX internal usage
	{"lyxtex",        inh_family, inh_series,
			  UP_SHAPE, Color_latex},
	// FIXME: The following two don't work on OS X, since the Symbol font
	//        uses a different encoding, and is therefore disabled in
	//        FontLoader::available().
	{"lyxsymbol",     SYMBOL_FAMILY, inh_series,
			  inh_shape, Color_math},
	{"lyxboldsymbol", SYMBOL_FAMILY, BOLD_SERIES,
			  inh_shape, Color_math},
	{"lyxblacktext",  ROMAN_FAMILY, MEDIUM_SERIES,
			  UP_SHAPE, Color_foreground},
	{"lyxnochange",   inh_family, inh_series,
			  inh_shape, Color_foreground},
	{"lyxfakebb",     TYPEWRITER_FAMILY, BOLD_SERIES,
			  UP_SHAPE, Color_math},
	{"lyxfakecal",    SANS_FAMILY, MEDIUM_SERIES,
			  ITALIC_SHAPE, Color_math},
	{"lyxfakefrak",   ROMAN_FAMILY, BOLD_SERIES,
			  ITALIC_SHAPE, Color_math}
};


fontinfo * lookupFont(string const & name)
{
	//lyxerr << "searching font '" << name << "'" << endl;
	int const n = sizeof(fontinfos) / sizeof(fontinfo);
	for (int i = 0; i < n; ++i)
		if (fontinfos[i].cmd_ == name) {
			//lyxerr << "found '" << i << "'" << endl;
			return fontinfos + i;
		}
	return 0;
}


fontinfo * searchFont(string const & name)
{
	fontinfo * f = lookupFont(name);
	return f ? f : fontinfos;
	// this should be mathnormal
	//return searchFont("mathnormal");
}


bool isFontName(string const & name)
{
	return lookupFont(name);
}


bool isMathFont(string const & name)
{
	fontinfo * f = lookupFont(name);
	return f && f->color_ == Color_math;
}


bool isTextFont(string const & name)
{
	fontinfo * f = lookupFont(name);
	return f && f->color_ == Color_foreground;
}


FontInfo getFont(string const & name)
{
	FontInfo font;
	augmentFont(font, name);
	return font;
}


void fakeFont(string const & orig, string const & fake)
{
	fontinfo * forig = searchFont(orig);
	fontinfo * ffake = searchFont(fake);
	if (forig && ffake) {
		forig->family_ = ffake->family_;
		forig->series_ = ffake->series_;
		forig->shape_  = ffake->shape_;
		forig->color_  = ffake->color_;
	} else {
		lyxerr << "Can't fake font '" << orig << "' with '"
		       << fake << "'" << endl;
	}
}


void augmentFont(FontInfo & font, string const & name)
{
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		// fake fonts if necessary
		if (!theFontLoader().available(getFont("mathfrak")))
			fakeFont("mathfrak", "lyxfakefrak");
		if (!theFontLoader().available(getFont("mathcal")))
			fakeFont("mathcal", "lyxfakecal");
	}
	fontinfo * info = searchFont(name);
	if (info->family_ != inh_family)
		font.setFamily(info->family_);
	if (info->series_ != inh_series)
		font.setSeries(info->series_);
	if (info->shape_ != inh_shape)
		font.setShape(info->shape_);
	if (info->color_ != Color_none)
		font.setColor(info->color_);
}


bool isAlphaSymbol(MathAtom const & at)
{
	if (at->asCharInset() ||
	    (at->asSymbolInset() &&
	     at->asSymbolInset()->isOrdAlpha()))
		return true;

	if (at->asFontInset()) {
		MathData const & md = at->asFontInset()->cell(0);
		for (const auto & i : md) {
			if (!(i->asCharInset() ||
			      (i->asSymbolInset() &&
			       i->asSymbolInset()->isOrdAlpha())))
				return false;
		}
		return true;
	}
	return false;
}


docstring asString(MathData const & md)
{
	odocstringstream os;
	otexrowstream ots(os);
	TeXMathStream ws(ots);
	ws << md;
	return os.str();
}


void asMathData(docstring const & str, MathData & md, Parse::flags pf)
{
	// If the QUIET flag is set, we are going to parse for either
	// a paste operation or a macro definition. We try to do the
	// right thing in all cases.

	bool quiet = pf & Parse::QUIET;
	bool macro = pf & Parse::MACRODEF;
	if ((str.size() == 1 && quiet) || (!mathed_parse_cell(md, str, pf) && quiet && !macro))
		mathed_parse_cell(md, str, pf | Parse::VERBATIM);

	// set the buffer of the MathData contents
	md.setContentsBuffer();
}


docstring asString(InsetMath const & inset)
{
	odocstringstream os;
	otexrowstream ots(os);
	TeXMathStream ws(ots);
	inset.write(ws);
	return os.str();
}


docstring asString(MathAtom const & at)
{
	odocstringstream os;
	otexrowstream ots(os);
	TeXMathStream ws(ots);
	at->write(ws);
	return os.str();
}


int axis_height(MetricsBase & mb)
{
	Changer dummy = mb.changeFontSet("mathnormal");
	return theFontMetrics(mb.font).ascent('-') - 1;
}


void validate_math_word(LaTeXFeatures & features, docstring const & word)
{
	MathWordList const & words = mathedWordList();
	MathWordList::const_iterator it = words.find(word);
	if (it != words.end()) {
		string const req = it->second.required;
		if (!req.empty())
			features.require(req);
	}
}


} // namespace lyx
