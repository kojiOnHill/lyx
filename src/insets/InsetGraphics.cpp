/**
 * \file InsetGraphics.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Baruch Even
 * \author Herbert Voß
 *
 * Full author contact details are available in file CREDITS.
 */

/*
TODO

    * What advanced features the users want to do?
      Implement them in a non latex dependent way, but a logical way.
      LyX should translate it to latex or any other fitting format.
    * When loading, if the image is not found in the expected place, try
      to find it in the clipart, or in the same directory with the image.
    * The image choosing dialog could show thumbnails of the image formats
      it knows of, thus selection based on the image instead of based on
      filename.
    * Add support for the 'picins' package.
    * Add support for the 'picinpar' package.
*/

/* NOTES:
 * Fileformat:
 * The filename is kept in  the lyx file in a relative way, so as to allow
 * moving the document file and its images with no problem.
 *
 *
 * Conversions:
 *   Postscript output means EPS figures.
 *
 *   PDF output is best done with PDF figures if it's a direct conversion
 *   or PNG figures otherwise.
 *	Image format
 *	from        to
 *	EPS         epstopdf
 *	PS          ps2pdf
 *	JPG/PNG     direct
 *	PDF         direct
 *	others      PNG
 */

#include <config.h>

#include "insets/InsetGraphics.h"
#include "insets/RenderGraphic.h"

#include "Buffer.h"
#include "BufferView.h"
#include "Converter.h"
#include "Cursor.h"
#include "DispatchResult.h"
#include "Encoding.h"
#include "ErrorList.h"
#include "Exporter.h"
#include "Format.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "InsetIterator.h"
#include "LaTeX.h"
#include "LaTeXFeatures.h"
#include "MetricsInfo.h"
#include "Mover.h"
#include "output_docbook.h"
#include "xml.h"
#include "texstream.h"
#include "TocBackend.h"

#include "frontends/alert.h"
#include "frontends/Application.h"

#include "graphics/GraphicsCache.h"
#include "graphics/GraphicsCacheItem.h"
#include "graphics/GraphicsImage.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/docstream.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/Length.h"
#include "support/Lexer.h"
#include "support/lyxlib.h"
#include "support/lstrings.h"
#include "support/os.h"
#include "support/qstring_helpers.h"

#include <QProcess>
#include <QtGui/QImage>

#include <algorithm>
#include <sstream>

using namespace std;
using namespace lyx::support;

namespace lyx {

namespace Alert = frontend::Alert;

namespace {

/// Find the most suitable image format for images in \p format
/// Note that \p format may be unknown (i.e. an empty string)
string findTargetFormat(string const & format, OutputParams const & runparams)
{
	// Are we latexing to DVI or PDF?
	if (runparams.flavor == Flavor::PdfLaTeX
	    || runparams.flavor == Flavor::XeTeX
	    || runparams.flavor == Flavor::LuaTeX) {
		LYXERR(Debug::GRAPHICS, "findTargetFormat: PDF mode");
		Format const * const f = theFormats().getFormat(format);
		// Convert vector graphics to pdf
		if (f && f->vectorFormat())
			return "pdf6";
		// pdflatex can use jpeg, png and pdf directly
		if (format == "jpg")
			return format;
		// Convert everything else to png
		return "png";
	}

	// for HTML and DocBook, we leave the known formats and otherwise convert to png
	if (runparams.flavor == Flavor::Html || runparams.flavor == Flavor::DocBook5) {
		Format const * const f = theFormats().getFormat(format);
		// Convert vector graphics to svg
		if (f && f->vectorFormat() && theConverters().isReachable(format, "svg"))
			return "svg";
		// Leave the known formats alone
		if (format == "jpg" || format == "png" || format == "gif")
			return format;
		// Convert everything else to png
		return "png";
	}
	// If it's postscript, we always do eps.
	LYXERR(Debug::GRAPHICS, "findTargetFormat: PostScript mode");
	if (format != "ps")
		// any other than ps is changed to eps
		return "eps";
	// let ps untouched
	return format;
}


void readInsetGraphics(Lexer & lex, Buffer const & buf, bool allowOrigin,
	InsetGraphicsParams & params)
{
	bool finished = false;

	while (lex.isOK() && !finished) {
		lex.next();

		string const token = lex.getString();
		LYXERR(Debug::GRAPHICS, "Token: '" << token << '\'');

		if (token.empty())
			continue;

		if (token == "\\end_inset") {
			finished = true;
		} else {
			if (!params.Read(lex, token, buf, allowOrigin))
				lyxerr << "Unknown token, "
				       << token
				       << ", skipping."
				       << endl;
		}
	}
}

} // namespace


InsetGraphics::InsetGraphics(Buffer * buf)
	: Inset(buf), graphic_label(xml::uniqueID(from_ascii("graph"))),
	  graphic_(new RenderGraphic(this))
{
}


InsetGraphics::InsetGraphics(InsetGraphics const & ig)
	: Inset(ig),
	  graphic_label(xml::uniqueID(from_ascii("graph"))),
	  graphic_(new RenderGraphic(*ig.graphic_, this))
{
	setParams(ig.params());
}


Inset * InsetGraphics::clone() const
{
	return new InsetGraphics(*this);
}


InsetGraphics::~InsetGraphics()
{
	hideDialogs("graphics", this);
	delete graphic_;
}


void InsetGraphics::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {
	case LFUN_INSET_EDIT: {
		InsetGraphicsParams p = params();
		if (!cmd.argument().empty())
			string2params(to_utf8(cmd.argument()), buffer(), p);
		editGraphics(p);
		break;
	}

	case LFUN_INSET_MODIFY: {
		if (cmd.getArg(0) != "graphics") {
			Inset::doDispatch(cur, cmd);
			break;
		}

		InsetGraphicsParams p;
		string2params(to_utf8(cmd.argument()), buffer(), p);
		if (p.filename.empty()) {
			cur.noScreenUpdate();
			break;
		}

		cur.recordUndo();
		setParams(p);
		// if the inset is part of a graphics group, all the
		// other members should be updated too.
		if (!params_.groupId.empty())
			graphics::unifyGraphicsGroups(buffer(),
						      to_utf8(cmd.argument()));
		break;
	}

	case LFUN_INSET_DIALOG_UPDATE:
		cur.bv().updateDialog("graphics", params2string(params(), buffer()));
		break;

	case LFUN_GRAPHICS_RELOAD:
		params_.filename.refresh();
		graphic_->reload();
		break;

	default:
		Inset::doDispatch(cur, cmd);
		break;
	}
}


