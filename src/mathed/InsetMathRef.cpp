/**
 * \file InsetMathRef.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathRef.h"

#include "BufferParams.h"
#include "BufferView.h"
#include "Buffer.h"
#include "Cursor.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "LaTeXFeatures.h"
#include "LyX.h"
#include "MathData.h"
#include "MathFactory.h"
#include "MathStream.h"
#include "MathSupport.h"
#include "ParIterator.h"
#include "xml.h"

#include "insets/InsetCommand.h"
#include "insets/InsetRef.h"

#include "support/debug.h"
#include "support/gettext.h"
#include "support/lstrings.h"

#include <ostream>

using namespace std;

namespace lyx {

InsetMathRef::InsetMathRef(Buffer * buf)
	: InsetMathCommand(buf, from_ascii("ref"), false)
{}


InsetMathRef::InsetMathRef(Buffer * buf, docstring const & data)
	: InsetMathCommand(buf, data, false)
{}


Inset * InsetMathRef::clone() const
{
	return new InsetMathRef(*this);
}


void InsetMathRef::infoize(odocstream & os) const
{
	os << "Ref: " << cell(0);
}


void InsetMathRef::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	// Ctrl + click: go to label
	if (cmd.action() == LFUN_MOUSE_RELEASE && cmd.modifier() == ControlModifier) {
		LYXERR0("trying to goto ref '" << to_utf8(asString(cell(0))) << "'");
		//FIXME: use DispatchResult argument
		lyx::dispatch(FuncRequest(LFUN_LABEL_GOTO, asString(cell(0))));
		return;
	}

	switch (cmd.action()) {
	case LFUN_INSET_MODIFY: {
		string const arg0 = cmd.getArg(0);
		string const arg1 = cmd.getArg(1);
		if (arg0 == "ref") {
			if (arg1 == "changetarget") {
				string const oldtarget = cmd.getArg(2);
				string const newtarget = cmd.getArg(3);
				if (!oldtarget.empty() && !newtarget.empty()
				    && asString(cell(0)) == from_utf8(oldtarget))
					changeTarget(from_utf8(newtarget));
				cur.forceBufferUpdate();
				break;
			}
			MathData md(buffer_);
			if (createInsetMath_fromDialogStr(cmd.argument(), md)) {
				cur.recordUndo();
				Buffer & buf = buffer();
				*this = *md[0].nucleus()->asRefInset();
				setBuffer(buf);
				break;
			}
		} else if (arg0 == "changetype") {
			docstring const data = from_ascii(createDialogStr(arg1));
			MathData md(buffer_);
			if (createInsetMath_fromDialogStr(data, md)) {
				cur.recordUndo();
				Buffer & buf = buffer();
				*this = *md[0].nucleus()->asRefInset();
				setBuffer(buf);
				break;
			}
		}
		cur.undispatched();
		break;
	}

	case LFUN_INSET_DIALOG_UPDATE: {
		string const data = createDialogStr();
		cur.bv().updateDialog("ref", data);
		break;
	}

	case LFUN_INSET_SETTINGS: {
		string const data = createDialogStr();
		cur.bv().showDialog("ref", data, this);
		cur.dispatched();
		break;
	}

	case LFUN_MOUSE_RELEASE:
		if (cur.selection()) {
			cur.undispatched();
			break;
		}
		if (cmd.button() == mouse_button::button1) {
			string const data = createDialogStr();
			cur.bv().showDialog("ref", data, this);
			break;
		}
		cur.undispatched();
		break;

	case LFUN_MOUSE_PRESS: {
		bool do_selection = cmd.button() == mouse_button::button1
			&& cmd.modifier() == ShiftModifier;
		// For some reason the cursor points inside the first cell, which is not
		// active.
		cur.leaveInset(*this);
		cur.bv().mouseSetCursor(cur, do_selection);
		break;
	}

	case LFUN_MOUSE_DOUBLE:
	case LFUN_MOUSE_TRIPLE:
		// eat other mouse commands
		break;

	default:
		InsetMathCommand::doDispatch(cur, cmd);
		break;
	}
}


bool InsetMathRef::getStatus(Cursor & cur, FuncRequest const & cmd,
			 FuncStatus & status) const
{
	switch (cmd.action()) {
	// we handle these
	case LFUN_INSET_MODIFY:
		if (cmd.getArg(0) == "changetype")
			status.setOnOff(from_ascii(cmd.getArg(1)) == commandname());
		if (cmd.getArg(1) == "cpageref")
			status.setEnabled(buffer_
				&& (buffer().params().xref_package == "cleveref"
				    || buffer().params().xref_package == "zref"));
		else
			status.setEnabled(true);
		return true;
	case LFUN_INSET_DIALOG_UPDATE:
	case LFUN_INSET_SETTINGS:
	case LFUN_MOUSE_RELEASE:
	case LFUN_MOUSE_PRESS:
	case LFUN_MOUSE_DOUBLE:
	case LFUN_MOUSE_TRIPLE:
		status.setEnabled(true);
		return true;
	default:
		return InsetMathCommand::getStatus(cur, cmd, status);
	}
}


docstring const InsetMathRef::screenLabel() const
{
	docstring str;
	for (int i = 0; !types[i].latex_name.empty(); ++i) {
		if (commandname() == types[i].latex_name) {
			str = _(to_utf8(types[i].short_gui_name));
			break;
		}
	}
	str += asString(cell(0));

	//if (/* !isLatex && */ !cell(0).empty()) {
	//	str += "||";
	//	str += asString(cell(1));
	//}
	return str;
}


