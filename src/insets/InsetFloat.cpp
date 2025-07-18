/**
 * \file InsetFloat.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jürgen Vigna
 * \author Lars Gullik Bjønnes
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetFloat.h"

#include "InsetBox.h"
#include "InsetCaption.h"
#include "InsetLabel.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Counters.h"
#include "Cursor.h"
#include "DispatchResult.h"
#include "Floating.h"
#include "FloatList.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "LaTeXFeatures.h"
#include "xml.h"
#include "ParIterator.h"
#include "TexRow.h"
#include "texstream.h"
#include "TextClass.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/gettext.h"
#include "support/Lexer.h"
#include "support/lstrings.h"

using namespace std;
using namespace lyx::support;


namespace lyx {

// With this inset it will be possible to support the latex package
// float.sty, and I am sure that with this and some additional support
// classes we can support similar functionality in other formats
// (read DocBook).
// By using float.sty we will have the same handling for all floats, both
// for those already in existence (table and figure) and all user created
// ones¹. So suddenly we give the users the possibility of creating new
// kinds of floats on the fly. (and with a uniform look)
//
// API to float.sty:
//   \newfloat{type}{placement}{ext}[within]
//     type      - The "type" of the new class of floats, like program or
//                 algorithm. After the appropriate \newfloat, commands
//                 such as \begin{program} or \end{algorithm*} will be
//                 available.
//     placement - The default placement for the given class of floats.
//                 They are like in standard LaTeX: t, b, p and h for top,
//                 bottom, page, and here, respectively. On top of that
//                 there is a new type, H, which does not really correspond
//                 to a float, since it means: put it "here" and nowhere else.
//                 Note, however that the H specifier is special and, because
//                 of implementation details cannot be used in the second
//                 argument of \newfloat.
//     ext       - The file name extension of an auxiliary file for the list
//                 of figures (or whatever). LaTeX writes the captions to
//                 this file.
//     within    - This (optional) argument determines whether floats of this
//                 class will be numbered within some sectional unit of the
//                 document. For example, if within is equal to chapter, the
//                 floats will be numbered within chapters.
//   \floatstyle{style}
//     style -  plain, boxed, ruled
//   \floatname{float}{floatname}
//     float     -
//     floatname -
//   \floatplacement{float}{placement}
//     float     -
//     placement -
//   \restylefloat{float}
//     float -
//   \listof{type}{title}
//     title -

// ¹ the algorithm float is defined using the float.sty package. Like this
//   \floatstyle{ruled}
//   \newfloat{algorithm}{htbp}{loa}[<sect>]
//   \floatname{algorithm}{Algorithm}
//
// The intention is that floats should be definable from two places:
//          - layout files
//          - the "gui" (i.e. by the user)
//
// From layout files.
// This should only be done for floats defined in a documentclass and that
// does not need any additional packages. The two most known floats in this
// category is "table" and "figure". Floats defined in layout files are only
// stored in lyx files if the user modifies them.
//
// By the user.
// There should be a gui dialog (and also a collection of lyxfuncs) where
// the user can modify existing floats and/or create new ones.
//
// The individual floats will also have some settable
// variables: wide and placement.
//
// Lgb

//FIXME: why do we set in stone the type here?
InsetFloat::InsetFloat(Buffer * buf, string const & params_str)
	: InsetCaptionable(buf)
{
	string2params(params_str, params_);
	setCaptionType(params_.type);
}


// Enforce equality of float type and caption type.
void InsetFloat::setCaptionType(std::string const & type)
{
	InsetCaptionable::setCaptionType(type);
	params_.type = captionType();
	// check if the float type exists
	if (buffer().params().documentClass().floats().typeExist(params_.type))
		setNewLabel();
	else
		setLabel(bformat(_("ERROR: Unknown float type: %1$s"), from_utf8(params_.type)));
}


docstring InsetFloat::layoutName() const
{
	return "Float:" + from_utf8(params_.type);
}


docstring InsetFloat::toolTip(BufferView const & bv, int x, int y) const
{
	if (isOpen(bv))
		return InsetCaptionable::toolTip(bv, x, y);

	OutputParams rp(&buffer().params().encoding());
	return getCaptionText(rp);
}


void InsetFloat::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY: {
		if (!buffer().params().documentClass().floats().typeExist(cmd.getArg(0))) {
			// not for us: pass further.
			cur.undispatched();
			break;
		}
		InsetFloatParams params;
		string2params(to_utf8(cmd.argument()), params);
		cur.recordUndoInset(this);

		// placement, wide and sideways are not used for subfloats
		if (!params_.subfloat) {
			params_.placement = params.placement;
			params_.wide      = params.wide;
			params_.sideways  = params.sideways;
		}
		params_.alignment  = params.alignment;
		setNewLabel();
		if (params_.type != params.type)
			setCaptionType(params.type);
		// what we really want here is a TOC update, but that means
		// a full buffer update
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_INSET_DIALOG_UPDATE: {
		cur.bv().updateDialog("float", params2string(params()));
		break;
	}

	default:
		InsetCaptionable::doDispatch(cur, cmd);
		break;
	}
}


bool InsetFloat::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY:
		if (!buffer().params().documentClass().floats().typeExist(cmd.getArg(0)))
			return Inset::getStatus(cur, cmd, flag);
	// fall through
	case LFUN_INSET_DIALOG_UPDATE:
		flag.setEnabled(true);
		return true;

	case LFUN_INSET_SETTINGS:
		if (InsetCaptionable::getStatus(cur, cmd, flag)) {
			flag.setEnabled(flag.enabled() && !params_.subfloat);
			return true;
		} else
			return false;

	case LFUN_NEWLINE_INSERT:
		if (params_.subfloat) {
			flag.setEnabled(false);
			return true;
		}
		// no subfloat:
		// fall through

	default:
		return InsetCaptionable::getStatus(cur, cmd, flag);
	}
}


bool InsetFloat::hasSubCaptions(ParIterator const & it) const
{
	return (it.innerInsetOfType(FLOAT_CODE) || it.innerInsetOfType(WRAP_CODE));
}


string InsetFloat::getAlignment() const
{
	string alignment;
	string const buf_alignment = buffer().params().float_alignment;
	if (params_.alignment == "document"
	    && !buf_alignment.empty()) {
		alignment = buf_alignment;
	} else if (!params_.alignment.empty()
		   && params_.alignment != "class"
		   && params_.alignment != "document") {
		alignment = params_.alignment;
	}
	return alignment;
}


CtObject InsetFloat::getCtObject(OutputParams const &) const
{
	return CtObject::OmitObject;
}


LyXAlignment InsetFloat::contentAlignment() const
{
	LyXAlignment align = LYX_ALIGN_NONE;
	string alignment = getAlignment();
	if (alignment == "left")
		align = LYX_ALIGN_LEFT;
	else if (alignment == "center")
		align = LYX_ALIGN_CENTER;
	else if (alignment == "right")
		align = LYX_ALIGN_RIGHT;

	return align;
}


void InsetFloatParams::write(ostream & os) const
{
	if (type.empty()) {
		// Better this than creating a parse error. This in fact happens in the
		// parameters dialog via InsetFloatParams::params2string.
		os << "senseless" << '\n';
	} else
		os << type << '\n';

	if (!placement.empty())
		os << "placement " << placement << "\n";
	if (!alignment.empty())
		os << "alignment " << alignment << "\n";

	if (wide)
		os << "wide true\n";
	else
		os << "wide false\n";

	if (sideways)
		os << "sideways true\n";
	else
		os << "sideways false\n";
}


void InsetFloatParams::read(Lexer & lex)
{
	lex.setContext("InsetFloatParams::read");
	lex >> type;
	if (lex.checkFor("placement"))
		lex >> placement;
	if (lex.checkFor("alignment"))
		lex >> alignment;
	lex >> "wide" >> wide;
	lex >> "sideways" >> sideways;
}


void InsetFloat::write(ostream & os) const
{
	os << "Float ";
	params_.write(os);
	InsetCaptionable::write(os);
}


void InsetFloat::read(Lexer & lex)
{
	params_.read(lex);
	InsetCaptionable::read(lex);
	setCaptionType(params_.type);
}


void InsetFloat::validate(LaTeXFeatures & features) const
{
	if (support::contains(params_.placement, 'H'))
		features.require("float");

	if (params_.sideways)
		features.require("rotfloat");

	if (features.inFloat())
		features.require("subfig");

	if (features.inDeletedInset()) {
		features.require("tikz");
		features.require("ct-tikz-object-sout");
	}

	features.useFloat(params_.type, features.inFloat());
	features.inFloat(true);
	InsetCaptionable::validate(features);
	features.inFloat(false);
}


docstring InsetFloat::xhtml(XMLStream & xs, OutputParams const & rp) const
{
	FloatList const & floats = buffer().params().documentClass().floats();
	Floating const & ftype = floats.getType(params_.type);
	string const & htmltype = ftype.htmlTag();
	string const & attr = ftype.htmlAttrib();

	odocstringstream ods;
	XMLStream newxs(ods);
	newxs << xml::StartTag(htmltype, attr);
	InsetText::XHTMLOptions const opts =
		InsetText::WriteLabel | InsetText::WriteInnerTag;
	InsetText::insetAsXHTML(newxs, rp, opts);
	newxs << xml::EndTag(htmltype);

	if (rp.inFloat == OutputParams::NONFLOAT) {
		// In this case, this float needs to be deferred, but we'll put it
		// before anything the text itself deferred.
		return ods.str();
	} else {
		// Things will already have been escaped, so we do not
		// want to escape them again.
		xs << XMLStream::ESCAPE_NONE << ods.str();
		return docstring();
	}
}


void InsetFloat::latex(otexstream & os, OutputParams const & runparams_in) const
{
	if (runparams_in.inFloat != OutputParams::NONFLOAT) {
		if (!paragraphs().empty() && !runparams_in.nice)
			// improve TexRow precision in non-nice mode
			os << safebreakln;

		if (runparams_in.moving_arg)
			os << "\\protect";
		os << "\\subfloat";

		OutputParams rp = runparams_in;
		rp.moving_arg = true;
		rp.inFloat = OutputParams::SUBFLOAT;
		os << getCaption(rp);
		os << '{';
		// The main argument is the contents of the float. This is not a moving argument.
		rp.moving_arg = false;
		InsetText::latex(os, rp);
		os << "}";

		return;
	}
	OutputParams runparams(runparams_in);
	runparams.inFloat = OutputParams::MAINFLOAT;

	FloatList const & floats = buffer().params().documentClass().floats();
	string tmptype = params_.type;
	if (params_.sideways && floats.allowsSideways(params_.type))
		tmptype = "sideways" + params_.type;
	if (params_.wide && floats.allowsWide(params_.type)
		&& (!params_.sideways ||
		     params_.type == "figure" ||
		     params_.type == "table"))
		tmptype += "*";
	// Figure out the float placement to use.
	// From lowest to highest:
	// - float default placement
	// - document wide default placement
	// - specific float placement
	string tmpplacement;
	string const buf_placement = buffer().params().float_placement;
	string const def_placement = floats.defaultPlacement(params_.type);
	if (params_.placement == "document"
	    && !buf_placement.empty()
	    && buf_placement != def_placement) {
		tmpplacement = buf_placement;
	} else if (!params_.placement.empty()
		   && params_.placement != "document"
		   && params_.placement != def_placement) {
		tmpplacement = params_.placement;
	}

	// Check if placement is allowed by this float
	string const allowed_placement =
		floats.allowedPlacement(params_.type);
	string placement;
	string::const_iterator lit = tmpplacement.begin();
	string::const_iterator end = tmpplacement.end();
	for (; lit != end; ++lit) {
		if (contains(allowed_placement, *lit))
			placement += *lit;
	}

	// Force \begin{<floatname>} to appear in a new line.
	os << breakln << "\\begin{" << from_ascii(tmptype) << '}';
	if (runparams.lastid != -1)
		os.texrow().start(runparams.lastid, runparams.lastpos);
	// We only output placement if different from the def_placement.
	// sidewaysfloats always use their own page,
	// therefore don't output the p option that is always set
	if (!placement.empty()
	    && (!params_.sideways || from_ascii(placement) != "p"))
		os << '[' << from_ascii(placement) << ']';
	os << '\n';

	if (runparams.inDeletedInset) {
		// This has to be done manually since we need it inside the float
		CtObject ctobject = runparams.ctObject;
		runparams.ctObject = CtObject::DisplayObject;
		Changes::latexMarkChange(os, buffer().params(), Change(Change::UNCHANGED),
					 Change(Change::DELETED), runparams);
		runparams.ctObject = ctobject;
	}

	string alignment = getAlignment();
	if (alignment == "left")
		os << "\\raggedright" << breakln;
	else if (alignment == "center")
		os << "\\centering" << breakln;
	else if (alignment == "right")
		os << "\\raggedleft" << breakln;

	InsetText::latex(os, runparams);

	if (runparams.inDeletedInset)
		os << "}";

	// Force \end{<floatname>} to appear in a new line.
	os << breakln << "\\end{" << from_ascii(tmptype) << "}\n";
}


int InsetFloat::plaintext(odocstringstream & os, OutputParams const & runparams, size_t max_length) const
{
	os << '[' << buffer().B_("float") << ' '
		<< floatName(params_.type) << ":\n";
	InsetText::plaintext(os, runparams, max_length);
	os << "\n]";

	return PLAINTEXT_NEWLINE + 1; // one char on a separate line
}


std::vector<const InsetCollapsible *> findSubfiguresInParagraph(const Paragraph &par)
{

	// Don't make the hypothesis that all subfigures are in the same paragraph.
	// Similarly, there may be several subfigures in the same paragraph (most likely case, based on the documentation).
	// Any box is considered as a subfigure, even though the most likely case is \minipage.
	// Boxes are not required to make subfigures. The common root between InsetBox and InsetFLoat is InsetCollapsible.
	std::vector<const InsetCollapsible *> subfigures;
	for (pos_type pos = 0; pos < par.size(); ++pos) {
		const Inset *inset = par.getInset(pos);
		if (!inset)
			continue;
		if (const auto box = dynamic_cast<const InsetBox *>(inset))
			subfigures.push_back(box);
		else if (const auto fl = dynamic_cast<const InsetFloat *>(inset))
			subfigures.push_back(fl);
	}
	return subfigures;
}


/// Takes an unstructured subfigure container (typically, an InsetBox) and find the elements within:
/// actual content (image or table), maybe a caption, maybe a label.
std::tuple<InsetCode, const Inset *, const InsetCaption *, const InsetLabel *> docbookParseHopelessSubfigure(const InsetText * subfigure)
{
	InsetCode type = NO_CODE;
	const Inset * content = nullptr;
	const InsetCaption * caption = nullptr;
	const InsetLabel * label = nullptr;

	for (const auto & it : subfigure->paragraphs()) {
		for (pos_type posIn = 0; posIn < it.size(); ++posIn) {
			const Inset * inset = it.getInset(posIn);
			if (inset) {
				switch (inset->lyxCode()) {
					case GRAPHICS_CODE:
					case TABULAR_CODE:
						if (!content) {
							content = inset;
							type = inset->lyxCode();
						}
						break;
					case CAPTION_CODE:
						if (!caption) {
							caption = dynamic_cast<const InsetCaption *>(inset);

							// A label often hides in a caption. Make a simplified version of the main loop.
							if (!label) {
								for (const auto &cit : caption->paragraphs()) {
									for (pos_type cposIn = 0; cposIn < cit.size(); ++cposIn) {
										const Inset *cinset = cit.getInset(posIn);
										if (cinset && cinset->lyxCode() == LABEL_CODE) {
											label = dynamic_cast<const InsetLabel *>(cinset);
											break;
										}
									}

									if (label)
										break;
								}
							}
						}
						break;
					case LABEL_CODE:
						if (!label)
							label = dynamic_cast<const InsetLabel *>(inset);
						break;
					default:
						break;
				}
			}
		}

		if (content && caption && label)
			break;
	}

	return std::make_tuple(type, content, caption, label);
}


void docbookSubfigures(XMLStream & xs, OutputParams const & runparams, const InsetCaption * caption,
					   const InsetLabel * label, std::vector<const InsetCollapsible *> const & subfigures)
{
	// Ensure there is no label output, it is supposed to be handled as xml:id.
	OutputParams rpNoLabel = runparams;
	if (label)
		rpNoLabel.docbook_anchors_to_ignore.emplace(label->getParam("name"));

	// First, open the formal group.
	docstring attr = docstring();
	if (label)
		attr += "xml:id=\"" + xml::cleanID(label->getParam("name")) + "\"";

	xs.startDivision(false);
	xs << xml::StartTag("formalgroup", attr);
	xs << xml::CR();

	xs << xml::StartTag("title"); // Don't take attr here, the ID should only go in one place, not two.
	if (caption) {
		caption->writeCaptionAsDocBook(xs, rpNoLabel);
	} else {
		xs << "No caption";
		// No caption has been detected, but this tag is required for the document to be valid DocBook.
	}
	xs << xml::EndTag("title");
	xs << xml::CR();

	// Deal with each subfigure individually. This should also deal with their caption and their label.
	// This should be a recursive call to InsetFloat.
	// An item in subfigure should either be an InsetBox containing an InsetFloat, or an InsetBox directly containing
	// an image or a table, or directly an InsetFloat.
	for (const InsetCollapsible * subfigure: subfigures) {
		if (subfigure == nullptr)
			continue;

		// The collapsible may already be a float (InsetFloat).
		if (dynamic_cast<const InsetFloat *>(subfigure)) {
			subfigure->docbook(xs, runparams);
			continue;
		}

		// Subfigures are in boxes, then in InsetFloat.
		{
			bool foundInsetFloat = false;
			for (const auto &it : subfigure->paragraphs()) {
				for (pos_type posIn = 0; posIn < it.size(); ++posIn) {
					const Inset *inset = it.getInset(posIn);
					if (inset && inset->lyxCode() == FLOAT_CODE) {
						foundInsetFloat = true;
						inset->docbook(xs, runparams);
						break;
					}
				}

				if (foundInsetFloat)
					break;
			}
			if (foundInsetFloat)
				continue;
		}

		// Subfigures are in boxes, then directly an image or a table. In that case, generate the whole content of the
		// InsetBox, but not the box container.
		// Impose some model on the subfigure: at most a caption, at most a label, exactly one figure or one table.
		{
			InsetCode stype = NO_CODE;
			const Inset * scontent = nullptr;
			const InsetCaption * scaption = nullptr;
			const InsetLabel * slabel = nullptr;

			std::tie(stype, scontent, scaption, slabel) = docbookParseHopelessSubfigure(subfigure);

			// If there is something, generate it. This is very much like docbookNoSubfigures, but many things
			// must be coded differently because there is no float.
			// TODO: some code is identical to Floating, like Floating::docbookTag or Floating::docbookCaption. How to reuse that code?
			if (scontent) {
				// Floating::docbookCaption()
				string docbook_caption = "caption"; // This is already correct for tables.
				if (stype == GRAPHICS_CODE)
					docbook_caption = "title";

				// Floating::docbookTag() with hasTitle = true, as we are in formalgroup.
				string stag = "float";
				if (stype == GRAPHICS_CODE)
					stag = "figure";
				else if (stype == TABULAR_CODE)
					stag = "table";

				// Ensure there is no label output, it is supposed to be handled as xml:id.
				if (slabel)
					rpNoLabel.docbook_anchors_to_ignore.emplace(slabel->getParam("name"));

				// Ensure the float does not output its caption, as it is handled here (DocBook mandates a specific place for
				// captions, they cannot appear at the end of the float, albeit LyX is happy with that).
				OutputParams rpNoTitle = runparams;
				rpNoTitle.docbook_in_float = true;
				if (stype == TABULAR_CODE)
					rpNoTitle.docbook_in_table = true;

				// Organisation: <float> <title if any/> <contents without title/> </float>.
				docstring sattr = docstring();
				if (slabel)
					sattr += "xml:id=\"" + xml::cleanID(slabel->getParam("name")) + "\"";
				// No layout way of adding attributes, unlike the normal code path.

				xs << xml::StartTag(stag, sattr);
				xs << xml::CR();
				xs << xml::StartTag(docbook_caption);
				if (scaption)
					scaption->writeCaptionAsDocBook(xs, rpNoLabel);
				else // Mandatory in formalgroup.
					xs << "No caption detected";
				xs << xml::EndTag(docbook_caption);
				xs << xml::CR();
				scontent->docbook(xs, rpNoTitle);
				xs << xml::EndTag(stag);
				xs << xml::CR();

				// This subfigure could be generated.
				continue;
			}
		}

		// If there is no InsetFloat in the inset, output a warning.
		xs << XMLStream::ESCAPE_NONE << "Error: no float found in the box. "
							"To use subfigures in DocBook, elements must be wrapped in a float "
		                    "inset and have a title/caption.";
		// TODO: could also output a table, that would ensure that the document is correct and *displays* correctly (but without the right semantics), instead of just an error.

		// Recurse to generate as much content as possible (avoid any loss).
		subfigure->docbook(xs, runparams);
	}

	// Every subfigure is done: close the formal group.
	xs << xml::EndTag("formalgroup");
	xs << xml::CR();
	xs.endDivision();
}


void docbookGenerateFillerMedia(XMLStream & xs)
{
	xs << xml::StartTag("mediaobject");
	xs << xml::CR();
	xs << xml::StartTag("textobject");
	xs << xml::CR();
	xs << xml::StartTag("phrase");
	xs << "This figure is empty.";
	xs << xml::EndTag("phrase");
	xs << xml::CR();
	xs << xml::EndTag("textobject");
	xs << xml::CR();
	xs << xml::EndTag("mediaobject");
	xs << xml::CR();
}


void docbookGenerateFillerTable(XMLStream & xs, BufferParams::TableOutput format)
{
	switch (format) {
	case BufferParams::HTMLTable:
		xs << xml::StartTag("tr");
		xs << xml::CR();
		xs << xml::StartTag("td");
		xs << "This table is empty.";
		xs << xml::EndTag("td");
		xs << xml::CR();
		xs << xml::EndTag("tr");
		xs << xml::CR();
		break;
	case BufferParams::CALSTable:
		// CALS tables allow for <mediaobject>, use that instead.
		docbookGenerateFillerMedia(xs);
		break;
	}
}


void docbookNoSubfigures(XMLStream & xs, OutputParams const & runparams, const InsetCaption * caption,
                         const InsetLabel * label, Floating const & ftype, const InsetFloat * thisFloat) {
	// Ensure there is no label output, it is supposed to be handled as xml:id.
	OutputParams rpNoLabel = runparams;
	if (label)
		rpNoLabel.docbook_anchors_to_ignore.emplace(label->getParam("name"));

	// Ensure the float does not output its caption, as it is handled here (DocBook mandates a specific place for
	// captions, they cannot appear at the end of the float, albeit LyX is happy with that).
	OutputParams rpNoTitle = runparams;
	rpNoTitle.docbook_in_float = true;
	if (ftype.docbookFloatType() == "table")
		rpNoTitle.docbook_in_table = true;

	// Generate the contents of the float (to check for emptiness).
	odocstringstream osFloatContent;
	bool hasFloat = false;

	if (thisFloat) {
		XMLStream xsFloatContent(osFloatContent);
		thisFloat->InsetText::docbook(xsFloatContent, rpNoTitle);
		hasFloat = !osFloatContent.str().empty();
	}

	// Do the same for the caption.
	odocstringstream osCaptionContent;
	bool hasCaption = false;

	if (caption != nullptr) {
		XMLStream xsCaptionContent(osCaptionContent);
		caption->writeCaptionAsDocBook(xsCaptionContent, rpNoLabel);
		hasCaption = !osCaptionContent.str().empty();
	}

	// Organisation: <float with xml:id if any> <title if any/> <contents without title nor xml:id/> </float>.
	// Titles and xml:id are generated with specific insets and must be dealt with using OutputParams.

	// - Generate the attributes for the float tag.
	docstring attr = docstring();
	if (label)
		attr += "xml:id=\"" + xml::cleanID(label->getParam("name")) + "\"";
	if (!ftype.docbookAttr().empty()) {
		if (!attr.empty())
			attr += " ";
		attr += from_utf8(ftype.docbookAttr());
	}

	// - Open the float tag.
	xs << xml::StartTag(ftype.docbookTag(hasCaption), attr);
	xs << xml::CR();

	// - Generate the caption.
	if (hasCaption) {
		string const &titleTag = ftype.docbookCaption();
		xs << xml::StartTag(titleTag);
		xs << XMLStream::ESCAPE_NONE << osCaptionContent.str();
		xs << xml::EndTag(titleTag);
		xs << xml::CR();
	}

	// - Output the actual content of the float or some dummy content (to ensure that the output
	// document is valid). Use HTML tables by default, unless an InsetFloat is given.
	if (hasFloat)
		xs << XMLStream::ESCAPE_NONE << osFloatContent.str();
	else if (ftype.docbookFloatType() == "table") {
		BufferParams::TableOutput tableFormat = BufferParams::HTMLTable;
		if (thisFloat)
			tableFormat = thisFloat->buffer().params().docbook_table_output;
		docbookGenerateFillerTable(xs, tableFormat);
	} else
		docbookGenerateFillerMedia(xs);

	// - Close the float.
	xs << xml::EndTag(ftype.docbookTag(hasCaption));
	xs << xml::CR();
}


void InsetFloat::docbook(XMLStream & xs, OutputParams const & runparams) const
{
	const InsetCaption* caption = getCaptionInset();
	const InsetLabel* label = getLabelInset();

	// Determine whether the float has subfigures.
	std::vector<const InsetCollapsible *> subfigures;
	auto end = paragraphs().end();
	for (auto it = paragraphs().begin(); it != end; ++it) {
		std::vector<const InsetCollapsible *> foundSubfigures = findSubfiguresInParagraph(*it);
		if (!foundSubfigures.empty()) {
			subfigures.reserve(subfigures.size() + foundSubfigures.size());
			subfigures.insert(subfigures.end(), foundSubfigures.begin(), foundSubfigures.end());
		}
	}

	// Gather a few things from global environment that are shared between all following cases.
	FloatList const & floats = buffer().params().documentClass().floats();
	Floating const & ftype = floats.getType(params_.type);

	// Switch on subfigures.
	if (!subfigures.empty())
		docbookSubfigures(xs, runparams, caption, label, subfigures);
	else
		docbookNoSubfigures(xs, runparams, caption, label, ftype, this);
}


bool InsetFloat::insetAllowed(InsetCode code) const
{
	// The case that code == FLOAT_CODE is handled in Text.cpp,
	// because we need to know what type of float is meant.
	switch(code) {
	case WRAP_CODE:
	case FOOT_CODE:
	case MARGIN_CODE:
		return false;
	default:
		return InsetCaptionable::insetAllowed(code);
	}
}


void InsetFloat::updateBuffer(ParIterator const & it, UpdateType utype, bool const deleted)
{
	InsetCaptionable::updateBuffer(it, utype, deleted);
	bool const subflt = (it.innerInsetOfType(FLOAT_CODE)
			     || it.innerInsetOfType(WRAP_CODE));
	setSubfloat(subflt);
}


void InsetFloat::setWide(bool w, bool update_label)
{
	if (!buffer().params().documentClass().floats().allowsWide(params_.type))
		params_.wide = false;
	else
		params_.wide = w;
	if (update_label)
		setNewLabel();
}


void InsetFloat::setSideways(bool s, bool update_label)
{
	if (!buffer().params().documentClass().floats().allowsSideways(params_.type))
		params_.sideways = false;
	else
		params_.sideways = s;
	if (update_label)
		setNewLabel();
}


void InsetFloat::setSubfloat(bool s, bool update_label)
{
	params_.subfloat = s;
	if (update_label)
		setNewLabel();
}


void InsetFloat::setNewLabel()
{
	docstring lab = _("Float: ");

	if (params_.subfloat)
		lab = _("Subfloat: ");

	lab += floatName(params_.type);

	FloatList const & floats = buffer().params().documentClass().floats();

	if (params_.wide && floats.allowsWide(params_.type))
		lab += '*';

	if (params_.sideways && floats.allowsSideways(params_.type))
		lab += _(" (sideways)");

	setLabel(lab);
}


bool InsetFloat::allowsCaptionVariation(std::string const & newtype) const
{
	return !params_.subfloat && newtype != "Unnumbered";
}


TexString InsetFloat::getCaption(OutputParams const & runparams) const
{
	InsetCaption const * ins = getCaptionInset();
	if (ins == 0)
		return TexString();

	otexstringstream os;
	ins->getArgs(os, runparams);

	if (!runparams.nice)
		// increase TexRow precision in non-nice mode
		os << safebreakln;
	os << '[';
	otexstringstream os2;
	ins->getArgument(os2, runparams);
	TexString ts = os2.release();
	docstring & arg = ts.str;
	// Protect ']'
	if (arg.find(']') != docstring::npos)
		arg = '{' + arg + '}';
	os << std::move(ts);
	os << ']';
	if (!runparams.nice)
		os << safebreakln;
	return os.release();
}


void InsetFloat::string2params(string const & in, InsetFloatParams & params)
{
	params = InsetFloatParams();
	if (in.empty())
		return;

	istringstream data(in);
	Lexer lex;
	lex.setStream(data);
	lex.setContext("InsetFloat::string2params");
	params.read(lex);
}


string InsetFloat::params2string(InsetFloatParams const & params)
{
	ostringstream data;
	params.write(data);
	return data.str();
}


} // namespace lyx