bool InsetGraphics::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	switch (cmd.action()) {
	case LFUN_INSET_MODIFY:
		if (cmd.getArg(0) != "graphics")
			return Inset::getStatus(cur, cmd, flag);
	// fall through
	case LFUN_INSET_EDIT:
	case LFUN_INSET_DIALOG_UPDATE:
	case LFUN_GRAPHICS_RELOAD:
		flag.setEnabled(true);
		return true;

	default:
		return Inset::getStatus(cur, cmd, flag);
	}
}


bool InsetGraphics::showInsetDialog(BufferView * bv) const
{
	bv->showDialog("graphics", params2string(params(), bv->buffer()),
		const_cast<InsetGraphics *>(this));
	return true;
}



void InsetGraphics::metrics(MetricsInfo & mi, Dimension & dim) const
{
	graphic_->metrics(mi, dim);
}


void InsetGraphics::draw(PainterInfo & pi, int x, int y) const
{
	graphic_->draw(pi, x, y, params_.darkModeSensitive);
}


void InsetGraphics::write(ostream & os) const
{
	os << "Graphics\n";
	params().Write(os, buffer());
}


void InsetGraphics::read(Lexer & lex)
{
	lex.setContext("InsetGraphics::read");
	//lex >> "Graphics";
	readInsetGraphics(lex, buffer(), true, params_);
	graphic_->update(params().as_grfxParams());
}


void InsetGraphics::outBoundingBox(graphics::BoundingBox & bbox) const
{
	if (bbox.empty())
		return;

	FileName const file(params().filename.absFileName());

	if (!file.exists())
		return;

	// No correction is necessary for a vector image
	bool const zipped = theFormats().isZippedFile(file);
	FileName const unzipped_file = zipped ? unzipFile(file) : file;
	string const format = theFormats().getFormatFromFile(unzipped_file);
	if (zipped)
		unzipped_file.removeFile();
	if (theFormats().getFormat(format)
	    && theFormats().getFormat(format)->vectorFormat())
		return;

	// Get the actual image dimensions in pixels
	int width = 0;
	int height = 0;
	graphics::Cache & gc = graphics::Cache::get();
	if (gc.inCache(file)) {
		graphics::Image const * image = gc.item(file)->image();
		if (image) {
			width  = image->width();
			height = image->height();
		}
	}
	// Even if cached, the image is not loaded without GUI
	if  (width == 0 && height == 0) {
		QImage image(toqstr(file.absFileName()));
		width  = image.width();
		height = image.height();
	}
	if (width == 0 || height == 0)
		return;

	// Use extractbb to find the dimensions in the typeset output
	QProcess extractbb;
	extractbb.start("extractbb", QStringList() << "-O" << toqstr(file.absFileName()));
	if (!extractbb.waitForStarted() || !extractbb.waitForFinished()) {
		LYXERR0("Cannot read output bounding box of " << file);
		return;
	}

	string const result = extractbb.readAll().constData();
	size_t i = result.find("%%BoundingBox:");
	if (i == string::npos) {
		LYXERR0("Cannot find output bounding box of " << file);
		return;
	}

	string const bb = result.substr(i);
	int out_width = convert<int>(token(bb, ' ', 3));
	int out_height = convert<int>(token(bb, ' ', 4));

	// Compute the scaling ratio and correct the bounding box
	double scalex = out_width / double(width);
	double scaley = out_height / double(height);

	bbox.xl.value(scalex * bbox.xl.value());
	bbox.xr.value(scalex * bbox.xr.value());
	bbox.yb.value(scaley * bbox.yb.value());
	bbox.yt.value(scaley * bbox.yt.value());
}