void InsetMathRef::validate(LaTeXFeatures & features) const
{
	// This really should not happen here but does.
	if (!buffer_) {
		LYXERR0("Unassigned buffer_ in InsetMathRef::write!");
		LYXERR0("LaTeX output may be wrong!");
	}
	bool const use_refstyle =
		buffer_ && buffer().params().xref_package == "refstyle";

	if (commandname() == "vref" || commandname() == "vpageref") {
		if (buffer_ && buffer().masterParams().xref_package == "zref")
			features.require("zref-vario");
		else
			features.require("varioref");
	} else if (commandname() == "formatted") {
		if (use_refstyle)
			features.require("refstyle");
		else if (buffer_ && buffer().masterParams().xref_package == "cleveref")
			features.require("cleveref");
		else if (buffer_ && buffer().masterParams().xref_package == "zref")
			features.require("zref-clever");
		else
			features.require("prettyref");
	} else if (commandname() == "cpageref") {
		if (buffer_ && buffer().masterParams().xref_package == "cleveref")
			features.require("cleveref");
		else if (buffer_ && buffer().masterParams().xref_package == "zref")
			features.require("zref-clever");
	}
	// if eqref is used with refstyle, we do our own output
	else if (commandname() == "eqref" && use_refstyle)
		features.require("amsmath");
	else if (commandname() == "nameref")
		features.require("nameref");
}


void InsetMathRef::docbook(XMLStream & xs, OutputParams const &) const
{
	if (cell(1).empty()) {
		docstring attr = from_utf8("linkend=\"") + xml::cleanID(asString(cell(0))) + from_utf8("\"");
		xs << xml::CompTag("xref", to_utf8(attr));
	} else {
		// Link with linkend, as is it within the document (not outside, in which case xlink:href is better suited).
		docstring attr = from_utf8("linkend=\"") + xml::cleanID(asString(cell(0))) + from_utf8("\"");
		xs << xml::StartTag("link", to_utf8(attr))
		   << asString(cell(1))
		   << xml::EndTag("link");
	}
}


void InsetMathRef::updateBuffer(ParIterator const & it, UpdateType /*utype*/, bool const /*deleted*/)
{
	if (!buffer_) {
		LYXERR0("InsetMathRef::updateBuffer: no buffer_!");
		return;
	}
	// register this inset into the buffer reference cache.
	buffer().addReference(getTarget(), this, it);
}


string const InsetMathRef::createDialogStr(string const & type) const
{
	InsetCommandParams icp(REF_CODE, (type.empty()
			?  to_ascii(commandname()) : type));
	icp["reference"] = asString(cell(0));
	if (!cell(1).empty())
		icp["name"] = asString(cell(1));
	return InsetCommand::params2string(icp);
}


docstring const InsetMathRef::getTarget() const
{
	return asString(cell(0));
}


void InsetMathRef::changeTarget(docstring const & target)
{
	InsetCommandParams icp(REF_CODE, to_ascii(commandname()));
	icp["reference"] = target;
	if (!cell(1).empty())
		icp["name"] = asString(cell(1));
	MathData md(buffer_);
	Buffer & buf = buffer();
	if (createInsetMath_fromDialogStr(
	    from_utf8(InsetCommand::params2string(icp)), md)) {
		*this = *md[0].nucleus()->asRefInset();
		// FIXME audit setBuffer calls
		setBuffer(buf);
	}
}


