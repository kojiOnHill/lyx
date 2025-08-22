/**
 * \file Counters.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author Martin Vermeer
 * \author André Pönitz
 * \author Richard Kimberly Heck (roman numerals)
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "Counters.h"
#include "Layout.h"

#include "support/convert.h"
#include "support/counter_reps.h"
#include "support/debug.h"
#include "support/docstring.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/Lexer.h"
#include "support/lstrings.h"

#include <algorithm>
#include <sstream>

using namespace std;
using namespace lyx::support;

namespace lyx {


Counter::Counter()
	: initial_value_(0), saved_value_(0)
{
	reset();
}


Counter::Counter(docstring const & mc, docstring const & ls,
		docstring const & lsa, docstring const & prettyformat,
		docstring const & guiname)
	: initial_value_(0), saved_value_(0), parent_(mc), labelstring_(ls),
	  labelstringappendix_(lsa), prettyformat_(prettyformat), guiname_(guiname)
{
	reset();
}


bool Counter::read(Lexer & lex)
{
	enum {
		CT_WITHIN = 1,
		CT_LABELSTRING,
		CT_LABELSTRING_APPENDIX,
		CT_PRETTYFORMAT,
		CT_INITIALVALUE,
		CT_GUINAME,
		CT_LATEXNAME,
		CT_REFFORMAT,
		CT_STEPOTHERCOUNTER,
		CT_END
	};

	LexerKeyword counterTags[] = {
		{ "end", CT_END },
		{ "guiname", CT_GUINAME },
		{ "initialvalue", CT_INITIALVALUE},
		{ "labelstring", CT_LABELSTRING },
		{ "labelstringappendix", CT_LABELSTRING_APPENDIX },
		{ "latexname", CT_LATEXNAME },
		{ "prettyformat", CT_PRETTYFORMAT },
		{ "refformat", CT_REFFORMAT },
		{ "stepothercounter", CT_STEPOTHERCOUNTER },
		{ "within", CT_WITHIN }
	};

	lex.pushTable(counterTags);

	bool getout = false;
	while (!getout && lex.isOK()) {
		int le = lex.lex();
		switch (le) {
			case Lexer::LEX_UNDEF:
				lex.printError("Unknown counter tag `$$Token'");
				continue;
			default:
				break;
		}
		switch (le) {
			case CT_WITHIN:
				lex.next();
				parent_ = lex.getDocString();
				if (parent_ == "none")
					parent_.erase();
				break;
			case CT_INITIALVALUE:
				lex.next();
				initial_value_ = lex.getInteger();
				// getInteger() returns -1 on error, and larger
				// negative values do not make much sense.
				// In the other case, we subtract one, since the
				// counter will be incremented before its first use.
				if (initial_value_ <= -1)
					initial_value_ = 0;
				else
					initial_value_ -= 1;
				break;
			case CT_PRETTYFORMAT:
				lex.next();
				prettyformat_ = lex.getDocString();
				break;
			case CT_REFFORMAT: {
				lex.next();
				docstring const key = lex.getDocString();
				lex.next();
				ref_formats_[key] = lex.getDocString();
				// LYXERR0("refformat: " << key << " => " << value);
				break;
			}
			case CT_LABELSTRING:
				lex.next();
				labelstring_ = lex.getDocString();
				labelstringappendix_ = labelstring_;
				break;
			case CT_LABELSTRING_APPENDIX:
				lex.next();
				labelstringappendix_ = lex.getDocString();
				break;
			case CT_GUINAME:
				lex.next();
				guiname_ = lex.getDocString();
				break;
			case CT_LATEXNAME:
				lex.next();
				latexname_ = lex.getDocString();
				break;
			case CT_STEPOTHERCOUNTER:
				lex.next();
				stepothercounter_ = lex.getDocString();
				if (stepothercounter_ == "none")
					stepothercounter_.erase();
				break;
			case CT_END:
				getout = true;
				break;
		}
		// fall back on GuiName if PrettyFormat is empty
		if (prettyformat_.empty()) {
			if (guiname_.empty())
				prettyformat_ = from_ascii("##");
			else
				prettyformat_ = "## (" + guiname_ + ")";
		}
	}

	// Here if have a full counter if getout == true
	if (!getout)
		LYXERR0("No End tag found for counter!");
	lex.popTable();
	return getout;
}


void Counter::set(int v)
{
	value_ = v;
}


void Counter::addto(int v)
{
	value_ += v;
}


int Counter::value() const
{
	return value_;
}


void Counter::saveValue()
{
	saved_value_ = value_;
}


void Counter::restoreValue()
{
	value_ = saved_value_;
}


void Counter::step()
{
	++value_;
}


void Counter::reset()
{
	value_ = initial_value_;
}


docstring const & Counter::refFormat(docstring const & prefix) const
{
	map<docstring, docstring>::const_iterator it = ref_formats_.find(prefix);
	if (it == ref_formats_.end())
		return prettyformat_;
	return it->second;
}


docstring const & Counter::parent() const
{
	return parent_;
}


bool Counter::checkAndRemoveParent(docstring const & cnt)
{
	if (parent_ != cnt)
		return false;
	parent_ = docstring();
	return true;
}


docstring const & Counter::labelString(bool in_appendix) const
{
	return in_appendix ? labelstringappendix_ : labelstring_;
}


Counter::StringMap & Counter::flatLabelStrings(bool in_appendix) const
{
	return in_appendix ? flatlabelstringappendix_ : flatlabelstring_;
}


Counters::Counters() : appendix_(false), subfloat_(false), longtable_(false)
{
	layout_stack_.push_back(nullptr);
	counter_stack_.push_back(from_ascii(""));
}


void Counters::newCounter(docstring const & newc,
			  docstring const & parentc,
			  docstring const & ls,
			  docstring const & lsa,
			  docstring const & prettyformat,
			  docstring const & guiname)
{
	if (!parentc.empty() && !hasCounter(parentc)) {
		lyxerr << "Parent counter does not exist: "
			   << to_utf8(parentc)
		       << endl;
		return;
	}
	counterList_[newc] = Counter(parentc, ls, lsa, prettyformat, guiname);
}


bool Counters::hasCounter(docstring const & c) const
{
	return counterList_.find(c) != counterList_.end();
}


bool Counters::read(Lexer & lex, docstring const & name, bool makenew)
{
	if (hasCounter(name)) {
		LYXERR(Debug::TCLASS, "Reading existing counter " << to_utf8(name));
		return counterList_[name].read(lex);
	}

	LYXERR(Debug::TCLASS, "Reading new counter " << to_utf8(name));
	Counter cnt;
	bool success = cnt.read(lex);
	// if makenew is false, we will just discard what we read
	if (success && makenew)
		counterList_[name] = cnt;
	else if (!success)
		LYXERR0("Error reading counter `" << name << "'!");
	return success;
}


void Counters::set(docstring const & ctr, int const val)
{
	CounterList::iterator const it = counterList_.find(ctr);
	if (it == counterList_.end()) {
		lyxerr << "set: Counter does not exist: "
		       << to_utf8(ctr) << endl;
		return;
	}
	it->second.set(val);
}


void Counters::addto(docstring const & ctr, int const val)
{
	CounterList::iterator const it = counterList_.find(ctr);
	if (it == counterList_.end()) {
		lyxerr << "addto: Counter does not exist: "
		       << to_utf8(ctr) << endl;
		return;
	}
	it->second.addto(val);
}


int Counters::value(docstring const & ctr) const
{
	CounterList::const_iterator const cit = counterList_.find(ctr);
	if (cit == counterList_.end()) {
		lyxerr << "value: Counter does not exist: "
		       << to_utf8(ctr) << endl;
		return 0;
	}
	return cit->second.value();
}


void Counters::saveValue(docstring const & ctr) const
{
	CounterList::const_iterator const cit = counterList_.find(ctr);
	if (cit == counterList_.end()) {
		lyxerr << "value: Counter does not exist: "
		       << to_utf8(ctr) << endl;
		return;
	}
	Counter const & cnt = cit->second;
	Counter & ccnt = const_cast<Counter &>(cnt);
	ccnt.saveValue();
}


void Counters::restoreValue(docstring const & ctr) const
{
	CounterList::const_iterator const cit = counterList_.find(ctr);
	if (cit == counterList_.end()) {
		lyxerr << "value: Counter does not exist: "
		       << to_utf8(ctr) << endl;
		return;
	}
	Counter const & cnt = cit->second;
	Counter & ccnt = const_cast<Counter &>(cnt);
	ccnt.restoreValue();
}


void Counters::resetChildren(docstring const & count)
{
	for (auto & ctr : counterList_) {
		if (ctr.second.parent() == count) {
			ctr.second.reset();
			resetChildren(ctr.first);
		}
	}
}


void Counters::stepParent(docstring const & ctr, UpdateType utype)
{
	CounterList::iterator it = counterList_.find(ctr);
	if (it == counterList_.end()) {
		lyxerr << "step: Counter does not exist: "
		       << to_utf8(ctr) << endl;
		return;
	}
	step(it->second.parent(), utype);
}


void Counters::step(docstring const & ctr, UpdateType /* deleted */)
{
	CounterList::iterator it = counterList_.find(ctr);
	if (it == counterList_.end()) {
		lyxerr << "step: Counter does not exist: "
		       << to_utf8(ctr) << endl;
		return;
	}

	it->second.step();
	if (!it->second.stepOtherCounter().empty())
		step(it->second.stepOtherCounter(), InternalUpdate);

	LBUFERR(!counter_stack_.empty());
	counter_stack_.pop_back();
	counter_stack_.push_back(ctr);

	resetChildren(ctr);
}