string InsetGraphics::createLatexOptions(bool const ps) const
{
	// Calculate the options part of the command, we must do it to a string
	// stream since we might have a trailing comma that we would like to remove
	// before writing it to the output stream.
	ostringstream options;
	if (!params().bbox.empty()) {
		graphics::BoundingBox out_bbox = params().bbox;
		outBoundingBox(out_bbox);
		string const key = ps ? "bb=" : "viewport=";
		options << key << out_bbox.xl.asLatexString() << ' '
		        << out_bbox.yb.asLatexString() << ' '
		        << out_bbox.xr.asLatexString() << ' '
		        << out_bbox.yt.asLatexString() << ',';
	}
	if (params().draft)
	    options << "draft,";
	if (params().clip)
	    options << "clip,";
	ostringstream size;
	double const scl = convert<double>(params().scale);
	if (!params().scale.empty() && !float_equal(scl, 0.0, 0.05)) {
		if (!float_equal(scl, 100.0, 0.05))
			size << "scale=" << scl / 100.0 << ',';
	} else {
		if (!params().width.zero())
			size << "width=" << params().width.asLatexString() << ',';
		if (!params().height.zero())
			size << "totalheight=" << params().height.asLatexString() << ',';
		if (params().keepAspectRatio)
			size << "keepaspectratio,";
	}
	if (params().scaleBeforeRotation && !size.str().empty())
		options << size.str();

	// Make sure rotation angle is not very close to zero;
	// a float can be effectively zero but not exactly zero.
	if (!params().rotateAngle.empty()
		&& !float_equal(convert<double>(params().rotateAngle), 0.0, 0.001)) {
	    options << "angle=" << params().rotateAngle << ',';
	    if (!params().rotateOrigin.empty()) {
		options << "origin=" << params().rotateOrigin[0];
		if (contains(params().rotateOrigin,"Top"))
		    options << 't';
		else if (contains(params().rotateOrigin,"Bottom"))
		    options << 'b';
		else if (contains(params().rotateOrigin,"Baseline"))
		    options << 'B';
		options << ',';
	    }
	}
	if (!params().scaleBeforeRotation && !size.str().empty())
		options << size.str();

	if (!params().special.empty())
	    options << params().special << ',';

	string opts = options.str();
	// delete last ','
	if (suffixIs(opts, ','))
		opts = opts.substr(0, opts.size() - 1);

	return opts;
}


docstring InsetGraphics::toDocbookLength(Length const & len) const
{
	odocstringstream result;
	switch (len.unit()) {
	case Length::SP: // Scaled point (65536sp = 1pt) TeX's smallest unit.
		result << len.value() * 65536.0 * 72 / 72.27 << "pt";
		break;
	case Length::PT: // Point = 1/72.27in = 0.351mm
		result << len.value() * 72 / 72.27 << "pt";
		break;
	case Length::BP: // Big point (72bp = 1in), also PostScript point
		result << len.value() << "pt";
		break;
	case Length::DD: // Didot point = 1/72 of a French inch, = 0.376mm
		result << len.value() * 0.376 << "mm";
		break;
	case Length::MM: // Millimeter = 2.845pt
		result << len.value() << "mm";
		break;
	case Length::PC: // Pica = 12pt = 4.218mm
		result << len.value() << "pc";
		break;
	case Length::CC: // Cicero = 12dd = 4.531mm
		result << len.value() * 4.531 << "mm";
		break;
	case Length::CM: // Centimeter = 10mm = 2.371pc
		result << len.value() << "cm";
		break;
	case Length::IN: // Inch = 25.4mm = 72.27pt = 6.022pc
		result << len.value() << "in";
		break;
	case Length::EX: // Height of a small "x" for the current font.
		// Obviously we have to compromise here. Any better ratio than 1.5 ?
		result << len.value() / 1.5 << "em";
		break;
	case Length::EM: // Width of capital "M" in current font.
		result << len.value() << "em";
		break;
	case Length::MU: // Math unit (18mu = 1em) for positioning in math mode
		result << len.value() * 18 << "em";
		break;
	case Length::PTW: // Percent of TextWidth
	case Length::PCW: // Percent of ColumnWidth
	case Length::PPW: // Percent of PageWidth
	case Length::PLW: // Percent of LineWidth
	case Length::PTH: // Percent of TextHeight
	case Length::PPH: // Percent of PaperHeight
	case Length::BLS: // Percent of BaselineSkip
		// Sigh, this will go wrong.
		result << len.value() << "%";
		break;
	default:
		result << len.asDocstring();
		break;
	}
	return result.str();
}


docstring InsetGraphics::createDocBookAttributes() const
{
	// Calculate the options part of the command, we must do it to a string
	// stream since we copied the code from createLatexParams() ;-)

	odocstringstream options;
	auto const scl = convert<double>(params().scale);
	if (!params().scale.empty() && !float_equal(scl, 0.0, 0.05)) {
		if (!float_equal(scl, 100.0, 0.05))
			options << " scale=\""
				    << support::iround(scl)
				    << "\" ";
	} else {
		if (!params().width.zero())
			options << " width=\"" << toDocbookLength(params().width)  << "\" ";
		if (!params().height.zero())
			options << " depth=\"" << toDocbookLength(params().height)  << "\" ";
		if (params().keepAspectRatio)
			// This will be irrelevant unless both width and height are set
			options << "scalefit=\"1\" ";
	}

	// TODO: parse params().special?

	// trailing blanks are ok ...
	return options.str();
}


