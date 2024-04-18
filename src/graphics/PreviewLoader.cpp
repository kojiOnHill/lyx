/**
 * \file PreviewLoader.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "PreviewLoader.h"
#include "PreviewImage.h"
#include "GraphicsCache.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "Converter.h"
#include "Encoding.h"
#include "Format.h"
#include "LyXRC.h"
#include "output.h"
#include "OutputParams.h"
#include "TexRow.h"
#include "texstream.h"

#include "frontends/Application.h" // hexName

#include "support/convert.h"
#include "support/debug.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/ForkedCalls.h"
#include "support/lstrings.h"
#include "support/os.h"

#include "support/TempFile.h"

#include <atomic>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>

#include <QTimer>

using namespace std;
using namespace lyx;
using namespace lyx::support;



namespace {

typedef pair<docstring, FileName> SnippetPair;

// A list of all snippets to be converted to previews
typedef list<docstring> PendingSnippets;

// Each item in the vector is a pair<snippet, image file name>.
typedef vector<SnippetPair> BitmapFile;


FileName const unique_tex_filename(FileName const & bufferpath)
{
	TempFile tempfile(bufferpath, "lyxpreviewXXXXXX.tex");
	tempfile.setAutoRemove(false);
	return tempfile.name();
}


void setAscentFractions(vector<double> & ascent_fractions,
			FileName const & metrics_file)
{
	// If all else fails, then the images will have equal ascents and
	// descents.
	vector<double>::iterator it  = ascent_fractions.begin();
	vector<double>::iterator end = ascent_fractions.end();
	fill(it, end, 0.5);

	ifstream in(metrics_file.toFilesystemEncoding().c_str());
	if (!in.good()) {
		LYXERR(lyx::Debug::GRAPHICS, "setAscentFractions(" << metrics_file << ")\n"
			<< "Unable to open file!");
		return;
	}

	bool error = false;

	int snippet_counter = 1;
	while (!in.eof() && it != end) {
		string snippet;
		int id;
		double ascent_fraction;

		in >> snippet >> id >> ascent_fraction;

		if (!in.good())
			// eof after all
			break;

		error = snippet != "Snippet";
		if (error)
			break;

		error = id != snippet_counter;
		if (error)
			break;

		*it = ascent_fraction;

		++snippet_counter;
		++it;
	}

	if (error) {
		LYXERR(lyx::Debug::GRAPHICS, "setAscentFractions(" << metrics_file << ")\n"
			<< "Error reading file!\n");
	}
}


std::function <bool (SnippetPair const &)> FindFirst(docstring const & comp)
{
	return [&comp](SnippetPair const & sp) { return sp.first == comp; };
}


/// Store info on a currently executing, forked process.
class InProgress {
public:
	///
	InProgress() : pid(0) {}
	///
	InProgress(string const & filename_base,
		   PendingSnippets const & pending,
		   string const & to_format);
	/// Remove any files left lying around and kill the forked process.
	void stop() const;

	///
	string command;
	///
	FileName metrics_file;
	///
	BitmapFile snippets;
	///
	pid_t pid;
};

typedef map<pid_t, InProgress>  InProgressProcesses;

typedef InProgressProcesses::value_type InProgressProcess;

} // namespace


namespace lyx {
namespace graphics {

class PreviewLoader::Impl {
public:
	///
	Impl(PreviewLoader & p, Buffer const & b);
	/// Stop any InProgress items still executing.
	~Impl();
	///
	PreviewImage const * preview(docstring const & latex_snippet) const;
	///
	PreviewLoader::Status status(docstring const & latex_snippet) const;
	///
	void add(docstring const & latex_snippet);
	///
	void remove(docstring const & latex_snippet);
	/// \p wait whether to wait for the process to complete or, instead,
	/// to do it in the background.
	void startLoading(bool wait = false);
	///
	void refreshPreviews();

	/// Emit this signal when an image is ready for display.
	signal<void(PreviewImage const &)> imageReady;

	Buffer const & buffer() const { return buffer_; }

	lyx::Converter const * setConverter(string const & from);

private:
	/// Called by the ForkedCall process that generated the bitmap files.
	void finishedGenerating(pid_t, int);
	///
	void dumpPreamble(otexstream &, Flavor) const;
	///
	void dumpData(odocstream &, BitmapFile const &) const;

	/** cache_ allows easy retrieval of already-generated images
	 *  using the LaTeX snippet as the identifier.
	 */
	typedef std::shared_ptr<PreviewImage> PreviewImagePtr;
	///
	typedef map<docstring, PreviewImagePtr> Cache;
	///
	Cache cache_;

	/** pending_ stores the LaTeX snippets in anticipation of them being
	 *  sent to the converter.
	 */
	PendingSnippets pending_;

	/** in_progress_ stores all forked processes so that we can proceed
	 *  thereafter.
	 */
	InProgressProcesses in_progress_;

	///
	PreviewLoader & parent_;
	///
	Buffer const & buffer_;
	///
	mutable int font_scaling_factor_;
	///
	mutable int fg_color_;
	///
	mutable int bg_color_;
	///
	QTimer * delay_refresh_;
	///
	bool finished_generating_;

	/// We don't own this
	static lyx::Converter const * pconverter_;

};


