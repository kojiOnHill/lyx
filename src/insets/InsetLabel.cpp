/**
 * \file InsetLabel.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetLabel.h"

#include "InsetRef.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "Language.h"
#include "LyX.h"
#include "ParIterator.h"
#include "xml.h"
#include "texstream.h"
#include "Text.h"
#include "TextClass.h"
#include "TocBackend.h"

#include "mathed/InsetMathRef.h"

#include "frontends/alert.h"

#include "support/convert.h"
#include "support/gettext.h"
#include "support/lstrings.h"

using namespace std;
using namespace lyx::support;

namespace lyx {


InsetLabel::InsetLabel(Buffer * buf, InsetCommandParams const & p)
	: InsetCommand(buf, p)
{}


void InsetLabel::initView()
{
	// This seems to be used only for inset creation.
	// Therefore we do not update refs here, since this would
	// erroneously change refs from existing duplicate labels
	// (#8141).
	updateLabel(getParam("name"));
}


void InsetLabel::uniqueLabel(docstring & label) const
{
	docstring const new_label = label;
	int i = 1;
	bool ambiguous = false;
	while (buffer().activeLabel(label)) {
		label = new_label + '-' + convert<docstring>(i);
		++i;
		ambiguous = true;
	}
	if (ambiguous) {
		// Warn the user that the label has been changed to something else.
		frontend::Alert::warning(_("Label names must be unique!"),
			bformat(_("The label %1$s already exists,\n"
			"it will be changed to %2$s."), new_label, label));
	}
}


void InsetLabel::updateLabel(docstring const & new_label, bool const active)
{
	docstring label = new_label;
	if (active)
		uniqueLabel(label);
	setParam("name", label);
}


void InsetLabel::updateLabelAndRefs(docstring const & new_label,
		Cursor * cursor)
{
	docstring const old_label = getParam("name");
	docstring label = new_label;
	uniqueLabel(label);
	if (label == old_label)
		return;

	// This handles undo groups automagically
	UndoGroupHelper ugh(&buffer());
	if (cursor)
		cursor->recordUndo();
	bool const changes = buffer().masterParams().track_changes;
	if (changes) {
		// With change tracking, we insert a new label and
		// delete the old one
		InsetCommandParams p(LABEL_CODE, "label");
		p["name"] = label;
		string const data = InsetCommand::params2string(p);
		lyx::dispatch(FuncRequest(LFUN_INSET_INSERT, data));
		lyx::dispatch(FuncRequest(LFUN_CHAR_DELETE_FORWARD));
	} else
		setParam("name", label);
	updateReferences(old_label, label, changes);
}


void InsetLabel::updateReferences(docstring const & old_label,
		docstring const & new_label, bool const changes)
{
	UndoGroupHelper ugh(nullptr);
	if (changes) {
		// With change tracking, we insert a new ref and
		// delete the old one
		lyx::dispatch(FuncRequest(LFUN_MASTER_BUFFER_FORALL,
					  "inset-forall Ref inset-modify ref changetarget "
					  + old_label + " " + new_label));
	} else {
		for (auto const & p: buffer().references(old_label)) {
			ugh.resetBuffer(p.second.buffer());
			CursorData(p.second).recordUndo();
			if (p.first->lyxCode() == MATH_REF_CODE) {
				InsetMathRef * mi = p.first->asInsetMath()->asRefInset();
				mi->changeTarget(new_label);
			} else {
				InsetCommand * ref = p.first->asInsetCommand();
				ref->setParam("reference", new_label);
			}
		}
	}
}


ParamInfo const & InsetLabel::findInfo(string const & /* cmdName */)
{
	static ParamInfo param_info_;
	if (param_info_.empty())
		param_info_.add("name", ParamInfo::LATEX_REQUIRED,
				ParamInfo::HANDLING_ESCAPE);
	return param_info_;
}


docstring InsetLabel::screenLabel() const
{
	return screen_label_;
}