namespace {

enum GraphicsCopyStatus {
	SUCCESS,
	FAILURE,
	IDENTICAL_PATHS,
	IDENTICAL_CONTENTS
};


pair<GraphicsCopyStatus, FileName> const
copyFileIfNeeded(FileName const & file_in, FileName const & file_out)
{
	LYXERR(Debug::FILES, "Comparing " << file_in << " and " << file_out);
	unsigned long const checksum_in  = file_in.checksum();
	unsigned long const checksum_out = file_out.checksum();

	if (checksum_in == checksum_out)
		// Nothing to do...
		return make_pair(IDENTICAL_CONTENTS, file_out);

	Mover const & mover = getMover(theFormats().getFormatFromFile(file_in));
	bool const success = mover.copy(file_in, file_out);
	if (!success) {
		// FIXME UNICODE
		LYXERR(Debug::GRAPHICS,
			to_utf8(bformat(_("Could not copy the file\n%1$s\n"
							   "into the temporary directory."),
						from_utf8(file_in.absFileName()))));
	}

	GraphicsCopyStatus status = success ? SUCCESS : FAILURE;
	return make_pair(status, file_out);
}


pair<GraphicsCopyStatus, FileName> const
copyToDirIfNeeded(DocFileName const & file, string const & dir, bool encrypt_path)
{
	string const file_in = file.absFileName();
	string const only_path = onlyPath(file_in);
	if (rtrim(only_path, "/") == rtrim(dir, "/"))
		return make_pair(IDENTICAL_PATHS, FileName(file_in));

	string mangled = file.mangledFileName(empty_string(), encrypt_path);
	if (theFormats().isZippedFile(file)) {
		// We need to change _eps.gz to .eps.gz. The mangled name is
		// still unique because of the counter in mangledFileName().
		// We can't just call mangledFileName() with the zip
		// extension removed, because base.eps and base.eps.gz may
		// have different content but would get the same mangled
		// name in this case.
		// Also take into account that if the name of the zipped file
		// has no zip extension then the name of the unzipped one is
		// prefixed by "unzipped_".
		string const base = removeExtension(file.unzippedFileName());
		string::size_type const prefix_len =
			prefixIs(onlyFileName(base), "unzipped_") ? 9 : 0;
		string::size_type const ext_len =
			file_in.length() + prefix_len - base.length();
		mangled[mangled.length() - ext_len] = '.';
	}
	FileName const file_out(makeAbsPath(mangled, dir));

	return copyFileIfNeeded(file, file_out);
}


string const stripExtensionIfPossible(string const & file, bool nice)
{
	// Remove the extension so the LaTeX compiler will use whatever
	// is appropriate (when there are several versions in different
	// formats).
	// Do this only if we are not exporting for internal usage, because
	// pdflatex prefers png over pdf and it would pick up the png images
	// that we generate for preview.
	// This works only if the filename contains no dots besides
	// the just removed one. We can fool here by replacing all
	// dots with a macro whose definition is just a dot ;-)
	// The automatic format selection does not work if the file
	// name is escaped.
	string const latex_name = latex_path(file, EXCLUDE_EXTENSION);
	if (!nice || contains(latex_name, '"'))
		return latex_name;
	return latex_path(removeExtension(file), PROTECT_EXTENSION, ESCAPE_DOTS);
}


string const stripExtensionIfPossible(string const & file, string const & to, bool nice)
{
	// No conversion is needed. LaTeX can handle the graphic file as is.
	// This is true even if the orig_file is compressed.
	Format const * f = theFormats().getFormat(to);
	if (!f)
		return latex_path(file, EXCLUDE_EXTENSION);
	string const to_format = f->extension();
	string const file_format = getExtension(file);
	// for latex .ps == .eps
	if (to_format == file_format ||
	    (to_format == "eps" && file_format ==  "ps") ||
	    (to_format ==  "ps" && file_format == "eps"))
		return stripExtensionIfPossible(file, nice);
	return latex_path(file, EXCLUDE_EXTENSION);
}

} // namespace