docstring const & Counters::guiName(docstring const & cntr) const
{
	CounterList::const_iterator it = counterList_.find(cntr);
	if (it == counterList_.end()) {
		lyxerr << "step: Counter does not exist: "
			   << to_utf8(cntr) << endl;
		return empty_docstring();
	}

	docstring const & guiname = it->second.guiName();
	if (guiname.empty())
		return cntr;
	return guiname;
}


docstring const & Counters::latexName(docstring const & cntr) const
{
	CounterList::const_iterator it = counterList_.find(cntr);
	if (it == counterList_.end()) {
		lyxerr << "step: Counter does not exist: "
			   << to_utf8(cntr) << endl;
		return empty_docstring();
	}

	docstring const & latexname = it->second.latexName();
	if (latexname.empty())
		return cntr;
	return latexname;
}


void Counters::reset()
{
	appendix_ = false;
	subfloat_ = false;
	current_float_.erase();
	for (auto & ctr : counterList_)
		ctr.second.reset();
	counter_stack_.clear();
	counter_stack_.push_back(from_ascii(""));
	layout_stack_.clear();
	layout_stack_.push_back(nullptr);
}


void Counters::reset(docstring const & match)
{
	LASSERT(!match.empty(), return);

	for (auto & ctr : counterList_) {
		if (ctr.first.find(match) != string::npos)
			ctr.second.reset();
	}
}