void InsetLabel::updateBuffer(ParIterator const & it, UpdateType, bool const /*deleted*/)
{
	docstring const & label = getParam("name");

	// Check if this one is active (i.e., neither deleted with change-tracking
	// nor in an inset that does not produce output, such as notes or inactive branches)
	Paragraph const & para = it.paragraph();
	bool active = !para.isDeleted(it.pos()) && para.inInset().producesOutput();
	// If not, check whether we are in a deleted/non-outputting inset
	if (active) {
		for (size_type sl = 0 ; sl < it.depth() ; ++sl) {
			Paragraph const & outer_par = it[sl].paragraph();
			if (outer_par.isDeleted(it[sl].pos())
			    || !outer_par.inInset().producesOutput()) {
				active = false;
				break;
			}
		}
	}

	if (buffer().activeLabel(label) && active) {
		// Problem: We already have an active InsetLabel with the same name!
		screen_label_ = _("DUPLICATE: ") + label;
		return;
	}
	buffer().setInsetLabel(label, this, active);
	screen_label_ = label;

	// save info on the active counter
	Counters & cnts =
		buffer().masterBuffer()->params().documentClass().counters();
	active_counter_ = cnts.currentCounter();
	Language const * lang = it->getParLanguage(buffer().params());
	if (lang && !active_counter_.empty()) {
		if (active_counter_ != from_ascii("equation")) {
			counter_value_ = cnts.theCounter(active_counter_, lang->code());
			pretty_counter_ = cnts.prettyCounter(active_counter_, lang->code());
			docstring prex;
			split(label, prex, ':');
			if (prex == label) {
				// No prefix found
				formatted_counter_ = pretty_counter_;
			} else
				formatted_counter_ = cnts.formattedCounter(active_counter_, prex, lang->code());
		} else {
			// For equations, the counter value and pretty counter
			// value will be set by the parent InsetMathHull.
			counter_value_ = from_ascii("#");
			pretty_counter_ = from_ascii("");
			formatted_counter_ = from_ascii("");
		}
	} else {
		counter_value_ = from_ascii("#");
		pretty_counter_ = from_ascii("#");
		formatted_counter_ = from_ascii("#");
	}
}


void InsetLabel::addToToc(DocIterator const & cpit, bool output_active,
			  UpdateType, TocBackend & backend) const
{
	docstring const & label = getParam("name");

	// inactive labels get a cross mark
	if (buffer().insetLabel(label, true) != this)
		output_active = false;

	// We put both active and inactive labels to the outliner
	shared_ptr<Toc> toc = backend.toc("label");
	TocItem toc_item = TocItem(cpit, 0, screen_label_, output_active);
	toc_item.prettyStr(formatted_counter_);
	toc->push_back(toc_item);
	// The refs get assigned only to the active label. If no active one exists,
	// assign the (BROKEN) refs to the first inactive one.
	if (buffer().insetLabel(label, true) == this || !buffer().activeLabel(label)) {
		for (auto const & p : buffer().references(label)) {
			DocIterator const ref_pit(p.second);
			if (p.first->lyxCode() == MATH_REF_CODE)
				toc->push_back(TocItem(ref_pit, 1,
						p.first->asInsetMath()->asRefInset()->screenLabel(),
						output_active));
			else
				toc->push_back(TocItem(ref_pit, 1,
						static_cast<InsetRef *>(p.first)->getTOCString(),
						static_cast<InsetRef *>(p.first)->outputActive()));
		}
	}
}


bool InsetLabel::getStatus(Cursor & cur, FuncRequest const & cmd,
			   FuncStatus & status) const
{
	bool enabled;
	switch (cmd.action()) {
	case LFUN_LABEL_INSERT_AS_REFERENCE:
	case LFUN_LABEL_COPY_AS_REFERENCE:
		enabled = true;
		break;
	case LFUN_INSET_MODIFY:
		if (cmd.getArg(0) == "changetype") {
			// this is handled by InsetCommand,
			// but not by InsetLabel.
			enabled = false;
			break;
		}
		// no "changetype":
		// fall through
	default:
		return InsetCommand::getStatus(cur, cmd, status);
	}

	status.setEnabled(enabled);
	return true;
}