string InsetGraphics::prepareFile(OutputParams const & runparams) const
{
	// The following code depends on non-empty filenames
	if (params().filename.empty())
		return string();

	string const orig_file = params().filename.absFileName();
	// this is for dryrun and display purposes, do not use latexFilename
	string const rel_file = params().filename.relFileName(buffer().filePath());

	// previewing source code, no file copying or file format conversion
	if (runparams.dryrun)
		return stripExtensionIfPossible(rel_file, runparams.nice);

	// The master buffer. This is useful when there are multiple levels
	// of include files
	Buffer const * masterBuffer = buffer().masterBuffer();

	// Return the output name if we are inside a comment or the file does
	// not exist.
	// We are not going to change the extension or using the name of the
	// temporary file, the code is already complicated enough.
	if (runparams.inComment || !params().filename.isReadableFile())
		return params().filename.outputFileName(masterBuffer->filePath());

	// We place all temporary files in the master buffer's temp dir.
	// This is possible because we use mangled file names.
	// This is necessary for DVI export.
	string const temp_path = masterBuffer->temppath();

	// temp_file will contain the file for LaTeX to act on if, for example,
	// we move it to a temp dir or uncompress it.
	FileName temp_file;
	GraphicsCopyStatus status;
	tie(status, temp_file) = copyToDirIfNeeded(params().filename, temp_path, false);

	if (status == FAILURE)
		return orig_file;

	// a relative filename should be relative to the master buffer.
	// "nice" means that the buffer is exported to LaTeX format but not
	// run through the LaTeX compiler.
	string output_file = runparams.nice ?
		params().filename.outputFileName(masterBuffer->filePath()) :
		onlyFileName(temp_file.absFileName());

	if (runparams.nice) {
		if (!isValidLaTeXFileName(output_file)) {
			frontend::Alert::warning(_("Invalid filename"),
				_("The following filename will cause troubles "
				  "when running the exported file through LaTeX: ") +
				from_utf8(output_file));
		}
		// only show DVI-specific warning when export format is plain latex
		if (!isValidDVIFileName(output_file)
			&& runparams.flavor == Flavor::LaTeX) {
				frontend::Alert::warning(_("Problematic filename for DVI"),
				         _("The following filename can cause troubles "
					       "when running the exported file through LaTeX "
						   "and opening the resulting DVI: ") +
					     from_utf8(output_file), true);
		}
	}

	FileName source_file = runparams.nice ? FileName(params().filename) : temp_file;
	// determine the export format
	string const tex_format = flavor2format(runparams.flavor);

	if (theFormats().isZippedFile(params().filename)) {
		FileName const unzipped_temp_file =
			FileName(unzippedFileName(temp_file.absFileName()));
		output_file = unzippedFileName(output_file);
		source_file = FileName(unzippedFileName(source_file.absFileName()));
		if (compare_timestamps(unzipped_temp_file, temp_file) > 0) {
			// temp_file has been unzipped already and
			// orig_file has not changed in the meantime.
			temp_file = unzipped_temp_file;
			LYXERR(Debug::GRAPHICS, "\twas already unzipped to " << temp_file);
		} else {
			// unzipped_temp_file does not exist or is too old
			temp_file = unzipFile(temp_file);
			LYXERR(Debug::GRAPHICS, "\tunzipped to " << temp_file);
		}
	}

	string const from = theFormats().getFormatFromFile(temp_file);
	if (from.empty())
		LYXERR(Debug::GRAPHICS, "\tCould not get file format.");

	string const to   = findTargetFormat(from, runparams);
	string const ext  = theFormats().extension(to);
	LYXERR(Debug::GRAPHICS, "\t we have: from " << from << " to " << to);

	// We're going to be running the exported buffer through the LaTeX
	// compiler, so must ensure that LaTeX can cope with the graphics
	// file format.

	LYXERR(Debug::GRAPHICS, "\tthe orig file is: " << orig_file);

	if (from == to) {
		// source and destination formats are the same
		if (!runparams.nice && !temp_file.hasExtension(ext)) {
			// The LaTeX compiler will not be able to determine
			// the file format from the extension, so we must
			// change it.
			FileName const new_file =
				FileName(changeExtension(temp_file.absFileName(), ext));
			if (temp_file.moveTo(new_file)) {
				temp_file = new_file;
				output_file = changeExtension(output_file, ext);
				source_file =
					FileName(changeExtension(source_file.absFileName(), ext));
			} else {
				LYXERR(Debug::GRAPHICS, "Could not rename file `"
					<< temp_file << "' to `" << new_file << "'.");
			}
		}
		// The extension of temp_file might be != ext!
		runparams.exportdata->addExternalFile(tex_format, source_file,
						      output_file);
		runparams.exportdata->addExternalFile("dvi", source_file,
						      output_file);
		return stripExtensionIfPossible(output_file, to, runparams.nice);
	}

	// so the source and destination formats are different
	FileName const to_file = FileName(changeExtension(temp_file.absFileName(), ext));
	string const output_to_file = changeExtension(output_file, ext);

	// Do we need to perform the conversion?
	// Yes if to_file does not exist or if temp_file is newer than to_file
	if (compare_timestamps(temp_file, to_file) < 0) {
		// FIXME UNICODE
		LYXERR(Debug::GRAPHICS,
			to_utf8(bformat(_("No conversion of %1$s is needed after all"),
				   from_utf8(rel_file))));
		runparams.exportdata->addExternalFile(tex_format, to_file,
						      output_to_file);
		runparams.exportdata->addExternalFile("dvi", to_file,
						      output_to_file);
		return stripExtensionIfPossible(output_to_file, runparams.nice);
	}

	LYXERR(Debug::GRAPHICS,"\tThe original file is " << orig_file << "\n"
		<< "\tA copy has been made and convert is to be called with:\n"
		<< "\tfile to convert = " << temp_file << '\n'
		<< "\t from " << from << " to " << to);

	// FIXME (Abdel 12/08/06): Is there a need to show these errors?
	ErrorList el;
	Converters::RetVal const rv = 
	    theConverters().convert(&buffer(), temp_file, to_file, params().filename,
			       from, to, el,
			       Converters::try_default | Converters::try_cache);
	if (rv == Converters::KILLED) {
		LYXERR0("Graphics preparation killed.");
		if (buffer().isClone() && buffer().isExporting())
			throw ConversionException();
	} else if (rv == Converters::SUCCESS) {
		runparams.exportdata->addExternalFile(tex_format,
				to_file, output_to_file);
		runparams.exportdata->addExternalFile("dvi",
				to_file, output_to_file);
	}

	return stripExtensionIfPossible(output_to_file, runparams.nice);
}


