/**
 * \file LaTeXFonts.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "LaTeXFonts.h"

#include "LaTeXFeatures.h"

#include "frontends/alert.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/docstream.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/Lexer.h"
#include "support/lstrings.h"


using namespace std;
using namespace lyx::support;


namespace lyx {

LaTeXFonts latexfonts;


LaTeXFont LaTeXFont::altFont(docstring const & name) const
{
	return theLaTeXFonts().getAltFont(name);
}


bool LaTeXFont::available(bool ot1, bool nomath) const
{
	if (nomath && !nomathfont_.empty())
		return altFont(nomathfont_).available(ot1, nomath);
	else if (ot1 && !ot1font_.empty())
		return (ot1font_ == "none") ?
			true : altFont(ot1font_).available(ot1, nomath);
	else if (required_.empty() && package_.empty())
		return true;
	else if (!required_.empty()
		&& LaTeXFeatures::isAvailable(to_ascii(required_)))
		return true;
	else if (required_.empty() && !package_.empty()
		&& LaTeXFeatures::isAvailable(to_ascii(package_)))
		return true;
	else if (!altfonts_.empty()) {
		for (auto const & name : altfonts_) {
			if (altFont(name).available(ot1, nomath))
				return true;
		}
	}
	return false;
}


bool LaTeXFont::providesNoMath(bool ot1, bool complete) const
{
	docstring const usedfont = getUsedFont(ot1, complete, false, false);

	if (usedfont.empty())
		return false;
	else if (usedfont != name_)
		return altFont(usedfont).providesNoMath(ot1, complete);

	return (!nomathfont_.empty() && available(ot1, true));
}


bool LaTeXFont::providesOSF(bool ot1, bool complete, bool nomath) const
{
	docstring const usedfont = getUsedFont(ot1, complete, nomath, false);

	if (usedfont.empty())
		return false;
	else if (usedfont != name_)
		return altFont(usedfont).providesOSF(ot1, complete, nomath);
	else if (!osffont_.empty())
		return altFont(osffont_).available(ot1, nomath);
	else if (!available(ot1, nomath))
		return false;

	return (!osfoption_.empty() || !osfscoption_.empty());
}


bool LaTeXFont::providesSC(bool ot1, bool complete, bool nomath) const
{
	docstring const usedfont = getUsedFont(ot1, complete, nomath, false);

	if (usedfont.empty())
		return false;
	else if (usedfont != name_)
		return altFont(usedfont).providesSC(ot1, complete, nomath);
	else if (!available(ot1, nomath))
		return false;

	return (!scoption_.empty() || !osfscoption_.empty());
}


bool LaTeXFont::hasMonolithicExpertSet(bool ot1, bool complete, bool nomath) const
{
	docstring const usedfont = getUsedFont(ot1, complete, nomath, false);

	if (usedfont.empty())
		return false;
	else if (usedfont != name_)
		return altFont(usedfont).hasMonolithicExpertSet(ot1, complete, nomath);
	return (!osfoption_.empty() && !scoption_.empty() && osfoption_ == scoption_)
		|| (osfoption_.empty() && scoption_.empty() && !osfscoption_.empty());
}


bool LaTeXFont::providesScale(bool ot1, bool complete, bool nomath) const
{
	docstring const usedfont = getUsedFont(ot1, complete, nomath, false);

	if (usedfont.empty())
		return false;
	else if (usedfont != name_)
		return altFont(usedfont).providesScale(ot1, complete, nomath);
	else if (!available(ot1, nomath))
		return false;
	return (!scaleoption_.empty() || !scalecmd_.empty());
}


bool LaTeXFont::providesMoreOptions(bool ot1, bool complete, bool nomath) const
{
	docstring const usedfont = getUsedFont(ot1, complete, nomath, false);

	if (usedfont.empty())
		return false;
	else if (usedfont != name_)
		return altFont(usedfont).providesMoreOptions(ot1, complete, nomath);
	else if (!available(ot1, nomath))
		return false;

	return (moreopts_);
}

bool LaTeXFont::provides(std::string const & name, bool ot1, bool complete, bool nomath) const
{
	docstring const usedfont = getUsedFont(ot1, complete, nomath, false);

	if (usedfont.empty())
		return false;
	else if (usedfont != name_)
		return altFont(usedfont).provides(name, ot1, complete, nomath);
	else if (provides_.empty())
		return false;

	for (auto const & provide : provides_) {
		if (provide == name)
			return true;
	}
	return false;
}


docstring const LaTeXFont::getUsedFont(bool ot1, bool complete, bool nomath, bool osf) const
{
	if (osf && osfFontOnly())
		return osffont_;
	else if (nomath && !nomathfont_.empty() && available(ot1, true))
		return nomathfont_;
	else if (ot1 && !ot1font_.empty())
		return (ot1font_ == "none") ? docstring() : ot1font_;
	else if (family_ == "rm" && complete && !completefont_.empty()
	         && altFont(completefont_).available(ot1, nomath))
			return completefont_;
	else if (switchdefault_) {
		if (required_.empty()
		    || (!required_.empty()
		        && LaTeXFeatures::isAvailable(to_ascii(required_))))
			return name_;
	}
	else if (!required_.empty()
		&& LaTeXFeatures::isAvailable(to_ascii(required_)))
			return name_;
	else if (!package_.empty()
		&& LaTeXFeatures::isAvailable(to_ascii(package_)))
			return name_;
	else if (!preamble_.empty() && package_.empty()
		 && required_.empty() && !switchdefault_
		 && altfonts_.empty()) {
			return name_;
	}
	// if we haven't somethin up to here, try fallback fonts
	if (!altfonts_.empty()) {
		for (auto const & name : altfonts_) {
			LaTeXFont altf = altFont(name);
			if (altf.available(ot1, nomath))
				return altf.getUsedFont(ot1, complete, nomath, osf);
		}
	}

	return docstring();
}


docstring const LaTeXFont::getUsedPackage(bool ot1, bool complete, bool nomath) const
{
	docstring const usedfont = getUsedFont(ot1, complete, nomath, false);
	if (usedfont.empty())
		return docstring();
	return theLaTeXFonts().getLaTeXFont(usedfont).package();
}


string const LaTeXFont::getAvailablePackage(bool dryrun) const
{
	if (package_.empty())
		return string();

	string const package = to_ascii(package_);
	if (!required_.empty() && LaTeXFeatures::isAvailable(to_ascii(required_)))
		return package;
	else if (LaTeXFeatures::isAvailable(package))
		return package;
	// Output unavailable packages in source preview
	else if (dryrun)
		return package;

	docstring const req = required_.empty() ? package_ : required_;
	frontend::Alert::warning(_("Font not available"),
			bformat(_("The LaTeX package `%1$s' needed for the font `%2$s'\n"
				  "is not available on your system. LyX will fall back to the default font."),
				req, guiname_), true);

	return string();
}


string const LaTeXFont::getPackageOptions(bool ot1, bool complete, bool sc, bool osf,
					  int scale, string const & extraopts, bool nomath) const
{
	ostringstream os;
	bool const needosfopt = (osf != osfdefault_);
	bool const has_osf = providesOSF(ot1, complete, nomath);
	bool const has_sc = providesSC(ot1, complete, nomath);
	bool const moreopts = providesMoreOptions(ot1, complete, nomath);

	if (!packageoptions_.empty())
		os << to_ascii(packageoptions_);

	if (sc && needosfopt && has_osf && has_sc) {
		if (!os.str().empty())
			os << ',';
		if (!osfscoption_.empty())
			os << to_ascii(osfscoption_);
		else
			os << to_ascii(osfoption_)
			   << ',' << to_ascii(scoption_);
	} else if (needosfopt && has_osf) {
		if (!os.str().empty())
			os << ',';
		os << to_ascii(osfoption_);
	} else if (sc && has_sc) {
		if (!os.str().empty())
			os << ',';
		os << to_ascii(scoption_);
	}

	if (scale != 100 && !scaleoption_.empty()
	    && providesScale(ot1, complete, nomath)) {
		if (!os.str().empty())
			os << ',';
		os << subst(to_ascii(scaleoption_), "$$val",
			    convert<std::string>(float(scale) / 100));
	}

	if (moreopts && !extraopts.empty()) {
		if (!os.str().empty())
			os << ',';
		os << extraopts;
	}
	return os.str();
}


string const LaTeXFont::getLaTeXCode(bool dryrun, bool ot1, bool complete, bool sc,
				     bool osf, bool nomath, string const & extraopts, int scale) const
{
	ostringstream os;

	docstring const usedfont = getUsedFont(ot1, complete, nomath, osf);
	if (usedfont.empty())
		return string();
	else if (usedfont != name_)
		return altFont(usedfont).getLaTeXCode(dryrun, ot1, complete, sc,
						      osf, nomath, extraopts, scale);

	if (switchdefault_) {
		if (family_.empty()) {
			LYXERR0("Error: Font `" << name_ << "' has no family defined!");
			return string();
		}
		if (available(ot1, nomath) || dryrun)
			os << "\\renewcommand{\\" << to_ascii(family_) << "default}{"
			   << to_ascii(name_) << "}\n";
		else
			frontend::Alert::warning(_("Font not available"),
					bformat(_("The LaTeX package `%1$s' needed for the font `%2$s'\n"
						  "is not available on your system. LyX will fall back to the default font."),
						required_, guiname_), true);
	} else {
		string const package =
			getAvailablePackage(dryrun);
		string const packageopts = getPackageOptions(ot1, complete, sc, osf, scale, extraopts, nomath);
		if (packageopts.empty() && !package.empty())
			os << "\\usepackage{" << package << "}\n";
		else if (!packageopts.empty() && !package.empty())
			os << "\\usepackage[" << packageopts << "]{" << package << "}\n";
	}
	if (osf && providesOSF(ot1, complete, nomath) && !osffont_.empty())
		os << altFont(osffont_).getLaTeXCode(dryrun, ot1, complete, sc, osf,
						     nomath, extraopts, scale);
	if (scale != 100 && !scalecmd_.empty()
	    && providesScale(ot1, complete, nomath)) {
		if (contains(scalecmd_, '@'))
			os << "\\makeatletter\n";
		os << subst(to_ascii(scalecmd_), "$$val",
			    convert<std::string>(float(scale) / 100)) << '\n';
		if (contains(scalecmd_, '@'))
			os << "\\makeatother\n";
	}

	if (!preamble_.empty())
		os << to_utf8(preamble_);

	return os.str();
}


bool LaTeXFont::hasFontenc(string const & name) const
{
	for (auto const & fe : fontenc_) {
		if (fe == name)
			return true;
	}
	return false;
}


bool LaTeXFont::readFont(Lexer & lex)
{
	enum LaTeXFontTags {
		LF_ALT_FONTS = 1,
		LF_COMPLETE_FONT,
		LF_END,
		LF_FAMILY,
		LF_FONTENC,
		LF_GUINAME,
		LF_NOMATHFONT,
		LF_OSFDEFAULT,
		LF_OSFFONT,
		LF_OSFFONTONLY,
		LF_OSFOPTION,
		LF_OSFSCOPTION,
		LF_OT1_FONT,
		LF_MOREOPTS,
		LF_PACKAGE,
		LF_PACKAGEOPTIONS,
		LF_PREAMBLE,
		LF_PROVIDES,
		LF_REQUIRES,
		LF_SCALECMD,
		LF_SCALEOPTION,
		LF_SCOPTION,
		LF_SWITCHDEFAULT
	};

	// Keep these sorted alphabetically!
	LexerKeyword latexFontTags[] = {
		{ "altfonts",             LF_ALT_FONTS },
		{ "completefont",         LF_COMPLETE_FONT },
		{ "endfont",              LF_END },
		{ "family",               LF_FAMILY },
		{ "fontencoding",         LF_FONTENC },
		{ "guiname",              LF_GUINAME },
		{ "moreoptions",          LF_MOREOPTS },
		{ "nomathfont",           LF_NOMATHFONT },
		{ "osfdefault",           LF_OSFDEFAULT },
		{ "osffont",              LF_OSFFONT },
		{ "osffontonly",          LF_OSFFONTONLY },
		{ "osfoption",            LF_OSFOPTION },
		{ "osfscoption",          LF_OSFSCOPTION },
		{ "ot1font",              LF_OT1_FONT },
		{ "package",              LF_PACKAGE },
		{ "packageoptions",       LF_PACKAGEOPTIONS },
		{ "preamble",             LF_PREAMBLE },
		{ "provides",             LF_PROVIDES },
		{ "requires",             LF_REQUIRES },
		{ "scalecommand",         LF_SCALECMD },
		{ "scaleoption",          LF_SCALEOPTION },
		{ "scoption",             LF_SCOPTION },
		{ "switchdefault",        LF_SWITCHDEFAULT }
	};

	bool error = false;
	bool finished = false;
	lex.pushTable(latexFontTags);
	// parse style section
	while (!finished && lex.isOK() && !error) {
		int le = lex.lex();
		// See comment in LyXRC.cpp.
		switch (le) {
		case Lexer::LEX_FEOF:
			continue;

		case Lexer::LEX_UNDEF: // parse error
			lex.printError("Unknown LaTeXFont tag `$$Token'");
			error = true;
			continue;

		default:
			break;
		}
		switch (static_cast<LaTeXFontTags>(le)) {
		case LF_END: // end of structure
			finished = true;
			break;
		case LF_ALT_FONTS: {
			lex.eatLine();
			docstring altp = lex.getDocString();
			altfonts_ = getVectorFromString(altp);
			break;
		}
		case LF_COMPLETE_FONT:
			lex >> completefont_;
			break;
		case LF_FAMILY:
			lex >> family_;
			break;
		case LF_GUINAME:
			lex >> guiname_;
			break;
		case LF_FONTENC: {
			lex.eatLine();
			string fe = lex.getString();
			fontenc_ = getVectorFromString(fe);
			break;
		}
		case LF_NOMATHFONT:
			lex >> nomathfont_;
			break;
		case LF_OSFOPTION:
			lex >> osfoption_;
			break;
		case LF_OSFFONT:
			lex >> osffont_;
			break;
		case LF_OSFDEFAULT:
			lex >> osfdefault_;
			break;
		case LF_OSFFONTONLY:
			lex >> osffontonly_;
			break;
		case LF_OSFSCOPTION:
			lex >> osfscoption_;
			break;
		case LF_OT1_FONT:
			lex >> ot1font_;
			break;
		case LF_PACKAGE:
			lex >> package_;
			break;
		case LF_PACKAGEOPTIONS:
			lex >> packageoptions_;
			break;
		case LF_PREAMBLE:
			preamble_ = lex.getLongString(from_ascii("EndPreamble"));
			break;
		case LF_PROVIDES: {
			lex.eatLine();
			string features = lex.getString();
			provides_ = getVectorFromString(features);
			break;
		}
		case LF_REQUIRES:
			lex >> required_;
			break;
		case LF_SCALECMD:
			lex >> scalecmd_;
			break;
		case LF_SCALEOPTION:
			lex >> scaleoption_;
			break;
		case LF_SCOPTION:
			lex >> scoption_;
			break;
		case LF_MOREOPTS:
			lex >> moreopts_;
			break;
		case LF_SWITCHDEFAULT:
			lex >> switchdefault_;
			break;
		}
	}
	if (!finished) {
		lex.printError("No End tag found for LaTeXFont tag `$$Token'");
		return false;
	}
	lex.popTable();
	return finished && !error;
}


bool LaTeXFont::read(Lexer & lex)
{
	switchdefault_ = false;
	osfdefault_ = false;
	moreopts_ = false;
	osffontonly_ = false;

	if (!lex.next()) {
		lex.printError("No name given for LaTeX font: `$$Token'.");
		return false;
	}

	name_ = lex.getDocString();
	LYXERR(Debug::INFO, "Reading LaTeX font " << name_);
	if (!readFont(lex)) {
		LYXERR0("Error parsing LaTeX font `" << name_ << '\'');
		return false;
	}

	if (fontenc_.empty())
		fontenc_.push_back("T1");

	return true;
}


void LaTeXFonts::readLaTeXFonts()
{
	// Read latexfonts file
	FileName filename = libFileSearch(string(), "latexfonts");
	if (filename.empty()) {
		LYXERR0("Error: latexfonts file not found!");
		return;
	}
	Lexer lex;
	lex.setFile(filename);
	lex.setContext("LaTeXFeatures::readLaTeXFonts");
	while (lex.isOK()) {
		int le = lex.lex();
		switch (le) {
		case Lexer::LEX_FEOF:
			continue;

		default:
			break;
		}
		string const type = lex.getString();
		if (type != "Font" && type != "AltFont") {
			lex.printError("Unknown LaTeXFont tag `$$Token'");
			continue;
		}
		LaTeXFont f;
		f.read(lex);
		if (!lex)
			break;

		if (type == "AltFont")
			texaltfontmap_[f.name()] = f;
		else
			texfontmap_[f.name()] = f;
	}
}


LaTeXFonts::TexFontMap LaTeXFonts::getLaTeXFonts()
{
	if (texfontmap_.empty())
		readLaTeXFonts();
	return texfontmap_;
}


LaTeXFont LaTeXFonts::getLaTeXFont(docstring const & name)
{
	if (name == "default" || name == "auto")
		return LaTeXFont();
	if (texfontmap_.empty())
		readLaTeXFonts();
	if (texfontmap_.find(name) == texfontmap_.end()) {
		LYXERR0("LaTeXFonts::getLaTeXFont: font '" << name << "' not found!");
		return LaTeXFont();
	}
	return texfontmap_[name];
}


LaTeXFont LaTeXFonts::getAltFont(docstring const & name)
{
	if (name == "default" || name == "auto")
		return LaTeXFont();
	if (texaltfontmap_.empty())
		readLaTeXFonts();
	if (texaltfontmap_.find(name) == texaltfontmap_.end()) {
		LYXERR0("LaTeXFonts::getAltFont: alternative font '" << name << "' not found!");
		return LaTeXFont();
	}
	return texaltfontmap_[name];
}


} // namespace lyx
