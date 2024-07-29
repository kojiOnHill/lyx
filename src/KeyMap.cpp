/**
 * \file KeyMap.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author Jean-Marc Lasgouttes
 * \author John Levon
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "KeyMap.h"

#include "KeySequence.h"
#include "LyXAction.h"
#include "Lexer.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/lstrings.h"
#include "support/TempFile.h"

#include "frontends/alert.h"

#include <fstream>
#include <sstream>
#include <utility>

using namespace std;
using namespace lyx::support;

namespace lyx {

string const KeyMap::printKeySym(KeySymbol const & key, KeyModifier mod)
{
	string buf;

	string const s = key.getSymbolName();

	if (mod & ControlModifier)
		buf += "C-";
#if defined(USE_MACOSX_PACKAGING) || defined(USE_META_KEYBINDING)
	if (mod & MetaModifier)
		buf += "M-";
	if (mod & AltModifier)
		buf += "A-";
#else
	if (mod & AltModifier)
		buf += "M-";
#endif
	if (mod & ShiftModifier)
		buf += "S-";

	buf += s;
	return buf;
}


size_t KeyMap::bind(string const & seq, FuncRequest const & func)
{
	LYXERR(Debug::KBMAP, "BIND: Sequence `" << seq << "' Action `"
	       << func.action() << '\'');

	KeySequence k(nullptr, nullptr);

	string::size_type const res = k.parse(seq);
	if (res == string::npos) {
		bind(&k, func);
	} else {
		LYXERR(Debug::KBMAP, "Parse error at position " << res
				     << " in key sequence '" << seq << "'.");
	}

	return res == string::npos ? 0 : res;
}


size_t KeyMap::unbind(string const & seq, FuncRequest const & func)
{
	KeySequence k(nullptr, nullptr);

	string::size_type const res = k.parse(seq);
	if (res == string::npos)
		unbind(&k, func);
	else
		LYXERR(Debug::KBMAP, "Parse error at position " << res
				     << " in key sequence '" << seq << "'.");
	return res == string::npos ? 0 : res;
}


void KeyMap::bind(KeySequence * seq, FuncRequest const & func, unsigned int r)
{
	KeySymbol code = seq->sequence[r];
	if (!code.isOK())
		return;

	KeyModifier const mod1 = seq->modifiers[r].first;
	KeyModifier const mod2 = seq->modifiers[r].second;

	// check if key is already there
	// FIXME perf: Profiling shows that this is responsible of 99% of the
	// cost of GuiPrefs::applyView()
	Table::iterator end = table.end();
	for (Table::iterator it = table.begin(); it != end; ++it) {
		if (code == it->code
		    && mod1 == it->mod.first
		    && mod2 == it->mod.second) {
			// overwrite binding
			if (r + 1 == seq->length()) {
				LYXERR(Debug::KBMAP, "Warning: New binding for '"
					<< to_utf8(seq->print(KeySequence::Portable))
					<< "' is overriding old binding...");
				if (it->prefixes)
					it->prefixes.reset();
				it->func = func;
				it->func.setOrigin(FuncRequest::KEYBOARD);
				return;
			} else if (!it->prefixes) {
				lyxerr << "Error: New binding for '"
				       << to_utf8(seq->print(KeySequence::Portable))
				       << "' is overriding old binding..."
				       << endl;
				return;
			} else {
				it->prefixes->bind(seq, func, r + 1);
				return;
			}
		}
	}

	Table::iterator newone = table.insert(table.end(), Key());
	newone->code = code;
	newone->mod = seq->modifiers[r];
	if (r + 1 == seq->length()) {
		newone->func = func;
		newone->func.setOrigin(FuncRequest::KEYBOARD);
		newone->prefixes.reset();
	} else {
		newone->prefixes.reset(new KeyMap);
		newone->prefixes->bind(seq, func, r + 1);
	}
}


void KeyMap::unbind(KeySequence * seq, FuncRequest const & func, unsigned int r)
{
	KeySymbol code = seq->sequence[r];
	if (!code.isOK())
		return;

	KeyModifier const mod1 = seq->modifiers[r].first;
	KeyModifier const mod2 = seq->modifiers[r].second;

	// check if key is already there
	vector <Table::iterator> removes;
	Table::iterator end = table.end();
	for (Table::iterator it = table.begin(); it != end; ++it) {
		if (code == it->code
		    && mod1 == it->mod.first
		    && mod2 == it->mod.second) {
			// remove
			if (r + 1 == seq->length()) {
				if (it->func == func || func == FuncRequest::unknown) {
					removes.push_back(it);
					if (it->prefixes)
						it->prefixes.reset();
				}
			} else if (it->prefixes) {
				it->prefixes->unbind(seq, func, r + 1);
				if (it->prefixes->empty())
					removes.push_back(it);
				return;
			}
		}
	}

	for (unsigned i = removes.size(); i > 0; --i)
		table.erase(removes[i-1]);
}


FuncRequest KeyMap::getBinding(KeySequence const & seq, unsigned int r)
{
	KeySymbol code = seq.sequence[r];
	if (!code.isOK())
		return FuncRequest::unknown;

	KeyModifier const mod1 = seq.modifiers[r].first;
	KeyModifier const mod2 = seq.modifiers[r].second;

	// check if key is already there
	Table::iterator end = table.end();
	for (Table::iterator it = table.begin(); it != end; ++it) {
		if (code == it->code
		    && mod1 == it->mod.first
		    && mod2 == it->mod.second) {
			if (r + 1 == seq.length())
				return (it->prefixes) ? FuncRequest::prefix : it->func;
			else if (it->prefixes)
				return it->prefixes->getBinding(seq, r + 1);
		}
	}
	return FuncRequest::unknown;
}


void KeyMap::clear()
{
	table.clear();
}


bool KeyMap::read(string const & bind_file, KeyMap * unbind_map, BindReadType rt, bool i18n)
{
	FileName bf = i18n ? i18nLibFileSearch("bind", bind_file, "bind")
			   : libFileSearch("bind", bind_file, "bind");
	if (bf.empty()) {
		if (rt == MissingOK)
			return true;

		lyxerr << "Could not find bind file: " << bind_file;
		if (rt == Default) {
			frontend::Alert::warning(_("Could not find bind file"),
				bformat(_("Unable to find the bind file\n%1$s.\n"
						"Please check your installation."), from_utf8(bind_file)));
			return false;
		}

		static string const defaultBindfile = "cua";
		if (bind_file == defaultBindfile) {
			frontend::Alert::warning(_("Could not find `cua.bind' file"),
				_("Unable to find the default bind file `cua.bind'.\n"
				   "Please check your installation."));
			return false;
		}

		// Try it with the default file.
		frontend::Alert::warning(_("Could not find bind file"),
			bformat(_("Unable to find the bind file\n%1$s.\n"
			          "Falling back to default."), from_utf8(bind_file)));
		return read(defaultBindfile, unbind_map);
	}
	return read(bf, unbind_map);
}


bool KeyMap::read(FileName const & bind_file, KeyMap * unbind_map)
{
	ReturnValues retval = readWithoutConv(bind_file, unbind_map);
	if (retval != FormatMismatch)
		return retval == ReadOK;

	LYXERR(Debug::FILES, "Converting bind file to " << LFUN_FORMAT);
	TempFile tmp("convert_bind");
	FileName const tempfile = tmp.name();
	bool const success = prefs2prefs(bind_file, tempfile, true);
	if (!success) {
		LYXERR0 ("Unable to convert " << bind_file <<
			" to format " << LFUN_FORMAT);
		return false;
	}
	retval = readWithoutConv(tempfile, unbind_map);
	return retval == ReadOK;
}


KeyMap::ReturnValues KeyMap::readWithoutConv(FileName const & bind_file, KeyMap * unbind_map)
{
	enum {
		BN_BIND,
		BN_BINDFILE,
		BN_FORMAT,
		BN_UNBIND
	};

	LexerKeyword bindTags[] = {
		{ "\\bind",      BN_BIND },
		{ "\\bind_file", BN_BINDFILE },
		{ "\\unbind",    BN_UNBIND },
		{ "format",      BN_FORMAT }
	};

	Lexer lexrc(bindTags);
	if (lyxerr.debugging(Debug::PARSER))
		lexrc.printTable(lyxerr);

	lexrc.setFile(bind_file);
	if (!lexrc.isOK()) {
		LYXERR0("KeyMap::read: cannot open bind file:" << bind_file.absFileName());
		return FileError;
	}

	LYXERR(Debug::KBMAP, "Reading bind file:" << bind_file.absFileName());

	// format of pre-2.0 bind files, before this tag was introduced.
	int format = 0;
	bool error = false;
	while (lexrc.isOK()) {
		switch (lexrc.lex()) {

		case Lexer::LEX_UNDEF:
			lexrc.printError("Unknown tag `$$Token'");
			error = true;
			continue;

		case Lexer::LEX_FEOF:
			continue;

		case BN_FORMAT:
			if (lexrc.next())
				format = lexrc.getInteger();
			break;

		case BN_BIND: {
			if (!lexrc.next()) {
				lexrc.printError("BN_BIND: Missing key sequence");
				error = true;
				break;
			}
			string seq = lexrc.getString();

			if (!lexrc.next(true)) {
				lexrc.printError("BN_BIND: missing command");
				error = true;
				break;
			}
			string cmd = lexrc.getString();

			FuncRequest func = lyxaction.lookupFunc(cmd);
			if (func == FuncRequest::unknown) {
				lexrc.printError("BN_BIND: Unknown LyX function `$$Token'");
				error = true;
				break;
			}

			bind(seq, func);
			break;
		}

		case BN_UNBIND: {
			if (!lexrc.next()) {
				lexrc.printError("BN_UNBIND: Missing key sequence");
				error = true;
				break;
			}
			string seq = lexrc.getString();

			if (!lexrc.next(true)) {
				lexrc.printError("BN_UNBIND: missing command");
				error = true;
				break;
			}
			string cmd = lexrc.getString();
			FuncRequest func;
			if (cmd == "*")
				func = FuncRequest::unknown;
			else {
				func = lyxaction.lookupFunc(cmd);
				if (func == FuncRequest::unknown) {
					lexrc.printError("BN_UNBIND: Unknown LyX function `$$Token'");
					error = true;
					break;
				}
			}

			if (unbind_map)
				unbind_map->bind(seq, func);
			else
				unbind(seq, func);
			break;
		}

		case BN_BINDFILE:
			if (!lexrc.next()) {
				lexrc.printError("BN_BINDFILE: Missing file name");
				error = true;
				break;
			}
			string tmp = lexrc.getString();
			if (prefixIs(tmp, "../")) {
				tmp = split(tmp, '/');
				// look in top dir
				error |= !read(tmp, unbind_map, Default, false);
			} else
				// i18n search
				error |= !read(tmp, unbind_map);
			break;
		}

		// This is triggered the first time through the loop unless
		// we hit a format tag.
		if (format != LFUN_FORMAT)
			return FormatMismatch;
	}

	if (error) {
		LYXERR0("KeyMap::read: error while reading bind file:" << bind_file.absFileName());
		return ReadError;
	}
	return ReadOK;
}


void KeyMap::write(string const & bind_file, bool append, bool unbind) const
{
	ofstream os(bind_file.c_str(),
		append ? (ios::app | ios::out) : ios::out);

	if (!append)
		os << "## This file is automatically generated by lyx\n"
		   << "## All modifications will be lost\n\n"
		   << "Format " << LFUN_FORMAT << "\n\n";

	string tag = unbind ? "\\unbind" : "\\bind";
	for (auto const & bnd : listBindings(false)) {
		FuncCode const action = bnd.request.action();
		string const arg = to_utf8(bnd.request.argument());

		string cmd;
		if (unbind && bnd.request == FuncRequest::unknown)
			cmd = "*";
		else
			cmd = lyxaction.getActionName(action) + (arg.empty() ? string() : " " + arg);
		os << tag << " \""
		   << to_utf8(bnd.sequence.print(KeySequence::BindFile))
		   << "\" " << Lexer::quoteString(cmd)
		   << "\n";
	}
	os << "\n";
	os.close();
}


FuncRequest const & KeyMap::lookup(KeySymbol const &key,
		  KeyModifier mod, KeySequence * seq) const
{
	if (table.empty()) {
		seq->reset();
		return FuncRequest::unknown;
	}

	Table::const_iterator end = table.end();
	for (Table::const_iterator cit = table.begin(); cit != end; ++cit) {
		KeyModifier mask = cit->mod.second;
		KeyModifier check = static_cast<KeyModifier>(mod & ~mask);

		if (cit->code == key && cit->mod.first == check) {
			// match found
			if (cit->prefixes) {
				// this is a prefix key - set new map
				seq->curmap = cit->prefixes.get();
				return FuncRequest::prefix;
			} else {
				// final key - reset map
				seq->reset();
				return cit->func;
			}
		}
	}

	// error - key not found:
	seq->reset();

	return FuncRequest::unknown;
}


docstring const KeyMap::print(bool forgui) const
{
	docstring buf;
	Table::const_iterator end = table.end();
	for (Table::const_iterator cit = table.begin(); cit != end; ++cit) {
		buf += cit->code.print(cit->mod.first, forgui);
		buf += ' ';
	}
	return buf;
}


docstring KeyMap::printBindings(FuncRequest const & func,
				KeySequence::outputFormat format,
				bool const untranslated) const
{
	Bindings bindings = findBindings(func);
	if (bindings.empty())
		return docstring();

	odocstringstream res;
	Bindings::const_iterator cit = bindings.begin();
	Bindings::const_iterator cit_end = bindings.end();
	// print the first item
	res << cit->print(format, untranslated);
	// more than one shortcuts?
	for (++cit; cit != cit_end; ++cit)
		res << ", " << cit->print(format, untranslated);
	return res.str();
}


KeyMap::Bindings KeyMap::findBindings(FuncRequest const & func) const
{
	return findBindings(func, KeySequence(nullptr, nullptr));
}


KeyMap::Bindings KeyMap::findBindings(FuncRequest const & func,
			KeySequence const & prefix) const
{
	Bindings res;
	if (table.empty())
		return res;

	Table::const_iterator end = table.end();
	for (Table::const_iterator cit = table.begin(); cit != end; ++cit) {
		if (cit->prefixes) {
			KeySequence seq = prefix;
			seq.addkey(cit->code, cit->mod.first);
			Bindings res2 = cit->prefixes->findBindings(func, seq);
			res.insert(res.end(), res2.begin(), res2.end());
		} else if (cit->func == func) {
			KeySequence seq = prefix;
			seq.addkey(cit->code, cit->mod.first);
			res.push_back(seq);
		}
	}

	return res;
}


KeyMap::BindingList KeyMap::listBindings(bool unbound, KeyMap::ItemType tag) const
{
	BindingList list;
	listBindings(list, KeySequence(nullptr, nullptr), tag);
	if (unbound) {
		for (auto const & name_code : lyxaction) {
			FuncCode action = name_code.second;
			bool has_action = false;
			BindingList::const_iterator bit = list.begin();
			BindingList::const_iterator const ben = list.end();
			for (; bit != ben; ++bit)
				if (bit->request.action() == action) {
					has_action = true;
					break;
				}
			if (!has_action)
				list.push_back(Binding(FuncRequest(action), KeySequence(nullptr, nullptr), tag));
		}
	}
	return list;
}


void KeyMap::listBindings(BindingList & list,
	KeySequence const & prefix, KeyMap::ItemType tag) const
{
	Table::const_iterator it = table.begin();
	Table::const_iterator it_end = table.end();
	for (; it != it_end; ++it) {
		// a LFUN_COMMAND_PREFIX
		if (it->prefixes) {
			KeySequence seq = prefix;
			seq.addkey(it->code, it->mod.first, it->mod.second);
			it->prefixes->listBindings(list, seq, tag);
		} else {
			KeySequence seq = prefix;
			seq.addkey(it->code, it->mod.first, it->mod.second);
			list.push_back(Binding(it->func, seq, tag));
		}
	}
}


} // namespace lyx