void InsetGraphics::latex(otexstream & os,
			  OutputParams const & runparams) const
{
	// If there is no file specified or not existing,
	// just output a message about it in the latex output.
	LYXERR(Debug::GRAPHICS, "insetgraphics::latex: Filename = "
		<< params().filename.absFileName());

	bool const file_exists = !params().filename.empty()
			&& params().filename.isReadableFile();
	string message;
	// PDFLaTeX and Xe/LuaTeX fall back to draft themselves
	// and error about it. For DVI/PS, we do something similar here.
	// We also don't do such tricks when simply exporting a LaTeX file.
	if (!file_exists && !runparams.nice && runparams.flavor == Flavor::LaTeX) {
		TeXErrors terr;
		ErrorList & errorList = buffer().errorList("Export");
		docstring const s = params().filename.empty() ?
					_("Graphic not specified. Falling back to `draft' mode.")
				      : bformat(_("Graphic `%1$s' was not found. Falling back to `draft' mode."),
						params().filename.absoluteFilePath());
		Paragraph const & par = buffer().paragraphs().front();
		errorList.push_back(ErrorItem(_("Graphic not found!"), s,
					      {par.id(), 0}, {par.id(), -1}));
		buffer().bufferErrors(terr, errorList);
		if (params().bbox.empty())
		    message = "bb = 0 0 200 100";
		if (!params().draft) {
			if (!message.empty())
				message += ", ";
			message += "draft";
		}
		if (!message.empty())
			message += ", ";
		message += "type=eps";
		// If no existing file "filename" was found LaTeX
		// draws only a rectangle with the above bb and the
		// not found filename in it.
		LYXERR(Debug::GRAPHICS, "\tMessage = \"" << message << '\"');
	}

	// These variables collect all the latex code that should be before and
	// after the actual includegraphics command.
	string before;
	string after;

	// Write the options if there are any.
	bool const ps = runparams.flavor == Flavor::LaTeX
		|| runparams.flavor == Flavor::DviLuaTeX;
	string const opts = createLatexOptions(ps);
	LYXERR(Debug::GRAPHICS, "\tOpts = " << opts);

	if (contains(opts, '=') && contains(runparams.active_chars, '=')) {
		// We have a language that makes = active. Deactivate locally
		// for keyval option parsing (#2005).
		before = "\\begingroup\\catcode`\\=12";
		after = "\\endgroup ";
	}

	if (runparams.moving_arg)
		before += "\\protect";

	// We never use the starred form, we use the "clip" option instead.
	before += "\\includegraphics";

	if (!opts.empty() && !message.empty())
		before += ('[' + opts + ',' + message + ']');
	else if (!opts.empty() || !message.empty())
		before += ('[' + opts + message + ']');

	LYXERR(Debug::GRAPHICS, "\tBefore = " << before << "\n\tafter = " << after);

	string latex_str = before + '{';
	// Convert the file if necessary.
	// Remove the extension so LaTeX will use whatever is appropriate
	// (when there are several versions in different formats)
	docstring file_path = from_utf8(prepareFile(runparams));
	// we can only output characters covered by the current
	// encoding!
	docstring uncodable;
	docstring encodable_file_path;
	for (char_type c : file_path) {
		try {
			if (runparams.encoding->encodable(c))
				encodable_file_path += c;
			else if (runparams.dryrun) {
				encodable_file_path += "<" + _("LyX Warning: ")
						+ _("uncodable character") + " '";
				encodable_file_path += docstring(1, c);
				encodable_file_path += "'>";
			} else
				uncodable += c;
		} catch (EncodingException & /* e */) {
			if (runparams.dryrun) {
				encodable_file_path += "<" + _("LyX Warning: ")
						+ _("uncodable character") + " '";
				encodable_file_path += docstring(1, c);
				encodable_file_path += "'>";
			} else
				uncodable += c;
		}
	}
	if (!uncodable.empty() && !runparams.silent) {
		// issue a warning about omitted characters
		// FIXME: should be passed to the error dialog
		frontend::Alert::warning(_("Uncodable character in file path"),
			bformat(_("The following characters in one of the graphic paths are\n"
				  "not representable in the current encoding and have been omitted: %1$s.\n"
				  "You need to adapt either the encoding or the path."),
			uncodable));
	}
	latex_str += to_utf8(encodable_file_path);
	latex_str += '}' + after;
	// FIXME UNICODE
	os << from_utf8(latex_str);

	LYXERR(Debug::GRAPHICS, "InsetGraphics::latex outputting:\n" << latex_str);
}


int InsetGraphics::plaintext(odocstringstream & os,
        OutputParams const &, size_t) const
{
	// No graphics in ascii output. Possible to use gifscii to convert
	// images to ascii approximation.
	// 1. Convert file to ascii using gifscii
	// 2. Read ascii output file and add it to the output stream.
	// at least we send the filename
	// FIXME UNICODE
	// FIXME: We have no idea what the encoding of the filename is

	docstring const str = bformat(buffer().B_("Graphics file: %1$s"),
				      from_utf8(params().filename.absFileName()));
	os << '<' << str << '>';

	return 2 + str.size();
}


