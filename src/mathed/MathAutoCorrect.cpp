/**
 * \file MathAutoCorrect.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "MathAutoCorrect.h"

#include "Cursor.h"
#include "MathData.h"
#include "InsetMath.h"
#include "MathSupport.h"
#include "MathParser.h"

#include "support/debug.h"
#include "support/FileName.h"
#include "support/filetools.h" //  LibFileSearch
#include "support/docstream.h"

#include <fstream>
#include <sstream>

using namespace std;

namespace lyx {

using support::libFileSearch;

namespace {

class Correction {
public:
	///
	/// \brief Correction
	Correction() : from1_(nullptr), from2_(0), to_(nullptr) {}
	///
	bool correct(Cursor & cur, char_type c) const;
	///
	bool read(idocstream & is);
	///
	void write(odocstream & os) const;
private:
	///
	MathData from1_;
	///
	char_type from2_;
	///
	MathData to_;
};


bool Correction::read(idocstream & is)
{
	docstring s1, s2, s3;
	is >> s1 >> s2 >> s3;
	if (!is)
		return false;
	if (s2.size() != 1)
		return false;
	MathData ar1(nullptr), ar3(nullptr);
	mathed_parse_cell(ar1, s1);
	mathed_parse_cell(ar3, s3);
	from1_ = ar1;
	from2_ = s2[0];
	to_    = ar3;
	return true;
}


bool Correction::correct(Cursor & cur, char_type c) const
{
	//LYXERR(Debug::MATHED,
	//	"trying to correct ar: " << at << " from: '" << from1_ << '\'');
	if (from2_ != c)
		return false;
	pos_type n = from1_.size();
	if (cur.pos() < pos_type(from1_.size())) // not enough to match
		return false;
	pos_type start = cur.pos() - from1_.size();

	for (pos_type i = 0; i < n; i++)
		if (asString(cur.cell()[start + i]) != asString(from1_[i]))
			return false;

	LYXERR(Debug::MATHED, "match found! subst in " << cur.cell()
		<< " from: '" << from1_ << "' to '" << to_ << '\'');

	/* To allow undoing the completion, we proceed in 4 steps
	 * - inset the raw character
	 * - split undo group so that we have two separate undo actions
	 * - record undo, delete the character we just entered and the from1_ part
	 * - finally, do the insertion of the correction.
	 */
	cur.insert(c);
	cur.splitUndoGroup();
	cur.recordUndoSelection();
	cur.cell().erase(cur.pos() - n - 1, cur.pos());
	cur.pos() -= n + 1;

	cur.insert(to_);
	return true;
}


#if 0
void Correction::write(odocstream & os) const
{
	os << "from: '" << from1_ << "' and '" << from2_
	   << "' to '" << to_ << '\'' << endl;
}


idocstream & operator>>(idocstream & is, Correction & corr)
{
	corr.read(is);
	return is;
}


odocstream & operator<<(odocstream & os, Correction & corr)
{
	corr.write(os);
	return os;
}
#endif



class Corrections {
public:
	///
	typedef vector<Correction>::const_iterator const_iterator;
	///
	Corrections() {}
	///
	void insert(const Correction & corr) { data_.push_back(corr); }
	///
	bool correct(Cursor & cur, char_type c) const;
private:
	///
	vector<Correction> data_;
};


bool Corrections::correct(Cursor & cur, char_type c) const
{
	for (const_iterator it = data_.begin(); it != data_.end(); ++it)
		if (it->correct(cur, c))
			return true;
	return false;
}


Corrections theCorrections;

void initAutoCorrect()
{
	LYXERR(Debug::MATHED, "reading autocorrect file");
	support::FileName const file = libFileSearch(string(), "autocorrect");
	if (file.empty()) {
		lyxerr << "Could not find autocorrect file" << endl;
		return;
	}

	string line;
	ifstream is(file.toFilesystemEncoding().c_str());
	while (getline(is, line)) {
		if (line.empty() || line[0] == '#') {
			//LYXERR(Debug::MATHED, "ignoring line '" << line << '\'');
			continue;
		}
		idocstringstream il(from_utf8(line));

		//LYXERR(Debug::MATHED, "line '" << line << '\'');
		Correction corr;
		if (corr.read(il)) {
			//LYXERR(Debug::MATHED, "parsed: '" << corr << '\'');
			theCorrections.insert(corr);
		}
	}

	LYXERR(Debug::MATHED, "done reading autocorrections.");
}


} // namespace


bool math_autocorrect(Cursor & cur, char_type c)
{
	static bool initialized = false;

	if (!initialized) {
		initAutoCorrect();
		initialized = true;
	}

	return theCorrections.correct(cur, c);
}
} // namespace lyx