void InsetLabel::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY: {
		// the only other option here is "changetype", and we
		// do not have different types.
		if (cmd.getArg(0) != "label") {
			cur.undispatched();
			return;
		}
		InsetCommandParams p(LABEL_CODE);
		// FIXME UNICODE
		InsetCommand::string2params(to_utf8(cmd.argument()), p);
		if (p.getCmdName().empty()) {
			cur.noScreenUpdate();
			break;
		}
		if (p["name"] != params()["name"]) {
			// undo is handled in updateLabelAndRefs
			updateLabelAndRefs(p["name"], &cur);
		}
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_LABEL_COPY_AS_REFERENCE: {
		InsetCommandParams p(REF_CODE, "ref");
		p["reference"] = getParam("name");
		cap::clearSelection();
		cap::copyInset(cur, new InsetRef(buffer_, p), getParam("name"));
		break;
	}

	case LFUN_LABEL_INSERT_AS_REFERENCE: {
		InsetCommandParams p(REF_CODE, "ref");
		p["reference"] = getParam("name");
		string const data = InsetCommand::params2string(p);
		lyx::dispatch(FuncRequest(LFUN_INSET_INSERT, data));
		break;
	}

	default:
		InsetCommand::doDispatch(cur, cmd);
		break;
	}
}


void InsetLabel::latex(otexstream & os, OutputParams const & runparams_in) const
{
	OutputParams runparams = runparams_in;
	docstring command = getCommand(runparams);
	docstring const label = getParam("name");
	if (buffer().params().output_changes
	    && buffer().activeLabel(label)
	    && buffer().insetLabel(label, true) != this) {
		// this is a deleted label and we have a non-deleted with the same id
		// rename it for output to prevent wrong references
		docstring newlabel = label;
		int i = 1;
		while (buffer().insetLabel(newlabel)) {
			newlabel = label + "-DELETED-" + convert<docstring>(i);
			++i;
		}
		command = subst(command, label, newlabel);
	}
	// In macros with moving arguments, such as \section,
	// we store the label and output it after the macro (#2154)
	if (runparams_in.postpone_fragile_stuff)
		runparams_in.post_macro += command;
	else {
		// protect label in \thanks notes (#9404)
		if (runparams.moving_arg
		    && runparams.intitle
		    && runparams.inFootnote)
			os << "\\protect";
		os << command;
	}
}


int InsetLabel::plaintext(odocstringstream & os,
        OutputParams const &, size_t) const
{
	docstring const str = getParam("name");
	os << '<' << str << '>';
	return 2 + str.size();
}


void InsetLabel::docbook(XMLStream & xs, OutputParams const & runparams) const
{
	// Output an anchor only if it has not been processed before.
	docstring id = getParam("name");
	docstring cleaned_id = xml::cleanID(id);
	if (runparams.docbook_anchors_to_ignore.find(id) == runparams.docbook_anchors_to_ignore.end() &&
	        runparams.docbook_anchors_to_ignore.find(cleaned_id) == runparams.docbook_anchors_to_ignore.end()) {
		docstring attr = from_utf8("xml:id=\"") + cleaned_id + from_utf8("\"");
		xs << xml::CompTag("anchor", to_utf8(attr));
	}
}


docstring InsetLabel::xhtml(XMLStream & xs, OutputParams const &) const
{
	// Print the label as an HTML anchor, so that an external link can point to this equation.
	// (URL: FILE.html#EQ-ID.)
	// FIXME XHTML
	// Unfortunately, the name attribute has been deprecated, so we have to use
	// id here to get the document to validate as XHTML 1.1. This will cause a
	// problem with some browsers, though, I'm sure. (Guess which!) So we will
	// have to figure out what to do about this later.
	docstring const attr = "id=\"" + xml::cleanAttr(getParam("name")) + '"';
	xs << xml::CompTag("a", to_utf8(attr));
	return docstring();
}


} // namespace lyx