// For explanation on inserting graphics into DocBook checkout:
// http://en.tldp.org/LDP/LDP-Author-Guide/html/inserting-pictures.html
// See also the docbook guide at http://www.docbook.org/
void InsetGraphics::docbook(XMLStream & xs, OutputParams const & runparams) const
{
	string fn = params().filename.relFileName(runparams.export_folder);
	string tag = runparams.docbook_in_float ? "mediaobject" : "inlinemediaobject";

	xs << xml::StartTag(tag);
	xs << xml::CR();
	xs << xml::StartTag("imageobject");
	xs << xml::CR();
	xs << xml::CompTag("imagedata", "fileref=\"" + fn + "\" " + to_utf8(createDocBookAttributes()));
	xs << xml::CR();
	xs << xml::EndTag("imageobject");
	xs << xml::CR();
	xs << xml::EndTag(tag);
	xs << xml::CR();
}


string InsetGraphics::prepareHTMLFile(OutputParams const & runparams) const
{
	// The following code depends on non-empty filenames
	if (params().filename.empty())
		return string();

	if (!params().filename.isReadableFile())
		return string();

	// The master buffer. This is useful when there are multiple levels
	// of include files
	Buffer const * masterBuffer = buffer().masterBuffer();

	// We place all temporary files in the master buffer's temp dir.
	// This is possible because we use mangled file names.
	// FIXME We may want to put these files in some special temporary
	// directory.
	string const temp_path = masterBuffer->temppath();

	// Copy to temporary directory.
	FileName temp_file;
	GraphicsCopyStatus status;
	tie(status, temp_file) = copyToDirIfNeeded(params().filename, temp_path, true);

	if (status == FAILURE)
		return string();

	string const from = theFormats().getFormatFromFile(temp_file);
	if (from.empty()) {
		LYXERR(Debug::GRAPHICS, "\tCould not get file format.");
		return string();
	}

	string const to   = findTargetFormat(from, runparams);
	string const ext  = theFormats().extension(to);
	string const orig_file = params().filename.absFileName();
	string output_file = onlyFileName(temp_file.absFileName());
	LYXERR(Debug::GRAPHICS, "\t we have: from " << from << " to " << to);
	LYXERR(Debug::GRAPHICS, "\tthe orig file is: " << orig_file);

	if (from == to) {
		// source and destination formats are the same
		runparams.exportdata->addExternalFile("xhtml", temp_file, output_file);
		return output_file;
	}

	// so the source and destination formats are different
	FileName const to_file = FileName(changeExtension(temp_file.absFileName(), ext));
	string const output_to_file = changeExtension(output_file, ext);

	// Do we need to perform the conversion?
	// Yes if to_file does not exist or if temp_file is newer than to_file
	if (compare_timestamps(temp_file, to_file) < 0) {
		// FIXME UNICODE
		LYXERR(Debug::GRAPHICS,
			to_utf8(bformat(_("No conversion of %1$s is needed after all"),
				   from_utf8(orig_file))));
		runparams.exportdata->addExternalFile("xhtml", to_file, output_to_file);
		return output_to_file;
	}

	LYXERR(Debug::GRAPHICS,"\tThe original file is " << orig_file << "\n"
		<< "\tA copy has been made and convert is to be called with:\n"
		<< "\tfile to convert = " << temp_file << '\n'
		<< "\t from " << from << " to " << to);

	// FIXME (Abdel 12/08/06): Is there a need to show these errors?
	ErrorList el;
	Converters::RetVal const rv =
		theConverters().convert(&buffer(), temp_file, to_file, params().filename,
			from, to, el, Converters::try_default | Converters::try_cache);
	if (rv == Converters::KILLED) {
		if (buffer().isClone() && buffer().isExporting())
			throw ConversionException();
		return string();
	}
	if (rv != Converters::SUCCESS)
		return string();
	runparams.exportdata->addExternalFile("xhtml", to_file, output_to_file);
	return output_to_file;
}


CtObject InsetGraphics::getCtObject(OutputParams const &) const
{
	return CtObject::Object;
}


docstring InsetGraphics::xhtml(XMLStream & xs, OutputParams const & op) const
{
	string const output_file = op.dryrun ? string() : prepareHTMLFile(op);

	if (output_file.empty() && !op.dryrun) {
		LYXERR0("InsetGraphics::xhtml: Unable to prepare file `"
		        << params().filename << "' for output. File missing?");
		string const attr = "src='" + params().filename.absFileName()
		                    + "' alt='image: " + output_file + "'";
		xs << xml::CompTag("img", attr);
		return docstring();
	}

	// FIXME XHTML
	// We aren't doing anything with the crop and rotate parameters, and it would
	// really be better to do width and height conversion, rather than to output
	// these parameters here.
	string imgstyle;
	bool const havewidth  = !params().width.zero();
	bool const haveheight = !params().height.zero();
	if (havewidth || haveheight) {
		if (havewidth)
			imgstyle = "width: " + params().width.asHTMLString() + ";";
		if (haveheight)
			imgstyle += " height: " + params().height.asHTMLString() + ";";
	} else if (params().scale != "100") {
		// Use the scale on the bounding box.
		char* endPtr;
		double const scale = strtod(params().scale.c_str(), &endPtr);
		if (*endPtr == '\0') { // Parsing was possible.
			Length width = params().width;
			width.value(scale * width.value());
			Length height = params().height;
			height.value(scale * height.value());

			imgstyle = "width: " + width.asHTMLString() + "; ";
			imgstyle += "height: " + height.asHTMLString() + ";";
		}
		// Otherwise, failure to parse the scale: no information to pass on
		// to the HTML output.
	}
	if (!imgstyle.empty())
		imgstyle = "style='" + imgstyle + "' ";

	string const attr = imgstyle + "src='" + output_file + "' alt='image: "
	                    + output_file + "'";
	xs << xml::CompTag("img", attr);
	return docstring();
}