bool Counters::copy(docstring const & cnt, docstring const & newcnt)
{
	auto const it =	counterList_.find(cnt);
	if (it == counterList_.end())
		return false;
	counterList_[newcnt] = it->second;
	return true;
}


bool Counters::remove(docstring const & cnt)
{
	bool retval = counterList_.erase(cnt);
	if (!retval)
		return false;
	for (auto & ctr : counterList_) {
		if (ctr.second.checkAndRemoveParent(cnt))
			LYXERR(Debug::TCLASS, "Removed parent counter `" +
					to_utf8(cnt) + "' from counter: " + to_utf8(ctr.first));
	}
	return retval;
}


docstring Counters::labelItem(docstring const & ctr,
			      docstring const & numbertype) const
{
	CounterList::const_iterator const cit = counterList_.find(ctr);
	if (cit == counterList_.end()) {
		lyxerr << "Counter "
		       << to_utf8(ctr)
		       << " does not exist." << endl;
		return docstring();
	}

	int val = cit->second.value();

	if (numbertype == "hebrew")
		return docstring(1, hebrewCounter(val));

	if (numbertype == "alph")
		return docstring(1, loweralphaCounter(val));

	if (numbertype == "Alph")
		return docstring(1, alphaCounter(val));

	if (numbertype == "roman")
		return lowerromanCounter(val);

	if (numbertype == "Roman")
		return romanCounter(val);

	if (numbertype == "fnsymbol")
		return fnsymbolCounter(val);

	if (numbertype == "superarabic")
		return superarabicCounter(val);

	return convert<docstring>(val);
}