lyx::Converter const * PreviewLoader::Impl::pconverter_;


//
// The public interface, defined in PreviewLoader.h
//

PreviewLoader::PreviewLoader(Buffer const & b)
	: pimpl_(make_shared<Impl>(*this, b))
{}


PreviewImage const * PreviewLoader::preview(docstring const & latex_snippet) const
{
	return pimpl_->preview(latex_snippet);
}


PreviewLoader::Status PreviewLoader::status(docstring const & latex_snippet) const
{
	return pimpl_->status(latex_snippet);
}


void PreviewLoader::add(docstring const & latex_snippet) const
{
	pimpl_->add(latex_snippet);
}


void PreviewLoader::remove(docstring const & latex_snippet) const
{
	pimpl_->remove(latex_snippet);
}


void PreviewLoader::startLoading(bool wait) const
{
	pimpl_->startLoading(wait);
}


void PreviewLoader::refreshPreviews()
{
	pimpl_->refreshPreviews();
}


connection PreviewLoader::connect(slot const & slot) const
{
	return pimpl_->imageReady.connect(slot);
}


void PreviewLoader::emitSignal(PreviewImage const & pimage) const
{
	pimpl_->imageReady(pimage);
}


Buffer const & PreviewLoader::buffer() const
{
	return pimpl_->buffer();
}

} // namespace graphics
} // namespace lyx


// The details of the Impl
// =======================

namespace {

class IncrementedFileName {
public:
	IncrementedFileName(string const & to_format,
			    string const & filename_base)
		: to_format_(to_format), base_(filename_base), counter_(1)
	{}

	SnippetPair const operator()(docstring const & snippet)
	{
		ostringstream os;
		os << base_ << counter_++ << '.' << to_format_;
		string const file_name = os.str();
		return make_pair(snippet, FileName(file_name));
	}

private:
	string const & to_format_;
	string const & base_;
	int counter_;
};


InProgress::InProgress(string const & filename_base,
		       PendingSnippets const & pending,
		       string const & to_format)
	: metrics_file(filename_base + ".metrics"),
	  snippets(pending.size()), pid(0)
{
	PendingSnippets::const_iterator pit  = pending.begin();
	PendingSnippets::const_iterator pend = pending.end();
	BitmapFile::iterator sit = snippets.begin();

	transform(pit, pend, sit,
		       IncrementedFileName(to_format, filename_base));
}


void InProgress::stop() const
{
	if (pid)
		ForkedCallsController::kill(pid, 0);

	if (!metrics_file.empty())
		metrics_file.removeFile();

	BitmapFile::const_iterator vit  = snippets.begin();
	BitmapFile::const_iterator vend = snippets.end();
	for (; vit != vend; ++vit) {
		if (!vit->second.empty())
			vit->second.removeFile();
	}
}

} // namespace