void InsetGraphics::validate(LaTeXFeatures & features) const
{
	// If we have no image, we should not require anything.
	if (params().filename.empty())
		return;

	features.includeFile(graphic_label,
			     removeExtension(params().filename.absFileName()));

	features.require("graphicx");

	if (features.runparams().nice) {
		string const rel_file = params().filename.onlyFileNameWithoutExt();
		if (contains(rel_file, "."))
			features.require("lyxdot");
	}
	if (features.inDeletedInset()) {
		features.require("tikz");
		features.require("ct-tikz-object-sout");
	}
}


bool InsetGraphics::setParams(InsetGraphicsParams const & p)
{
	// If nothing is changed, just return and say so.
	if (params() == p && !p.filename.empty())
		return false;

	// Copy the new parameters.
	params_ = p;

	// Update the display using the new parameters.
	graphic_->update(params().as_grfxParams());

	// We have changed data, report it.
	return true;
}


InsetGraphicsParams const & InsetGraphics::params() const
{
	return params_;
}


void InsetGraphics::editGraphics(InsetGraphicsParams const & p) const
{
	theFormats().edit(buffer(), p.filename,
		     theFormats().getFormatFromFile(p.filename));
}


void InsetGraphics::addToToc(DocIterator const & cpit, bool output_active,
							 UpdateType, TocBackend & backend) const
{
	//FIXME UNICODE
	docstring const str = from_utf8(params_.filename.onlyFileName());
	TocBuilder & b = backend.builder("graphics");
	b.pushItem(cpit, str, output_active);
	b.pop();
}


string InsetGraphics::contextMenuName() const
{
	return "context-graphics";
}


void InsetGraphics::string2params(string const & in, Buffer const & buffer,
	InsetGraphicsParams & params)
{
	if (in.empty())
		return;

	istringstream data(in);
	Lexer lex;
	lex.setStream(data);
	lex.setContext("InsetGraphics::string2params");
	lex >> "graphics";
	params = InsetGraphicsParams();
	readInsetGraphics(lex, buffer, false, params);
}


string InsetGraphics::params2string(InsetGraphicsParams const & params,
	Buffer const & buffer)
{
	ostringstream data;
	data << "graphics" << ' ';
	params.Write(data, buffer);
	data << "\\end_inset\n";
	return data.str();
}


docstring InsetGraphics::toolTip(BufferView const &, int, int) const
{
	return from_utf8(params().filename.onlyFileName());
}

namespace graphics {

void getGraphicsGroups(Buffer const & b, set<string> & ids)
{
	for (Inset const & it : b.inset()) {
		InsetGraphics const * ins = it.asInsetGraphics();
		if (!ins)
			continue;
		InsetGraphicsParams const & inspar = ins->getParams();
		if (!inspar.groupId.empty())
			ids.insert(inspar.groupId);
	}
}


int countGroupMembers(Buffer const & b, string const & groupId)
{
	int n = 0;
	if (groupId.empty())
		return n;
	for (Inset const & it : b.inset()) {
		InsetGraphics const * ins = it.asInsetGraphics();
		if (!ins)
			continue; 
		if (ins->getParams().groupId == groupId)
			++n;
	}
	return n;
}


string getGroupParams(Buffer const & b, string const & groupId)
{
	if (groupId.empty())
		return string();
	for (Inset const & it : b.inset()) {
		InsetGraphics const * ins = it.asInsetGraphics();
		if (!ins)
			continue;
		InsetGraphicsParams const & inspar = ins->getParams();
		if (inspar.groupId == groupId) {
			InsetGraphicsParams tmp = inspar;
			tmp.filename.erase();
			return InsetGraphics::params2string(tmp, b);
		}
	}
	return string();
}


void unifyGraphicsGroups(Buffer & b, string const & argument)
{
	InsetGraphicsParams params;
	InsetGraphics::string2params(argument, b, params);

	// This handles undo groups automagically
	UndoGroupHelper ugh(&b);
	Inset & inset = b.inset();
	InsetIterator it  = begin(inset);
	InsetIterator const itend = end(inset);
	for (; it != itend; ++it) {
		InsetGraphics * ins = it->asInsetGraphics();
		if (!ins)
			continue;
		InsetGraphicsParams const & inspar = ins->getParams();
		if (params.groupId == inspar.groupId) {
			CursorData(it).recordUndo();
			params.filename = inspar.filename;
			ins->setParams(params);
		}
	}
}


InsetGraphics * getCurrentGraphicsInset(Cursor const & cur)
{
	Inset * instmp = &cur.inset();
	if (!instmp->asInsetGraphics())
		instmp = cur.nextInset();
	if (!instmp || !instmp->asInsetGraphics())
		return 0;

	return instmp->asInsetGraphics();
}

} // namespace graphics

} // namespace lyx