docstring Counters::theCounter(docstring const & counter,
			       string const & lang) const
{
	CounterList::const_iterator it = counterList_.find(counter);
	if (it == counterList_.end())
		return from_ascii("#");
	Counter const & ctr = it->second;
	Counter::StringMap & sm = ctr.flatLabelStrings(appendix());
	Counter::StringMap::iterator smit = sm.find(lang);
	if (smit != sm.end())
		return counterLabel(smit->second, lang);

	vector<docstring> callers;
	docstring const & fls = flattenLabelString(counter, appendix(),
						   lang, callers);
	sm[lang] = fls;
	return counterLabel(fls, lang);
}


docstring Counters::flattenLabelString(docstring const & counter,
				       bool in_appendix,
				       string const & lang,
				       vector<docstring> & callers) const
{
	if (find(callers.begin(), callers.end(), counter) != callers.end()) {
		// recursion detected
		lyxerr << "Warning: Recursion in label for counter `"
		       << counter << "' detected"
		       << endl;
		return from_ascii("??");
	}

	CounterList::const_iterator it = counterList_.find(counter);
	if (it == counterList_.end())
		return from_ascii("#");
	Counter const & c = it->second;

	docstring ls = translateIfPossible(c.labelString(in_appendix), lang);

	callers.push_back(counter);
	if (ls.empty()) {
		if (!c.parent().empty())
			ls = flattenLabelString(c.parent(), in_appendix, lang, callers)
				+ from_ascii(".");
		callers.pop_back();
		return ls + from_ascii("\\arabic{") + counter + "}";
	}

	while (true) {
		//lyxerr << "ls=" << to_utf8(ls) << endl;
		size_t const i = ls.find(from_ascii("\\the"), 0);
		if (i == docstring::npos)
			break;
		size_t const j = i + 4;
		size_t k = j;
		while (k < ls.size() && lowercase(ls[k]) >= 'a'
		       && lowercase(ls[k]) <= 'z')
			++k;
		docstring const newc = ls.substr(j, k - j);
		docstring const repl = flattenLabelString(newc, in_appendix,
							  lang, callers);
		ls.replace(i, k - j + 4, repl);
	}
	callers.pop_back();

	return ls;
}


docstring Counters::counterLabel(docstring const & format,
				 string const & lang) const
{
	docstring label = format;

	// FIXME: Using regexps would be better, but wide regexps are utf16 on windows.
	docstring const the = from_ascii("\\the");
	while (true) {
		//lyxerr << "label=" << label << endl;
		size_t const i = label.find(the, 0);
		if (i == docstring::npos)
			break;
		size_t const j = i + 4;
		size_t k = j;
		while (k < label.size() && lowercase(label[k]) >= 'a'
		       && lowercase(label[k]) <= 'z')
			++k;
		docstring const newc(label, j, k - j);
		label.replace(i, k - i, theCounter(newc, lang));
	}
	while (true) {
		//lyxerr << "label=" << label << endl;

		size_t const i = label.find('\\', 0);
		if (i == docstring::npos)
			break;
		size_t const j = label.find('{', i + 1);
		if (j == docstring::npos)
			break;
		size_t const k = label.find('}', j + 1);
		if (k == docstring::npos)
			break;
		docstring const numbertype(label, i + 1, j - i - 1);
		docstring const counter(label, j + 1, k - j - 1);
		label.replace(i, k + 1 - i, labelItem(counter, numbertype));
	}
	//lyxerr << "DONE! label=" << label << endl;
	return label;
}