namespace lyx {
namespace graphics {

PreviewLoader::Impl::Impl(PreviewLoader & p, Buffer const & b)
	: parent_(p), buffer_(b), finished_generating_(true)
{
	font_scaling_factor_ = int(buffer_.fontScalingFactor());
	if (theApp()) {
		fg_color_ = convert(theApp()->hexName(foregroundColor()), 16);
		bg_color_ = convert(theApp()->hexName(backgroundColor()), 16);
	} else {
		fg_color_ = 0x0;
		bg_color_ = 0xffffff;
	}
	if (!pconverter_)
		pconverter_ = setConverter("lyxpreview");

	delay_refresh_ = new QTimer(&parent_);
	delay_refresh_->setSingleShot(true);
	QObject::connect(delay_refresh_, SIGNAL(timeout()),
	                 &parent_, SLOT(refreshPreviews()));
}


lyx::Converter const * PreviewLoader::Impl::setConverter(string const & from)
{
	typedef vector<string> FmtList;
	FmtList const & loadableFormats = graphics::Cache::get().loadableFormats();
	FmtList::const_iterator it = loadableFormats.begin();
	FmtList::const_iterator const end = loadableFormats.end();

	for (; it != end; ++it) {
		string const to = *it;
		if (from == to)
			continue;

		lyx::Converter const * ptr = lyx::theConverters().getConverter(from, to);
		if (ptr)
			return ptr;
	}

	// Show the error only once. This is thread-safe.
	static nullptr_t no_conv = [&]{
		LYXERR0("PreviewLoader::startLoading()\n"
		        << "No converter from \"" << from
		        << "\" format has been defined.");
		return nullptr;
	} ();

	return no_conv;
}


PreviewLoader::Impl::~Impl()
{
	delete delay_refresh_;

	InProgressProcesses::iterator ipit  = in_progress_.begin();
	InProgressProcesses::iterator ipend = in_progress_.end();

	for (; ipit != ipend; ++ipit)
		ipit->second.stop();
}


PreviewImage const *
PreviewLoader::Impl::preview(docstring const & latex_snippet) const
{
	int fs = int(buffer_.fontScalingFactor());
	int fg = 0x0;
	int bg = 0xffffff;
	if (theApp()) {
		fg = convert(theApp()->hexName(foregroundColor()), 16);
		bg = convert(theApp()->hexName(backgroundColor()), 16);
	}
	if (font_scaling_factor_ != fs || fg_color_ != fg || bg_color_ != bg) {
		// Schedule refresh of all previews on zoom or color changes.
		// The previews are regenerated only after the zoom factor
		// has not been changed for about 1 second.
		fg_color_ = fg;
		bg_color_ = bg;
		delay_refresh_->start(1000);
	}
	// Don't try to access the cache until we are done.
	if (delay_refresh_->isActive() || !finished_generating_)
		return nullptr;

	Cache::const_iterator it = cache_.find(latex_snippet);
	return (it == cache_.end()) ? nullptr : it->second.get();
}


void PreviewLoader::Impl::refreshPreviews()
{
	font_scaling_factor_ = int(buffer_.fontScalingFactor());
	// Reschedule refresh until the previous process completed.
	if (!finished_generating_) {
		delay_refresh_->start(1000);
		return;
	}
	Cache::const_iterator cit = cache_.begin();
	Cache::const_iterator cend = cache_.end();
	while (cit != cend)
		parent_.remove((cit++)->first);
	finished_generating_ = false;
	buffer_.updatePreviews();
}


namespace {

std::function<bool (InProgressProcess const &)> FindSnippet(docstring const & s)
{
	return [&s](InProgressProcess const & process) {
		BitmapFile const & snippets = process.second.snippets;
		BitmapFile::const_iterator beg  = snippets.begin();
		BitmapFile::const_iterator end = snippets.end();
		return find_if(beg, end, FindFirst(s)) != end;
	};
}

} // namespace

PreviewLoader::Status
PreviewLoader::Impl::status(docstring const & latex_snippet) const
{
	Cache::const_iterator cit = cache_.find(latex_snippet);
	if (cit != cache_.end())
		return Ready;

	PendingSnippets::const_iterator pit  = pending_.begin();
	PendingSnippets::const_iterator pend = pending_.end();

	pit = find(pit, pend, latex_snippet);
	if (pit != pend)
		return InQueue;

	InProgressProcesses::const_iterator ipit  = in_progress_.begin();
	InProgressProcesses::const_iterator ipend = in_progress_.end();

	ipit = find_if(ipit, ipend, FindSnippet(latex_snippet));
	if (ipit != ipend)
		return Processing;

	return NotFound;
}


void PreviewLoader::Impl::add(docstring const & latex_snippet)
{
	if (!pconverter_ || status(latex_snippet) != NotFound)
		return;

	docstring const snippet = trim(latex_snippet);
	if (snippet.empty())
		return;

	LYXERR(Debug::GRAPHICS, "adding snippet:\n" << snippet);

	pending_.push_back(snippet);
}


namespace {

std::function<void (InProgressProcess &)> EraseSnippet(docstring const & s)
{
	return [&s](InProgressProcess & process) {
		BitmapFile & snippets = process.second.snippets;
		BitmapFile::iterator it  = snippets.begin();
		BitmapFile::iterator end = snippets.end();

		it = find_if(it, end, FindFirst(s));
		if (it != end)
			snippets.erase(it, it+1);
	};
}

} // namespace


void PreviewLoader::Impl::remove(docstring const & latex_snippet)
{
	Cache::iterator cit = cache_.find(latex_snippet);
	if (cit != cache_.end())
		cache_.erase(cit);

	PendingSnippets::iterator pit  = pending_.begin();
	PendingSnippets::iterator pend = pending_.end();

	pending_.erase(std::remove(pit, pend, latex_snippet), pend);

	InProgressProcesses::iterator ipit  = in_progress_.begin();
	InProgressProcesses::iterator ipend = in_progress_.end();

	for_each(ipit, ipend, EraseSnippet(latex_snippet));

	while (ipit != ipend) {
		InProgressProcesses::iterator curr = ipit++;
		if (curr->second.snippets.empty())
			in_progress_.erase(curr);
	}
}


void PreviewLoader::Impl::startLoading(bool wait)
{
	if (pending_.empty() || !pconverter_)
		return;

	// Only start the process off after the buffer is loaded from file.
	if (!buffer_.isFullyLoaded())
		return;

	LYXERR(Debug::GRAPHICS, "PreviewLoader::startLoading()");

	// As used by the LaTeX file and by the resulting image files
	FileName const directory(buffer_.temppath());

	FileName const latexfile = unique_tex_filename(directory);
	string const filename_base = removeExtension(latexfile.absFileName());

	// Create an InProgress instance to place in the map of all
	// such processes if it starts correctly.
	InProgress inprogress(filename_base, pending_, pconverter_->to());

	// clear pending_, so we're ready to start afresh.
	pending_.clear();

	// Output the LaTeX file.
	// we use the encoding of the buffer
	Encoding const & enc = buffer_.params().encoding();
	ofdocstream of;
	try { of.reset(enc.iconvName()); }
	catch (iconv_codecvt_facet_exception const & e) {
		LYXERR0("Caught iconv exception: " << e.what()
			<< "\nUnable to create LaTeX file: " << latexfile);
		return;
	}

	otexstream os(of);
	if (!openFileWrite(of, latexfile))
		return;

	if (!of) {
		LYXERR(Debug::GRAPHICS, "PreviewLoader::startLoading()\n"
					<< "Unable to create LaTeX file\n" << latexfile);
		return;
	}
	of << "\\batchmode\n";

	LYXERR(Debug::OUTFILE, "Format = " << buffer_.params().getDefaultOutputFormat());
	string latexparam = "";
	bool docformat = !buffer_.params().default_output_format.empty()
			&& buffer_.params().default_output_format != "default";
	// Use LATEX flavor if the document does not specify a specific
	// output format (see bug 9371).
	Flavor flavor = docformat
					? buffer_.params().getOutputFlavor()
					: Flavor::LaTeX;
	if (buffer_.params().encoding().package() == Encoding::japanese) {
		latexparam = " --latex=platex";
		flavor = Flavor::LaTeX;
	}
	else if (buffer_.params().useNonTeXFonts) {
		if (flavor == Flavor::LuaTeX)
			latexparam = " --latex=lualatex";
		else {
			flavor = Flavor::XeTeX;
			latexparam = " --latex=xelatex";
		}
	}
	else {
		switch (flavor) {
			case Flavor::PdfLaTeX:
				latexparam = " --latex=pdflatex";
				break;
			case Flavor::XeTeX:
				latexparam = " --latex=xelatex";
				break;
			case Flavor::LuaTeX:
				latexparam = " --latex=lualatex";
				break;
			case Flavor::DviLuaTeX:
				latexparam = " --latex=dvilualatex";
				break;
			default:
				flavor = Flavor::LaTeX;
		}
	}
	dumpPreamble(os, flavor);
	// handle inputenc etc.
	// I think this is already handled by dumpPreamble(): Kornel
	// buffer_.params().writeEncodingPreamble(os, features);
	of << "\n\\begin{document}\n";
	dumpData(of, inprogress.snippets);
	of << "\n\\end{document}\n";
	of.close();
	if (of.fail()) {
		LYXERR(Debug::GRAPHICS, "PreviewLoader::startLoading()\n"
					 << "File was not closed properly.");
		return;
	}

	// The conversion command.
	ostringstream cs;
	cs << subst(pconverter_->command(), "$${python}", os::python())
	   << " " << quoteName(latexfile.toFilesystemEncoding())
	   << " --dpi " << font_scaling_factor_;

	// FIXME XHTML
	// The colors should be customizable.
	if (!buffer_.isExporting()) {
		ColorCode const fg = PreviewLoader::foregroundColor();
		ColorCode const bg = PreviewLoader::backgroundColor();
		cs << " --fg " << theApp()->hexName(fg)
		   << " --bg " << theApp()->hexName(bg);
	}

	cs << latexparam;
	cs << " --bibtex=" << quoteName(buffer_.params().bibtexCommand());
	if (buffer_.params().bufferFormat() == "lilypond-book")
		cs << " --lilypond";

	string const command = cs.str();

	if (wait) {
		ForkedCall call(buffer_.filePath(), buffer_.layoutPos());
		int ret = call.startScript(ForkedProcess::Wait, command);
		// PID_MAX_LIMIT is 2^22 so we start one after that
		static atomic_int fake((1 << 22) + 1);
		int pid = fake++;
		inprogress.pid = pid;
		inprogress.command = command;
		in_progress_[pid] = inprogress;
		finishedGenerating(pid, ret);
		return;
	}

	// Initiate the conversion from LaTeX to bitmap images files.
	ForkedCall::sigPtr convert_ptr = make_shared<ForkedCall::sig>();
	weak_ptr<PreviewLoader::Impl> this_ = parent_.pimpl_;
	convert_ptr->connect([this_](pid_t pid, int retval){
			if (auto p = this_.lock()) {
				p->finishedGenerating(pid, retval);
			}
		});

	ForkedCall call(buffer_.filePath());
	int ret = call.startScript(command, convert_ptr);

	if (ret != 0) {
		LYXERR(Debug::GRAPHICS, "PreviewLoader::startLoading()\n"
					<< "Unable to start process\n" << command);
		return;
	}

	// Store the generation process in a list of all such processes
	inprogress.pid = call.pid();
	inprogress.command = command;
	in_progress_[inprogress.pid] = inprogress;
}


double PreviewLoader::displayPixelRatio() const
{
	return buffer().params().display_pixel_ratio;
}

void PreviewLoader::Impl::finishedGenerating(pid_t pid, int retval)
{
	// Paranoia check!
	InProgressProcesses::iterator git = in_progress_.find(pid);
	if (git == in_progress_.end()) {
		lyxerr << "PreviewLoader::finishedGenerating(): unable to find "
			"data for PID " << pid << endl;
		finished_generating_ = true;
		return;
	}

	string const command = git->second.command;
	string const status = retval > 0 ? "failed" : "succeeded";
	LYXERR(Debug::GRAPHICS, "PreviewLoader::finishedInProgress("
				<< retval << "): processing " << status
				<< " for " << command);
	if (retval > 0) {
		in_progress_.erase(git);
		finished_generating_ = true;
		return;
	}

	// Read the metrics file, if it exists
	vector<double> ascent_fractions(git->second.snippets.size());
	setAscentFractions(ascent_fractions, git->second.metrics_file);

	// Add these newly generated bitmap files to the cache and
	// start loading them into LyX.
	BitmapFile::const_iterator it  = git->second.snippets.begin();
	BitmapFile::const_iterator end = git->second.snippets.end();

	list<PreviewImagePtr> newimages;

	size_t metrics_counter = 0;
	for (; it != end; ++it, ++metrics_counter) {
		docstring const & snip = it->first;
		FileName const & file = it->second;
		double af = ascent_fractions[metrics_counter];

		// Add the image to the cache only if it's actually present
		// and not empty (an empty image is signaled by af < 0)
		if (af >= 0 && file.isReadableFile()) {
			PreviewImagePtr ptr(new PreviewImage(parent_, snip, file, af));
			cache_[snip] = ptr;

			newimages.push_back(ptr);
		}

	}

	// Remove the item from the list of still-executing processes.
	in_progress_.erase(git);

#if 0
	/* FIXME : there is no need for all these calls, which recompute
	 * all metrics for each and every preview. The single call at the
	 * end of this method is sufficient.

	 * It seems that this whole imageReady mechanism is actually not
	 * needed. If it is the case, the whole updateFrontend/updateInset
	 * bloat can go too.
	 */

	// Tell the outside world
	list<PreviewImagePtr>::const_reverse_iterator
		nit  = newimages.rbegin();
	list<PreviewImagePtr>::const_reverse_iterator
		nend = newimages.rend();
	for (; nit != nend; ++nit) {
		imageReady(*nit->get());
	}
#endif

	finished_generating_ = true;
	buffer_.scheduleRedrawWorkAreas();
}


void PreviewLoader::Impl::dumpPreamble(otexstream & os, Flavor flavor) const
{
	// Dump the preamble only.
	LYXERR(Debug::OUTFILE, "dumpPreamble, flavor == " << static_cast<int>(flavor));
	OutputParams runparams(&buffer_.params().encoding());
	runparams.flavor = flavor;
	runparams.nice = true;
	runparams.moving_arg = true;
	runparams.free_spacing = true;
	runparams.is_child = buffer_.parent();
	runparams.for_preview = true;
	buffer_.writeLaTeXSource(os, buffer_.filePath(), runparams, Buffer::OnlyPreamble);

	// FIXME! This is a HACK! The proper fix is to control the 'true'
	// passed to TeXMathStream below:
	// int InsetMathNest::latex(Buffer const &, odocstream & os,
	//                          OutputParams const & runparams) const
	// {
	//	TeXMathStream wi(os, runparams.moving_arg, true);
	//	par_->write(wi);
	//	return wi.line();
	// }
	os << "\n"
	   << "\\def\\lyxlock{}\n"
	   << "\n";

	// All equation labels appear as "(#)" + preview.sty's rendering of
	// the label name
	if (lyxrc.preview_hashed_labels)
		os << "\\renewcommand{\\theequation}{\\#}\n";

	// Use the preview style file to ensure that each snippet appears on a
	// fresh page.
	// Also support PDF output (automatically generated e.g. when
	// \usepackage[pdftex]{hyperref} is used and XeTeX.
	os << "\n"
	   << "\\usepackage[active,delayed,showlabels,lyx]{preview}\n"
	   << "\n";
}


void PreviewLoader::Impl::dumpData(odocstream & os,
				   BitmapFile const & vec) const
{
	if (vec.empty())
		return;

	BitmapFile::const_iterator it  = vec.begin();
	BitmapFile::const_iterator end = vec.end();

	Encoding const & enc = buffer_.params().encoding();

	for (; it != end; ++it) {
		bool uncodable_content = false;
		// check whether the content is encodable
		// FIXME: the preview loader should be able
		//        to handle multiple encodings
		//        or we should generally use utf8
		for (char_type n : it->first) {
			if (!enc.encodable(n)) {
				LYXERR0("Uncodable character '"
					<< docstring(1, n)
					<< "' in preview snippet!");
				uncodable_content = true;
				break;
			}
		}
		os << "\\begin{preview}\n";
		// do not show incomplete preview
		if (!uncodable_content)
			os << it->first;
		os << "\n\\end{preview}\n\n";
	}
}

} // namespace graphics
} // namespace lyx

#include "moc_PreviewLoader.cpp"