void InsetMathRef::writeMath(TeXMathStream & os) const
{
	docstring const & cmd = commandname();
	// This should not happen, but of course it does
	if (!buffer_) {
		LYXERR0("Unassigned buffer_ in InsetMathRef::write!");
		LYXERR0("LaTeX output may be wrong!");
	}
	// are we writing to the LyX file?
	if (!os.latex()) {
		// if so, then this is easy
		InsetMathCommand::writeMath(os);
		return;
	}
	bool const use_refstyle =
		buffer_ && buffer().params().xref_package == "refstyle";
	bool special_case =  cmd == "formatted" ||
			cmd == "vref" ||
			cmd == "vpageref" ||
			cmd == "cpageref" ||
			cmd == "labelonly" ||
			(cmd == "eqref" && use_refstyle);
	// we need to translate 'formatted' to prettyref or refstyle-type
	// commands and just output the label with labelonly
	// most of this is borrowed from InsetRef and should be kept in 
	// sync with that.
	ModeSpecifier specifier(os, currentMode(), lockedMode(), asciiOnly());
	MathEnsurer ensurer(os, false);
	if (!special_case) {
		os << from_ascii("\\") << cmd << "{" << cell(0) << from_ascii("}");
	}
	else if (cmd == "vref" || cmd == "vpageref") {
		os << from_ascii("\\");
		if (buffer_ && buffer().params().xref_package == "zref")
			os << "z";
		os << cmd << "{" << cell(0) << from_ascii("}");
	}
	else if (cmd == "cpageref") {
		os << from_ascii("\\");
		if (buffer_ && buffer().params().xref_package == "zref")
			os << "z";
		if (buffer_ && (buffer().params().xref_package == "cleveref"
		    || buffer().params().xref_package == "zref"))
			os << cmd;
		else
			os << "pageref";
		os << "{" << cell(0) << from_ascii("}");
	}
	else if (use_refstyle && cmd == "eqref") {
		// we advertise this as printing "(n)", so we'll do that, at least
		// for refstyle, since refstlye's own \eqref prints, by default,
		// "equation n". if one wants \eqref, one can get it by using a
		// formatted label in this case.
		os << '(' << from_ascii("\\ref{") << cell(0) << from_ascii("})");
	}
	else if (cmd == "formatted") {
		if (buffer_ && support::prefixIs(buffer().params().xref_package, "prettyref"))
			os << "\\prettyref{" << cell(0) << "}";
		else {
			odocstringstream ods;
			// get the label we are referencing
			for (auto const & d : cell(0)) {
				ods << d;
			}
			docstring const ref = ods.str();

			/*
			At the moment, the 'plural', 'nolink' and 'caps' options will
			not work here. Also ranges. The reason is that we handle these as
			'internal' LyX argumemts, but those are not handled by
			InsetCommandParams::getCommand, which is what is used
			in createInsetMath_fromDialogStr to interpret the data
			coming from the dialog.
			If this is fixed, then what follows will get the info
			we need.
			Fixing it, I think, would mean sub-classing
			InsetCommandParams to InsetRefParams, and the overriding
			getCommand.
			*******************************************************
			// reset
			ods.str(docstring());
			ods.clear();
			// get the options from the optional argument
			for (auto const & d : cell(1))
				ods << d;
			docstring const options = ods.str();
			bool const caps   = support::contains(options, 'C');
			bool const plural = support::contains(options, 's');
			*/
			vector<docstring> label;
			docstring prefix;
			docstring const fcmd =
				InsetRef::getFormattedCmd(ref, label, prefix, buffer().params().xref_package);
			os << fcmd;
			//if (plural)
			//	os << "[s]";
			bool first = true;
			os << "{";
			for (auto const & l : label) {
				if (!first) {
//					if (useRange())
//						os << "}{";
//					else
						os << ",";
				}
				os << l;
				first = false;
			}
			os << "}";
		}
	}
	else if (cmd == "labelonly") {
		// noprefix does not work here, for reasons given above.
		os << cell(0);
	}
}


InsetMathRef::ref_type_info InsetMathRef::types[] = {
	{ from_ascii("ref"),       from_ascii(N_("Standard[[mathref]]")),   from_ascii(N_("Ref: "))},
	{ from_ascii("eqref"),     from_ascii(N_("Equation")),              from_ascii(N_("EqRef: "))},
	{ from_ascii("pageref"),   from_ascii(N_("Page Number")),           from_ascii(N_("Page: "))},
	{ from_ascii("cpageref"),  from_ascii(N_("Prefixed Page Number")),  from_ascii(N_("PrefPage: "))},
	{ from_ascii("vpageref"),  from_ascii(N_("Textual Page Number")),   from_ascii(N_("TextPage: "))},
	{ from_ascii("vref"),      from_ascii(N_("Standard+Textual Page")), from_ascii(N_("Ref+Text: "))},
	{ from_ascii("formatted"), from_ascii(N_("PrettyRef")),             from_ascii(N_("FormatRef: "))},
	{ from_ascii("nameref"),   from_ascii(N_("Reference to Name")),     from_ascii(N_("NameRef: "))},
	{ from_ascii("labelonly"), from_ascii(N_("Label Only")),             from_ascii(N_("Label Only: "))},
	{ from_ascii(""), from_ascii(""), from_ascii("") }
};


} // namespace lyx