namespace {

docstring getFormattedLabel(docstring const & in, bool const lc, bool const pl)
{
	if (contains(in, from_ascii("|"))) {
		docstring res;
		docstring l;
		docstring v = rsplit(in, l, ' ');
		vector<docstring> labels = getVectorFromString(l, from_ascii("|"));
		res = pl ? labels.back() : labels.front();
		if (lc)
			res = lowercase(res);
		return res + " " + v;
	}
	return in;
}

} // namespace anon


docstring Counters::formattedCounter(docstring const & name,
				     docstring const & prex,
				     string const & lang,
				     bool const lc,
				     bool const pl) const
{
	CounterList::const_iterator it = counterList_.find(name);
	if (it == counterList_.end())
		return from_ascii("#");
	Counter const & ctr = it->second;

	docstring const value = theCounter(name, lang);
	docstring const format =
		counterLabel(translateIfPossible(getFormattedLabel(ctr.refFormat(prex), lc, pl), lang),
			     lang);
	if (format.empty())
		return value;
	return subst(format, from_ascii("##"), value);
}


docstring Counters::prettyCounter(docstring const & name,
				  string const & lang,
				  bool const lc,
				  bool const pl) const
{
	CounterList::const_iterator it = counterList_.find(name);
	if (it == counterList_.end())
		return from_ascii("#");
	Counter const & ctr = it->second;

	docstring const value = theCounter(name, lang);

	docstring const & format = translateIfPossible(
				counterLabel(getFormattedLabel(ctr.prettyFormat(), lc, pl), lang),
				lang);
	if (format.empty())
		return value;
	return subst(format, from_ascii("##"), value);
}


docstring Counters::currentCounter() const
{
	LBUFERR(!counter_stack_.empty());
	return counter_stack_.back();
}


docstring Counters::currentParentCounter() const
{
	docstring const cc = currentCounter();
	CounterList::const_iterator const cit = counterList_.find(cc);
	if (cit == counterList_.end()) {
		lyxerr << "value: Counter does not exist: "
		       << to_utf8(cc) << endl;
		return docstring();
	}
	Counter const & cnt = cit->second;
	return cnt.parent();
}


void Counters::setActiveLayout(Layout const & lay)
{
	LASSERT(!layout_stack_.empty(), return);
	Layout const * const lastlay = layout_stack_.back();
	// we want to check whether the layout has changed and, if so,
	// whether we are coming out of or going into an environment.
	if (!lastlay) {
		layout_stack_.pop_back();
		layout_stack_.push_back(&lay);
		if (lay.isEnvironment())
			beginEnvironment();
	} else if (lastlay->name() != lay.name()) {
		layout_stack_.pop_back();
		layout_stack_.push_back(&lay);
		if (lastlay->isEnvironment()) {
			// we are coming out of an environment
			// LYXERR0("Out: " << lastlay->name());
			endEnvironment();
		}
		if (lay.isEnvironment()) {
			// we are going into a new environment
			// LYXERR0("In: " << lay.name());
			beginEnvironment();
		}
	}
}


void Counters::beginEnvironment()
{
	counter_stack_.push_back(counter_stack_.back());
}


void Counters::endEnvironment()
{
	LASSERT(!counter_stack_.empty(), return);
	counter_stack_.pop_back();
}


vector<docstring> Counters::listOfCounters() const {
	vector<docstring> ret;
	for(auto const & k : counterList_)
		ret.emplace_back(k.first);
	return ret;
}


} // namespace lyx
