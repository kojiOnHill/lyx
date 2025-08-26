/**
 * \file GuiInputMethod.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>
#include <array>
#include <ios>

#include "GuiInputMethod.h"

#include "Buffer.h"
#include "ColorCache.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "GuiApplication.h"
#include "GuiCompleter.h"
#include "GuiFontLoader.h"
#include "GuiWorkArea.h"
#include "InsetList.h"
#include "mathed/InsetMathMacro.h"
#include "KeyMap.h"
#include "Language.h"
#include "LyX.h"
#include "LyXRC.h"
#include "Row.h"
#include "Text.h"
#include "TextMetrics.h"

#include "support/debug.h"
#include "support/lassert.h"
#include "support/qstring_helpers.h"

using namespace std;

namespace lyx {

namespace frontend {

struct GuiInputMethod::Private
{
	BufferView * buffer_view_;
	GuiWorkArea * work_area_;
	/// the current preedit text of the input method
	docstring preedit_str_ = from_utf8("");
	/// the non-virtual cursor the position of which is the starting point of
	/// the preedits
	Cursor * cur_;
	/// if undergoing preedit input is overriding a selection
	bool has_selection_ = false;
	///
	bool has_multiple_rows_;

	QInputMethod * sys_im_ = guiApp->inputMethod();
	QLocale locale_;
	/// if language should not use preedit in the math mode
	bool im_off_in_math_ = false;

	ColorCache & color_cache_ = guiApp->colorCache();
	QColor font_color_;
	QBrush font_brush_;

	ParagraphMetrics * pm_ptr_ = nullptr;

	PreeditStyle style_;
	std::vector<PreeditSegment> seg_turnout_;

	InputMethodState im_state_;

	Rows::iterator rows_;
	size_type rows_size_;

	pos_type cur_pos_ = 0;
	pos_type cur_row_idx_;
	pos_type caret_pos_;

	pos_type anchor_pos_ = 0;
	pos_type abs_pos_;

	bool real_boundary_    = false;
	bool virtual_boundary_ = false;
	bool initial_tf_entry_ = false;

	Point init_point_;
	Dimension cur_dim_;
	/// pixel offset of the caret within preedit text
	std::array<int, 2> caret_offset_ = {0, 0};
};


GuiInputMethod::GuiInputMethod(GuiWorkArea *parent)
	: QObject(parent), d(new Private)
{
	d->work_area_ = parent;
	d->buffer_view_ = &d->work_area_->bufferView();
	d->cur_ = &d->buffer_view_->cursor();

	LYXERR(Debug::DEBUG, "GuiInputMethod: Address of parent: " << parent);
	LYXERR(Debug::DEBUG, "GuiInputMethod: Address of buffer_view_: " <<
	       &d->work_area_->bufferView());

	connect(guiApp, &GuiApplication::acceptsInputMethod,
	        this, &GuiInputMethod::toggleInputMethodAcceptance);
	connect(this, &GuiInputMethod::inputMethodStateChanged,
	        d->sys_im_, &QInputMethod::update);
	connect(d->sys_im_, &QInputMethod::localeChanged,
	        this, &GuiInputMethod::onLocaleChanged);
	connect(this, &GuiInputMethod::cursorPositionChanged,
	        this, &GuiInputMethod::onCursorPositionChanged);

	// initialize locale status
	onLocaleChanged();
}

GuiInputMethod::~GuiInputMethod()
{
	delete d;
}


void GuiInputMethod::toggleInputMethodAcceptance(){
	if (guiApp->platformName() != "cocoa")
		// Since QInputMethod::locale() doesn't work on other systems, use
		// language at the cursor point to infer the current input language
		// as a second best
		d->im_off_in_math_ =
		        d->cur_->getFont().language()->inputMethodOffInMath();

	// note that d->cur_->inset() is a cache so it lags from actual key moves
	bool inMathMode = d->cur_->inset().currentMode() == Inset::MATH_MODE;
	bool inMathMacro = d->cur_->inset().asInsetMath() != nullptr &&
	                   d->cur_->inset().asInsetMath()->asMacroInset() != nullptr;
	bool enabled =
	        !((d->im_off_in_math_ && (inMathMode || inMathMacro)) ||
	          guiApp->isInCommandMode());
	if (enabled != d->im_state_.enabled_) {
		d->im_state_.enabled_= enabled;
		Q_EMIT inputMethodStateChanged(Qt::ImEnabled);
	}
}


void GuiInputMethod::onLocaleChanged()
{
	const std::string im_locale_code = fromqstr(d->sys_im_->locale().name());
	const Language * lang = languages.getFromCode(im_locale_code);

	d->im_off_in_math_ = lang != nullptr ? lang->inputMethodOffInMath() : false;
}


#ifdef Q_DEBUG
// this is experimental (2024/10/28)
// don't input methods implement ImHints except for Qt::ImhDigitsOnly yet?
void GuiInputMethod::setHint(Hint hint){
	d->work_area_->setInputMethodHints(
	            d->work_area_->inputMethodHints() | (Qt::InputMethodHints)hint);
	Q_EMIT inputMethodStateChanged(Qt::ImHints);
}
#endif // Q_DEBUG

/* *
 * *        Preedit
 * */

void GuiInputMethod::processPreedit(QInputMethodEvent* ev)
{
	// Linux can call this function even when IM is not used
	if (ev->preeditString().isEmpty() && ev->commitString().isEmpty() &&
	        !d->has_selection_ && d->im_state_.preediting_ == false)
		return;

	if (!d->buffer_view_->inlineCompletion().empty())
		d->work_area_->completer().hideInline();

	// prepare for dark mode
	Color fg(Color_foreground);
	d->font_color_ = d->color_cache_.get(fg);
	d->font_brush_.setColor(d->font_color_);
	LYXERR(Debug::DEBUG,
	       "Preedit font color is set to " << d->font_color_.name());

	d->locale_ = d->sys_im_->locale();
	d->style_.lang_ = d->locale_;

	LYXERR(Debug::DEBUG,
	       "IM locale: " << d->locale_.bcp47Name() <<
	       " preeditString: " << ev->preeditString() <<
	       " commitString: " << ev->commitString());

	// output the commit string to the text
	if (!ev->commitString().isEmpty()) {
		// take care of commit string assigned to a shortcut
		// e.g. quotation mark on international keyboard
		KeySequence keyseq;
		for (QChar const & ch : ev->commitString()) {
			KeySymbol keysym;
			keysym.init(ch.unicode());
			keyseq.addkey(keysym, NoModifier);
		}
		FuncRequest cmd = theTopLevelKeymap().getBinding(keyseq);

		if (cmd == FuncRequest::noaction || cmd == FuncRequest::unknown
		        || cmd.action() == LFUN_SELF_INSERT)
			// insert the processed text in the document (handles undo)
			cmd = FuncRequest(LFUN_SELF_INSERT,
			                  qstring_to_ucs4(ev->commitString()));

		cmd.setOrigin(FuncRequest::KEYBOARD);
		lyx::dispatch(cmd);
	}

	// if selected
	if (d->cur_->selection() && !ev->preeditString().isEmpty()) {
		d->has_selection_ = true;
		d->cur_->beginUndoGroup();
		d->cur_->recordUndoSelection();
		cap::cutSelection(*d->cur_, false);
		d->cur_->endUndoGroup();
	}

	// if preedit is cancelled
	if (ev->preeditString().isEmpty() && ev->commitString().isEmpty() &&
	        d->has_selection_ && d->im_state_.preediting_ == true) {
		d->cur_->beginUndoGroup();
		d->cur_->undoAction();
		d->cur_->endUndoGroup();
		d->has_selection_ = false;
		d->im_state_.preediting_ = false;
	}

	d->preedit_str_ = qstring_to_ucs4(ev->preeditString());

	// parse the input method event attributes
	setPreeditStyle(ev->attributes());

	// initialize related position, par, rows and boundaries
	d->cur_row_idx_ = initializePositions(d->cur_);
	// initialize virtual caret to the anchor (real cursor) position
	d->init_point_ =
	        initializeCaretCoords(d->cur_row_idx_,
	                              d->real_boundary_ && !d->im_state_.composing_mode_);

	// Push preedit texts into row elements, which can shift the anchor
	// point of the preedit texts in a centered or right-flushed row.
	// Check such a shift immediately below.
	updateMetrics(d->cur_);

	/*
	 *          Draw caret
	 */

	// set offset of the virtual preedit caret from the real position
	d->caret_offset_ = setCaretOffset(d->caret_pos_);

	// set graphical geometry of the caret
	d->im_state_.cursor_rect_.setCoords(
	            d->init_point_.x + d->caret_offset_[0],
	            d->init_point_.y + d->caret_offset_[1],
	            d->init_point_.x + d->caret_offset_[0] + d->cur_dim_.width(),
	            d->init_point_.y + d->caret_offset_[1] + d->cur_dim_.height());
	// while preedit exists, this is just coords of real cursor
	d->im_state_.anchor_rect_.setCoords(
	            d->init_point_.x,
	            d->init_point_.y,
	            d->init_point_.x + d->cur_dim_.width(),
	            d->init_point_.y + d->cur_dim_.height());

	// if preedit string is not empty, we are still working on it
	d->im_state_.preediting_ = d->preedit_str_.empty() ? false : true;

	// notify the completion to both im and app itself
	Q_EMIT inputMethodStateChanged(Qt::ImQueryInput);
	Q_EMIT preeditProcessed(ev);
}


void GuiInputMethod::onCursorPositionChanged()
{
	d->cur_pos_ = d->cur_->top().pos();
	d->anchor_pos_ = d->cur_->realAnchor().pos();
	setSurroundingText(*d->cur_);
}


void GuiInputMethod::setPreeditStyle(
        const QList<QInputMethodEvent::Attribute> & attr)
{
	d->style_.segments_.clear();

	const QInputMethodEvent::Attribute * focus_style = nullptr;

#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	d->initial_tf_entry_ = true;
#endif

	// brushes to draw each segment
	QBrush brush[4];
	// foreground of focused segment
	brush[0].setColor(guiApp->colorCache().get(Color_preeditfocustext));
	// background of focused segment
	brush[1].setColor(guiApp->colorCache().get(Color_preeditfocus));
	// foreground of other segments
	brush[2].setColor(guiApp->colorCache().get(Color_preedittext));
	// background of other segments
	brush[3].setColor(guiApp->colorCache().get(Color_preeditbg));
	// make pattern solid
	brush[0].setStyle(Qt::SolidPattern);
	brush[1].setStyle(Qt::SolidPattern);
	brush[2].setStyle(Qt::SolidPattern);
	brush[3].setStyle(Qt::SolidPattern);

	// next char position to be set up the preedit style
	pos_type next_seg_pos = 0;
	//
	d->seg_turnout_.clear();

	LYXERR(Debug::GUI, "Start parsing attributes of the preedit");
	// obtain attributes of input method
	for (const QInputMethodEvent::Attribute & it : attr) {

		switch (it.type) {
		case QInputMethodEvent::TextFormat:
			// Explanation on attributes of QInputMethodEvent:
			//     https://doc.qt.io/archives/qt-5.15/qinputmethodevent.html
			//
			// CJK and European languages that use deadkeys use preedit strings.
			// Japanese language tends to use longer preedit strings so that
			// sorting of their text format attributes is generally required.
			// Even though the above documentation does not exclude random
			// arrival of any number of sections in the communication with the
			// input method, there is observed regularity in its arrival and the
			// number of sections used is at most three.
			//
			// Moreover, MacOS has a peculiarity that the text format attribute
			// for the focused section arrives twice, whose behavior is
			// undefined in the above documentation.
			//
			// Typical observed pattern on the completing stage is as follows.
			// Assuming that the second section is a focused one, the order of
			// the attributes is:
			//
			//     Linux       : 2 1   3
			//     MacOS (Qt6) : 2 1 2 3 (the first '2' conveys color info)
			//     MacOS (Qt5) :   1 2 3
			//     Windows     :   1 2 3
			//
			// (The number shows the order of sections.)
			//
			// That is, the first attribute is the focused section and other
			// sections follow in order.
			//
			// Preedit strings on the composing stage consists of only one
			// section on Qt5. However, MacOS on Qt6 appends a void section
			// with length zero whose position is at the end of the string
			// containing information of the same color as the focused section.
			// We will not use this information but need to prepare for this
			// pattern of information arrival.
			//
			// Whereas observed protocol shows a fixed pattern which we are
			// going to utilize, we need prepare for *any* type of information
			// arrival that satisfies the documented protocol.

			next_seg_pos = setTextFormat(it, next_seg_pos, brush);

			break;

		case QInputMethodEvent::Cursor:
			d->caret_pos_ = setCaretPos(it.start);
			d->style_.caret_visible_ = it.length != 0;

			// colorization of caret is not implemented yet:
			// it will require modification of GuiWorkArea::Private::drawCaret
			// however we haven't encountered use cases in QInputMethodEvent
			d->style_.caret_color_ = it.value.value<QColor>();

			// we only prepare for finding such incoming event
			LYXERR(Debug::DEBUG,
			       "QInputMethodEvent: virtual cursor position: " <<
			       std::dec << d->caret_pos_ << " caret color: " <<
			       d->style_.caret_color_.name(QColor::HexRgb));
			break;

		// again not implemented yet?
		case QInputMethodEvent::Language:
			d->style_.lang_ = it.value.value<QLocale>();
			LYXERR(Debug::DEBUG,
			       "QInputMethodEvent::Language: " <<
			       d->style_.lang_.bcp47Name());
			break;

		// this also seems not implemented by any input methods
		case QInputMethodEvent::Ruby:
			d->style_.ruby_ = it.value.value<QString>();
			LYXERR(Debug::DEBUG,
			       "QInputMethodEvent::Ruby: " << d->style_.ruby_);
			break;

		case QInputMethodEvent::Selection:
			d->anchor_pos_ = it.start;
			d->caret_pos_ = it.start + it.length;
			d->cur_->setSelection(d->cur_->realAnchor(), it.length);
			LYXERR(Debug::DEBUG,
			       "QInputMethodEvent::Selection start: " << it.start <<
			       " length: " << it.length);
			break;

		} // end switch
	} // end for

	// Finalize TextFormat: sweep all remaining turnouts
	for (size_type i=0; i<d->seg_turnout_.size(); ++i)
		next_seg_pos = pickNextSegFromTurnout(next_seg_pos);
	if (!d->seg_turnout_.empty()) {
		LYXERR0("Turnouts of preedit segments have not been all swept");
		LATTEST(false);
	}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	// set background color for a focused segment
	if (!d->style_.segments_.empty()) {
		QTextCharFormat & focus_char_format =
		        d->style_.segments_[focusedSegmentIndex()].char_format_;
		if (d->style_.segments_.size() > 1) {
			QBrush fgbrush(guiApp->colorCache().get(Color_preeditfocustext));
			QBrush bgbrush(guiApp->colorCache().get(Color_preeditfocus));
			focus_char_format.setForeground(fgbrush);
			focus_char_format.setBackground(bgbrush);
		}
	}
#endif // QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

	// composing mode: it has no focused segments
	// composing mode in Qt version 6 or later gives focus_style->length == 0 and
	// focus_style->start > d->preedit_str_.length() on MacOS (undocumented)
	d->im_state_.composing_mode_ =
	        (d->style_.segments_.size() == 0 && focus_style != nullptr)
	        || (focus_style != nullptr && focus_style->length == 0)
	        || (focus_style == nullptr && d->style_.segments_.size() <= 1);

	if (d->im_state_.composing_mode_) {
		LYXERR(Debug::DEBUG, "preedit is in the composing mode");
		QTextCharFormat char_format;
		int start = 0;
		size_type length = 0;
		if (focus_style != nullptr && focus_style->length != 0) {
			char_format = focus_style->value.value<QTextCharFormat>();
			char_format.setFontUnderline(true);
			start = focus_style->start;
			length = (size_type)focus_style->length;
		} else if (!d->im_off_in_math_ && !d->preedit_str_.empty() &&
		           d->style_.segments_.empty()) {
			// dead keys on MacOS give no char format having preedit strings
			length = d->preedit_str_.length();
		} else
			return;
		conformToSurroundingFont(char_format);
		PreeditSegment seg = {start, length, char_format};
		d->style_.segments_.push_back(seg);
	} else {
		LYXERR(Debug::DEBUG, "preedit is in the completing mode");
		d->style_.caret_visible_ = false;
	}
}

pos_type GuiInputMethod::setTextFormat(const QInputMethodEvent::Attribute & it,
                                       pos_type next_seg_pos, const QBrush brush[])
{
	// get LyX's color setting
	QTextCharFormat char_format = it.value.value<QTextCharFormat>();

	LYXERR(Debug::GUI,
	       "QInputMethodEvent::TextFormat start: " << it.start <<
	       " end: " << it.start + it.length - 1 <<
	       " underline? " << char_format.font().underline() <<
	       " UnderlineStyle: " << char_format.underlineStyle() <<
	       " fg: " << char_format.foreground().color().name() <<
	       " bg: " << char_format.background().color().name());

	//
	// Fit and adjust arrived text formats
	//
	conformToSurroundingFont(char_format);
	// QLocale is used for wrapping words
	char_format.setProperty(QMetaType::QLocale, d->style_.lang_);
#if (! defined(Q_OS_MACOS)) || QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	char_format.setFontUnderline(true);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	// set the style of a focused sector as specified by color themes
	// it uses a system color if "Use system colors" is checked
	// NOTE: brush size is assumed to be four
	//       if it is to be changed, change the declaration of array in
	//       setPreeditStyle()
	if (char_format.background().color() != QColorConstants::Black) {
		char_format.setForeground(brush[0]);
		char_format.setBackground(brush[1]);
	} else {
		char_format.setForeground(brush[2]);
		char_format.setBackground(brush[3]);
	}
#else
	// avoid unused-parameter warning.
	(void) brush;
#endif // QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

	// The void "cursor segment" in the composing mode comes with it.start > 0 and
	// it.length == 0, whose info we don't use, so it does not go in if's below
	if (it.start == next_seg_pos && !d->initial_tf_entry_) {
		if (!d->seg_turnout_.empty()) {
			// Merge attributes held d->seg_turnout_
			pos_type updated_pos =
			        pickNextSegFromTurnout(next_seg_pos, &char_format);
			if (updated_pos == next_seg_pos) {
				// no matching segment in the turnout
				LYXERR(Debug::GUI, "Pushing to preedit register: (" << it.start
				       << ", " << it.start + it.length - 1 << ") bg color: "
				       << char_format.background().color().name());
				next_seg_pos = registerSegment(it.start, (size_type)it.length,
				                               char_format);
			} else
				next_seg_pos = updated_pos;
		} else {
			// push the constructed char format together with start and length
			// to the list
			LYXERR(Debug::GUI, "Pushing to preedit register: (" << it.start
			       << ", " << it.start + it.length - 1
			       << ") fg: "
		           << char_format.foreground().color().name()
			       << " bg: "
			       << char_format.background().color().name());
			next_seg_pos =
			        registerSegment(it.start, (size_type)it.length, char_format);
		}
		next_seg_pos = pickNextSegFromTurnout(next_seg_pos);
	} else if ((it.start > next_seg_pos || d->initial_tf_entry_) && it.length > 0) {
		LYXERR(Debug::GUI, "Pushing to preedit turnout:  (" << it.start << ", "
				<< it.start + it.length - 1 << ")");
		PreeditSegment turnout = {it.start, (size_type)it.length, char_format};
		d->seg_turnout_.push_back(turnout);
		d->initial_tf_entry_ = false;
	}
	return next_seg_pos;
}


pos_type GuiInputMethod::pickNextSegFromTurnout(pos_type next_seg_pos,
                                                QTextCharFormat * cf) {
	std::vector<PreeditSegment>::iterator to_erase;
	bool is_matched = false;

	// we prepare "multiple tracks" in d->seg_turnout_,
	// but typically only one is used
	for (auto past_attr = d->seg_turnout_.begin();
	     past_attr != d->seg_turnout_.end(); past_attr++) {
		if ((*past_attr).start_ == next_seg_pos) {
			PreeditSegment seg;
			if (cf != nullptr) {
				cf->merge((*past_attr).char_format_);
				seg = {(*past_attr).start_,
								  (size_type)(*past_attr).length_, *cf};
			} else
				seg = {(*past_attr).start_,
								  (size_type)(*past_attr).length_,
								  (*past_attr).char_format_};
			LYXERR(Debug::GUI,
			       "Pushing to preedit register: (" << (*past_attr).start_
			        << ", " << (*past_attr).start_ + (*past_attr).length_ - 1
			        << ") fg: "
			        << (*past_attr).char_format_.foreground().color().name()
			        << " bg: "
			        << (*past_attr).char_format_.background().color().name());
			d->style_.segments_.push_back(seg);
			next_seg_pos += (*past_attr).length_;
			if (d->seg_turnout_.size() > 1)
				to_erase = past_attr;
			is_matched = true;
			break; // assuming no duplicates in seg_turnout_
		}
	}
	// Clear d->seg_turnout_
	if (is_matched) {
		if (d->seg_turnout_.size() == 1) {
			LYXERR(Debug::GUI, "Preedit turnout clearing:    ("
			        << d->seg_turnout_.back().start_ << ", "
			        << d->seg_turnout_.back().start_
			               + d->seg_turnout_.back().length_ - 1 << ")");
			d->seg_turnout_.pop_back();
		} else if (d->seg_turnout_.size() > 1) {
			LYXERR(Debug::GUI, "Preedit turnout clearing: ("
			        << (*to_erase).start_ << ", "
			        << (*to_erase).start_ + (*to_erase).length_ - 1 << ")");
			d->seg_turnout_.erase(to_erase);
		}
	}

	return next_seg_pos;
}


pos_type GuiInputMethod::registerSegment(pos_type start, size_type length,
                                         QTextCharFormat char_format) {
	PreeditSegment seg = {start, length, char_format};
	d->style_.segments_.push_back(seg);
	return start + length;
}


void GuiInputMethod::conformToSurroundingFont(QTextCharFormat & char_format) {
	// Surrounding font at the cursor position
	d->cur_->setCurrentFont();
	const QFont & surrounding_font =
	        getFont(d->cur_->real_current_font.fontInfo());

	// set font families
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
	if (char_format.fontFamilies().isNull()) {
#else
	if (char_format.fontFamily().isNull()) {
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
		char_format.setFontFamilies(surrounding_font.families());
#else
		// setFontFamily and fontFamily() are deprecated at Qt-6.1 or later
		// they are introduced at Qt 5.13 (and)
		char_format.setFontFamily(surrounding_font.family());
#endif
	}
	// set font size
	if (char_format.fontPointSize() == 0)
		char_format.setFontPointSize(surrounding_font.pointSizeF());

	// set font weight to fit surrounding text
	char_format.setFontWeight(surrounding_font.weight());

	// set font italic to fit surrounding text
	if (!char_format.fontItalic())
		char_format.setFontItalic(surrounding_font.italic());
	else
		char_format.setFontItalic(!surrounding_font.italic());

	// set font color
	if (!lyxrc.use_system_colors &&
	        char_format.foreground().style() == Qt::NoBrush) {
		ColorCache cc;
		char_format.setForeground(
		    cc.get(d->cur_->real_current_font.fontInfo().realColor(), false));
	}

}


/* *
 * *         Drawing
 * */
void GuiInputMethod::setParagraphMetrics(ParagraphMetrics & pm){
	d->pm_ptr_ = &pm;
}

std::array<int,2> GuiInputMethod::setCaretOffset(pos_type caret_pos){

	// Note that preedit elements are virtual and not counted in pos().
	// pos: 0 1 2 3 4 5 6 7 8 | 8 8 8 8 8 8 8 8 8 8 | 9 10 11 ...
	//      <-  non-virtual ->|<- preedit element ->|<- non-virtual
	//
	// This is also true for next_row_pos.
	// On the other hand d->caret_pos_ counts preedit elements.

	if (d->preedit_str_.empty() || d->cur_->reverseDirectionNeeded()) {
		// reset shift of the virtual caret as the preedit string is cancelled
		// this part is also visited right before starting preedit input
		return {0, 0};
	}
	// when preedit cursor is available, string fonts are all common
	// so pick up the last one
	QTextCharFormat qtcf;
	if (!d->style_.segments_.empty())
		qtcf = d->style_.segments_.back().char_format_;
	else
		conformToSurroundingFont(qtcf);
	QFontMetrics qfm(qtcf.font());
	QString str_before_caret = "";

#if defined(Q_OS_MACOS) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	if (d->im_state_.composing_mode_)
		str_before_caret =
		    toqstr(d->preedit_str_.substr(0, caret_pos - d->cur_pos_));
	else
		// adjust for the reported caret position in the completion mode in Qt5
		str_before_caret = toqstr(
		            d->preedit_str_.substr(0, caret_pos - d->cur_pos_ -
		                                   shiftFromCaretToSegmentHead()));
#else
	str_before_caret =
		toqstr(d->preedit_str_.substr(0, caret_pos - d->cur_pos_));
#endif

	// process line wrapping
	//
	// NOTE:
	// preedits added on boundary goes to the beginning of the next line
	//         -> real_boundary = true   virtual_boundary = false
	// preedits added on boundary is appended to the same line
	//         -> real_boundary = false   virtual_boundary = true

	PreeditRow caret_row = getCaretInfo(d->real_boundary_, d->virtual_boundary_);
	std::array<int,2> caret_offset {};

	// caret_row.index doesn't decrease with virtual_boundary
	// has multiple preedit rows
	if (caret_row.index > d->cur_row_idx_ || d->real_boundary_) {
		QString lastline_str;

		if (d->real_boundary_ && caret_row.index == d->cur_row_idx_)
			lastline_str = str_before_caret;
		else
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			lastline_str = str_before_caret.sliced(
			    caret_row.pos - d->cur_pos_,
			    d->cur_pos_ + str_before_caret.length() - caret_row.pos);
#else
			lastline_str = str_before_caret.mid(
			    caret_row.pos - d->cur_pos_,
			    d->cur_pos_ + str_before_caret.length() - caret_row.pos);
#endif
		//
		// calculate left margin
		//
		int left_margin;
		int inset_offset;
		if (d->cur_->depth() > 1) {
			pit_type & outer_pit = d->cur_->bottom().pit();
			TextMetrics & tm =
			        d->buffer_view_->textMetrics(d->cur_->bottom().text());
			left_margin = tm.leftMargin(outer_pit);
			inset_offset = d->cur_->inset().leftOffset(d->buffer_view_);
		} else {
			left_margin = d->rows_[caret_row.index].left_margin;
			inset_offset = 0;
		}
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		if (d->real_boundary_ && !d->im_state_.composing_mode_)
			caret_offset[0] = qfm.horizontalAdvance(lastline_str);
		else
			caret_offset[0] = - d->init_point_.x + left_margin
			        + inset_offset + qfm.horizontalAdvance(lastline_str);
#else
		if (d->real_boundary_ && !d->im_state_.composing_mode_)
			caret_offset[0] = qfm.width(lastline_str);
		else
			caret_offset[0] = - d->init_point_.x + left_margin
			        + inset_offset + qfm.width(lastline_str);
#endif
	} else {
		int left_margin_diff = d->rows_[caret_row.index].left_margin -
		        d->rows_[d->cur_row_idx_].left_margin;
		// the case in which the preedit caret is in the first row of the preedit
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		caret_offset[0] =
		        left_margin_diff + qfm.horizontalAdvance(str_before_caret);
#else
		caret_offset[0] =
		        left_margin_diff + qfm.width(str_before_caret);
#endif
	}

	// vertical offset only applicable to main text
	caret_offset[1] = 0;
	for (pos_type i = d->cur_row_idx_ -
	     (d->real_boundary_ && d->im_state_.composing_mode_);
	     i < caret_row.index -
	     (d->real_boundary_ && !d->im_state_.composing_mode_); ++i)
		caret_offset[1] += d->rows_[i].descent() + d->rows_[i+1].ascent();

	return caret_offset;
}


// returns (x_offset, y_offset) array
std::array<int, 2> GuiInputMethod::preeditCaretOffset() const {
	return d->caret_offset_;
}


// returns whether the caret is visible
bool GuiInputMethod::isCaretVisible() const {
	return d->style_.caret_visible_;
}


#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
// Returns index of the focused segment (just in front of the virtual caret)
pos_type GuiInputMethod::focusedSegmentIndex() {
	pos_type idx = 0;
	// position within the preedit string
	pos_type pos = d->caret_pos_ - d->cur_->pos();

#ifdef Q_OS_MACOS
	// on MacOS, cursor position is the end of the preedit segment
	bool entry_found = false;
	if (!d->style_.segments_.empty() &&
	        d->style_.segments_[0].start_ != pos) {
		for (size_type i = 1; i < d->style_.segments_.size(); i++) {
			if (d->style_.segments_[i].start_ == pos) {
				entry_found = true;
				idx = --i;
				break;
			}
		}
		if (!entry_found) idx = d->style_.segments_.size() - 1;
	}
#else // Q_OS_MACOS
	// on other systems(?), cursor pos is the beginning of the preedit segment
	if (!d->style_.segments_.empty()) {
		for (size_type i = 0; i < d->style_.segments_.size(); i++) {
			if (d->style_.segments_[i].start_ == pos) {
				idx = i;
				break;
			}
		}
	}
#endif // Q_OS_MACOS

	return idx;
}


// pos shift factor from the caret to the selection segment's head
int GuiInputMethod::shiftFromCaretToSegmentHead() {
	if (d->style_.segments_.empty()) {
		return 0;
	} else {
		pos_type focused_segment_idx = focusedSegmentIndex();
		QString segment_string =
		        toqstr(d->preedit_str_.substr(
		                   d->style_.segments_[focused_segment_idx].start_,
		                   d->style_.segments_[focused_segment_idx].length_));
		return segment_string.length();
	}
}
#endif // QT_VERSION < QT_VERSION_CHECK(6, 0, 0)


/* *
 * *        Input Method Query
 * */
void GuiInputMethod::processQuery(Qt::InputMethodQuery query)
{
	// input method is not ready yet
	if (d->buffer_view_->inputMethod() == nullptr) {
		QVariant null_answer;
		Q_EMIT queryProcessed(null_answer);
		return;
	}

	docstring msg = "Responded to query " +
	        inputMethodQueryFlagsAsString(query) + " = ";

	switch (query) {

	// The widget accepts input method input
	case Qt::ImEnabled: {
		if (d->im_state_.enabled_)
			LYXERR(Debug::DEBUG, msg << "true");
		else
			LYXERR(Debug::DEBUG, msg << "false");
		Q_EMIT queryProcessed(d->im_state_.enabled_);
		break;
	}
	// this is the CJK-specific composition window position and
	// the context menu position when the menu key is pressed.
	case Qt::ImCursorRectangle: {
		QRectF * rect;
		if (d->im_state_.composing_mode_) {
			// in the editing mode, cursor_rect_ follows the position of the
			// virtual caret, but the drop down of predicted candidates wants
			// the starting point of the preedit, so respond with anchor_rect_
			// that points the starting point during the editing mode
			rect = &d->im_state_.anchor_rect_;
			LYXERR(Debug::DEBUG,
			       "     (Composing mode: use anchor_rect_ for ImCursorRectangle)");
		} else
			rect = &d->im_state_.cursor_rect_;

		LYXERR(Debug::DEBUG, msg << " x:" << rect->x() << " y:" << rect->y()
		       << " w:" << rect->width() << " h:" << rect->height());
		Q_EMIT queryProcessed(*rect);
		break;
	}
	case Qt::ImCurrentSelection: {
		LYXERR(Debug::DEBUG, msg << d->cur_->selectionAsString(false));
		Q_EMIT queryProcessed(toqstr(d->cur_->selectionAsString(false)));
		break;
	}
	// plain text around the input area, for example the current paragraph
	case Qt::ImSurroundingText: {
		if (d->im_state_.surrounding_text_.empty())
			LYXERR(Debug::DEBUG, msg << "\"\"");
		else
			LYXERR(Debug::DEBUG, msg <<
			       d->im_state_.surrounding_text_.substr(0, 20) << "...");
		Q_EMIT queryProcessed(toqstr(d->im_state_.surrounding_text_));
		break;
	}
	// logical position of the cursor within the entire document
	case Qt::ImAbsolutePosition: {
		// FIXME: position should be set only when there was a non-virtual
		// change in the document
		setAbsolutePosition(*d->cur_);
		LYXERR(Debug::DEBUG, msg << std::dec << d->abs_pos_);
		Q_EMIT queryProcessed((qlonglong)d->abs_pos_);
		break;
	}
	// plain text before the cursor
	case Qt::ImTextBeforeCursor: {
		updatePosAndSurroundingText();
		if (d->im_state_.text_before_.empty())
			LYXERR(Debug::DEBUG, msg << "\"\"");
		else
			LYXERR(Debug::DEBUG, msg << "…" << d->im_state_.text_before_);
		QVariant str(toqstr(d->im_state_.text_before_));
		Q_EMIT queryProcessed(str);
		break;
	}
	// plain text after the cursor
	case Qt::ImTextAfterCursor: {
		updatePosAndSurroundingText();
		if (d->im_state_.text_after_.empty())
			LYXERR(Debug::DEBUG, msg << "\"\"");
		else
			LYXERR(Debug::DEBUG, msg << d->im_state_.text_after_);
		QVariant str(toqstr(d->im_state_.text_after_));
		Q_EMIT queryProcessed(str);
		break;
	}
	// logical position of the cursor within the text surrounding the input area
	case Qt::ImCursorPosition: {
		updatePosAndSurroundingText();
		LYXERR(Debug::DEBUG, msg << std::dec << d->cur_->pos());
		Q_EMIT queryProcessed((qlonglong)d->cur_->pos());
		break;
	}
	// position of the selection anchor
	case Qt::ImAnchorPosition: {
		updatePosAndSurroundingText();
		LYXERR(Debug::DEBUG, msg << std::dec << (unsigned int)d->anchor_pos_);
		Q_EMIT queryProcessed(QVariant((unsigned int)d->anchor_pos_));
		break;
	}
	case Qt::ImInputItemClipRectangle: {
		QRectF viewport(d->work_area_->viewport()->x(),
		                d->work_area_->viewport()->y(),
		                d->work_area_->viewport()->width(),
		                d->work_area_->viewport()->height());
		LYXERR(Debug::DEBUG, msg << "(x,y,w,h) = " <<
		       viewport.x() << ", " << viewport.y() << ", " <<
		       viewport.width() << ", " << viewport.height() << ")");
		Q_EMIT queryProcessed(viewport);
		break;
	}
	// hints for input method on expected input
	case Qt::ImHints: {
		LYXERR(Debug::DEBUG, msg << "0x" << std::hex
		                         << d->work_area_->inputMethodHints()
		                         << std::dec);
		Q_EMIT queryProcessed((qlonglong)d->work_area_->inputMethodHints());
		break;
	}
	//
	case Qt::ImPreferredLanguage: {
		QLocale locale(toqstr(d->cur_->getFont().language()->code()));
		QString lang = locale.languageToString(locale.language());
		LYXERR(Debug::DEBUG, msg << lang);
		Q_EMIT queryProcessed(lang);
		break;
	}
	// Qt::ImAnchorRectangle holds the selection rectangle in preedit.
	// it seems this property is not yet used by most of input methods
	// as of August 2024.
	case Qt::ImAnchorRectangle: {
		LYXERR(Debug::DEBUG, msg << " x:" << d->im_state_.anchor_rect_.x()
		       << " y:" << d->im_state_.anchor_rect_.y()
		       << " w:" << d->im_state_.anchor_rect_.width()
		       << " h:" << d->im_state_.anchor_rect_.height());
		Q_EMIT queryProcessed(d->im_state_.anchor_rect_);
		break;
	}
	default: {
		QVariant null;
		LYXERR(Debug::DEBUG, "Unsupported query by LyX came in: " <<
		       inputMethodQueryFlagsAsString(query));
		Q_EMIT queryProcessed(null);
	}
	}
}

// Returns enum Qt::InputMethodQuery constant from its value.
// This is for debugging purpose only.
docstring GuiInputMethod::inputMethodQueryFlagsAsString(unsigned long int query) const
{
	docstring str;
	switch (query) {
	case 0x1:       str += "Qt::ImEnabled";                 break;
	case 0x2:       str += "Qt::ImCursorRectangle";         break;
	case 0x4:       str += "Qt::ImFont";                    break;
	case 0x8:       str += "Qt::ImCursorPosition";          break;
	case 0x10:      str += "Qt::ImSurroundingText";         break;
	case 0x20:      str += "Qt::ImCurrentSelection";        break;
	case 0x40:      str += "Qt::ImMaximumTextLength";       break;
	case 0x80:      str += "Qt::ImAnchorPosition";          break;
	case 0x100:     str += "Qt::ImHints";                   break;
	case 0x200:     str += "Qt::ImPreferredLanguage";       break;
	case 0x80000000:str += "Qt::ImPlatformData";            break;
	case 0x400:     str += "Qt::ImAbsolutePosition";        break;
	case 0x800:     str += "Qt::ImTextBeforeCursor";        break;
	case 0x1000:    str += "Qt::ImTextAfterCursor";         break;
	case 0x2000:    str += "Qt::ImEnterKeyType";            break;
	case 0x4000:    str += "Qt::ImAnchorRectangle";         break;
	case 0x8000:    str += "Qt::ImInputItemClipRectangle";  break;
	case 0x10000:   str += "Qt::ImReadOnly";                break;
	}
	return str;
}


//
//     helper functions
//

pos_type GuiInputMethod::initializePositions(Cursor * cur) {
	// the function sets the following variables:
	//     d->cur_pos_ = the starting pos of preedits
	//     d->real_boundary_ = if the starting point is boundary
	//     d->virtual_boundary_ = if the preedits hit the boundary
	// and returns the row index of the starting point of preedits

	// position of the real cursor (also the start of the preedit)
	if (cur->top().pos() != d->cur_pos_)
		Q_EMIT cursorPositionChanged();

	d->pm_ptr_ = resetParagraphMetrics(cur);
	// Note that getRowIndex(., false) gives the row index *after* preedit
	// strings since they are virtual, so it increases as preedit strings go
	// over multiple rows. To fix it at the starting point, getRowIndex(., true)
	// is used here. As a result, cur_row_idx points the row before the boundary
	// in the case it is true.
	pos_type cur_row_idx = d->pm_ptr_->getRowIndex(d->cur_pos_, true);

	//
	// boundary check
	//
	// the real cursor is on boundary
	bool & real_boundary    = d->real_boundary_;
	// the real cursor is followed by virtual chars on boundary
	bool & virtual_boundary = d->virtual_boundary_;
	// cursor is after real boundary
	bool post_real_boundary = false;

	// NOTE:
	// preedits added on boundary goes to the beginning of the next line
	//         -> real_boundary = true   virtual_boundary = false
	// preedits added on boundary is appended to the same line
	//         -> real_boundary = false  virtual_boundary = true

	// width of first-row preedit string in the case of virtual boundary
	int orphan_width = 0;
	if (!(d->rows_->empty() || d->rows_[cur_row_idx].empty()) &&
	        d->rows_[cur_row_idx].back().isPreedit())
		orphan_width =
		        horizontalAdvance(d->rows_[cur_row_idx].back().str);

	int const max_width =
	        d->buffer_view_->textMetrics(cur->innerText()).maxWidth()
	        - d->rows_[cur_row_idx].right_margin;

	bool const has_room_to_insert = d->preedit_str_.empty() ?
	            // if preedit is empty, examine the room with a typical letter
	            max_width - d->rows_[cur_row_idx].width()
	            + orphan_width > horizontalAdvance(from_utf8("あ")) :
	            max_width - d->rows_[cur_row_idx].width() + orphan_width
	            > horizontalAdvance(d->preedit_str_.substr(0,1));

	if (!d->cur_->reverseDirectionNeeded()) {
		if (cur_row_idx > 0) {
			// the second conditions below are to guarantee they are not the
			// continued lines of the virtual_boundary case
			real_boundary = d->buffer_view_->cursor().boundary()
					&& !d->rows_[cur_row_idx - 1].back().isPreedit()
					&& !has_room_to_insert;
			virtual_boundary = d->buffer_view_->cursor().boundary()
					&& !d->rows_[cur_row_idx - 1].back().isPreedit()
					&& has_room_to_insert;
		} else {
			real_boundary = d->buffer_view_->cursor().boundary()
					&& !has_room_to_insert;
			virtual_boundary = d->buffer_view_->cursor().boundary()
					&& has_room_to_insert;
		}
	}
	// cursor is at the head of the next row after boundary
	post_real_boundary =
	        cur_row_idx + 1 < (pos_type)d->rows_size_ ?
	            d->cur_pos_ == d->rows_[cur_row_idx+1].pos() &&
	            !has_room_to_insert : false;

	// Preedit string and the caret starts from the new line when either
	// real_boundary or post_real_boundary is true, so set the cursor row there.
	// As far as caret position is concerned, real_boundary and
	// post_real_boundary can be treated interchangeably, so merge them into
	// real_boundary, i.e. d->real_boundary_.
	real_boundary |= post_real_boundary;
	if (real_boundary)
		++cur_row_idx;

	LYXERR(Debug::DEBUG, "========== BEGIN: initializePositions ==========");
	LYXERR(Debug::DEBUG, "cur_row_idx   = " << cur_row_idx <<
	        "\treal_boundary    = " << real_boundary);
	LYXERR(Debug::DEBUG, "                      " <<
	        "\tvirtual_boundary = " << virtual_boundary);
	LYXERR(Debug::DEBUG, "max width     = " << std::dec <<
	        d->buffer_view_->textMetrics(
	            cur->innerText()).maxWidth()
	             - d->rows_[cur_row_idx].right_margin);
	if (real_boundary)
		LYXERR(Debug::DEBUG, "row width     = " << std::dec <<
		        d->rows_[cur_row_idx-1].width());
	else
		LYXERR(Debug::DEBUG, "row width     = " << std::dec <<
		        d->rows_[cur_row_idx].width());
	if (d->preedit_str_.empty())
		LYXERR(Debug::DEBUG, "wchar width   = " <<
		       horizontalAdvance(from_utf8("あ")));
	else
		LYXERR(Debug::DEBUG, "wchar width = " <<
		        horizontalAdvance(d->preedit_str_.substr(0,1)));
	LYXERR(Debug::DEBUG, "========== END:   initializePositions ==========");

	return cur_row_idx;
}


Point GuiInputMethod::initializeCaretCoords(pos_type const cur_row_idx,
                                            bool const boundary)
{
	// returns the real caret position adjusting boundary case
	// so that it points to the head of the next row

	// reset item transformation since it gets wrong after the item get
	// lost and regain focus.
	if (!d->preedit_str_.empty())
		d->work_area_->resetInputItemGeometry();

	Point point;
	// get dimension of the non-virtual caret
	d->buffer_view_->caretPosAndDim(point, d->cur_dim_);

	// the position of the completion candidate window for the first selected
	// segment seems to be determined by the position of cursor_rect *before*
	// it pops up
	// so the virtual caret must be reported as of the start of the next row
	// if it is on the boundary of a row
	// it only relates to the completion mode
	if (boundary) {
		Row & cur_row = d->rows_[cur_row_idx];
		// the initial position of the preedit is on boundary
		point = {cur_row.left_margin,
		         point.y + cur_row.contents_dim().height()};
	}
	return point;
}


void GuiInputMethod::updateMetrics(Cursor * cur) {
	d->buffer_view_->updateMetrics();
	resetParagraphMetrics(cur);
}


ParagraphMetrics * GuiInputMethod::resetParagraphMetrics(Cursor * cur) {
	// paragraph metrics of the par we are in
	ParagraphMetrics * pm_ptr =
	    &cur->bv().textMetrics(cur->innerText()).parMetrics(cur->top().pit());
	d->rows_ = pm_ptr->rows().begin();
	d->rows_size_ = pm_ptr->rows().size();
	// ID of the row in which the current real cursor resides
	return pm_ptr;
}


pos_type GuiInputMethod::setCaretPos(size_type preedit_length) {
	return d->cur_->top().pos() + preedit_length;
}


GuiInputMethod::PreeditRow GuiInputMethod::getCaretInfo(
        const bool real_boundary, const bool virtual_boundary){

	const pos_type second_row_idx = d->cur_row_idx_ + 1 - virtual_boundary;

	// accumulate the length of preedit elements within d->rows_[d->cur_row_idx_]
	// the length of str is used since preedits has zero widths (pos == endpos)
	// second_row_pos is only useful when preedit string goes over two rows
	Row::const_iterator begin =
	        d->rows_[d->cur_row_idx_].findElement(d->cur_pos_, false);
	pos_type second_row_pos = d->cur_pos_;
	for (Row::const_iterator eit = begin;
	     eit < d->rows_[d->cur_row_idx_].end(); ++eit)
		second_row_pos += eit->str.length();

	PreeditRow caret_row{};

	// when real_boundary is true, cursor position is at the beginning of the
	// new line, while the caret on screen stays at the end of one line above
	// below is the starting point to calculate caret_row.pos
	caret_row.pos = (real_boundary && !d->im_state_.composing_mode_) ?
	            d->cur_pos_ : second_row_pos;

	// if the preedit caret is on the second row or later, count the second row
	caret_row.index = d->caret_pos_ > second_row_pos ?
	            second_row_idx + virtual_boundary : d->cur_row_idx_;

	// the second row exists and begins with the preedit
	if (second_row_idx < (pos_type)d->rows_size_ &&
	        d->rows_[second_row_idx + virtual_boundary].begin()->isPreedit()) {

		for (pos_type i = second_row_idx + virtual_boundary;
			 i < (pos_type)d->rows_size_; i++)
		{
			if (d->rows_[i].front().isPreedit()) {
				int row_length = 0;
				for (const Row::Element & elm : d->rows_[i])
					row_length += elm.str.length();
				if (d->caret_pos_ <= caret_row.pos + row_length)
					break;
				caret_row.pos += row_length;
				++caret_row.index;
			}
		}
	} else
		caret_row.pos = d->cur_pos_;

	LYXERR(Debug::DEBUG, "============= BEGIN: getCaretInfo ===============");
	LYXERR(Debug::DEBUG, "*d->cur_pos_ptr   = " << d->cur_pos_);
	LYXERR(Debug::DEBUG, "second_row_pos    = " << second_row_pos);
	LYXERR(Debug::DEBUG, "d->caret_pos_     = " << std::dec << d->caret_pos_ <<
	        "\tcaret_row.index   = " << std::dec << caret_row.index);
	LYXERR(Debug::DEBUG, "caret_row.pos_    = " << std::dec << caret_row.pos <<
	        "\td->cur_row_idx_   = " << std::dec << d->cur_row_idx_);
	LYXERR(Debug::DEBUG, "real_boundary     = " << real_boundary <<
	        "\tvirtual_boundary  = " << virtual_boundary);
	LYXERR(Debug::DEBUG, "=============== END: getCaretInfo ===============");

	return caret_row;
}

// Get the text before and after the cursor, only those representable as
// a string, which is used to report back to the input method
void GuiInputMethod::setSurroundingText(const Cursor & cur) {
	if (cur.top().inset().asInsetText() == nullptr)
		return;

	docstring partext = cur.top().paragraph().asString();

	// count insets that can affect the cur position in the above string
	InsetList const & inset_list = cur.paragraph().insetList();
	pos_type insets_before_cur = 0;
	for (const auto & inset : inset_list) {
		if (inset.pos < d->cur_pos_)
			++insets_before_cur;
	}

	if (d->cur_pos_ >= insets_before_cur) {
		d->im_state_.text_before_ =
				partext.substr(0, d->cur_pos_ - insets_before_cur);
		d->im_state_.text_after_  =
				partext.substr(d->cur_pos_ - insets_before_cur);
		d->im_state_.surrounding_text_ =
		        d->im_state_.text_before_ + d->im_state_.text_after_;
	} else {
		d->im_state_.text_before_.clear();
		d->im_state_.text_after_  = partext;
		d->im_state_.surrounding_text_ = partext;
	}

	LYXERR(Debug::DEBUG, "surrounding text before cursor = " <<
	       d->im_state_.text_before_);
	LYXERR(Debug::DEBUG, "surrounding text after  cursor = " <<
	       d->im_state_.text_after_);

	// notify the input method about the update
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
	// work around Qt bug
	// see: https://code.qt.io/cgit/qt/qtbase.git/commit/?id=e759d38d
	using ::operator|;
#endif
	Q_EMIT inputMethodStateChanged(Qt::ImSurroundingText |
	                               Qt::ImTextBeforeCursor |
	                               Qt::ImTextAfterCursor);
	return;
}

void GuiInputMethod::updatePosAndSurroundingText()
{
	if (d->cur_->top().pos() == d->cur_pos_)
		return;
	Q_EMIT cursorPositionChanged();
}


docstring & GuiInputMethod::preeditString() const
{
	return d->preedit_str_;
}
pos_type & GuiInputMethod::segmentStart(size_type seg_id) const
{
	return d->style_.segments_[seg_id].start_;
}
size_type & GuiInputMethod::segmentLength(size_type seg_id) const
{
	return d->style_.segments_[seg_id].length_;
}
size_type GuiInputMethod::segmentSize() const
{
	return d->style_.segments_.size();
}
QTextCharFormat & GuiInputMethod::charFormat(pos_type index) const {
	return d->style_.segments_[index].char_format_;
}
pos_type GuiInputMethod::charFormatIndex(pos_type pos) const {
	// find out format index at preedit_pos
	// note that vector preeditStarts may not be sorted
	size_t fmt_index;
	std::vector<pair<size_type, size_type>> fmt_candidates;
	for (size_t j = 0 ; j < d->style_.segments_.size() ; ++j)
		if ((pos_type)d->style_.segments_[j].start_ <= pos)
			fmt_candidates.push_back(make_pair(d->style_.segments_[j].start_,j));
	if (fmt_candidates.size() > 0) {
		fmt_index = (*max_element(std::begin(fmt_candidates),
		                          std::end(fmt_candidates))).second;
	} else
		fmt_index = 0;

	return fmt_index;
}

QLocale & GuiInputMethod::locale() const
{
	return d->locale_;
}

// set absolute position of the cursor in the entire document
void GuiInputMethod::setAbsolutePosition(Cursor & cur) const
{
	d->abs_pos_ = 0;
	ParagraphList paras = cur.buffer()->paragraphs();
	for (int i=0; i < (int)paras.size(); ++i) {
		if (i >= cur.pit()) break;
		d->abs_pos_ += paras[i].size();
	}
	d->abs_pos_ += cur.pos();
}

int GuiInputMethod::horizontalAdvance(docstring const & s,
                                      pos_type const char_format_index) {
	QFontMetrics qfm(charFormat(char_format_index).font());
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return qfm.horizontalAdvance(toqstr(s));
#else
	return qfm.width(toqstr(s));
#endif
}

int GuiInputMethod::horizontalAdvance(docstring const & s) {
	QTextCharFormat qtcf;
	conformToSurroundingFont(qtcf);
	QFontMetrics qfm(qtcf.font());
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return qfm.horizontalAdvance(toqstr(s));
#else
	return qfm.width(toqstr(s));
#endif
}

bool GuiInputMethod::canWrapAnywhere(pos_type const char_format_index) {
	QLocale locale =
		charFormat(char_format_index).property(QMetaType::QLocale).toLocale();
	// Languages wrappable at any place that use the input method
	// CJK is listed here but anything else?
	if (locale == QLocale::Japanese || locale == QLocale::Chinese ||
		locale == QLocale::Korean || locale == QLocale::LiteraryChinese)
		return true;
	else
		return false;
}

} // namespace frontend

} // namespace lyx

#include "moc_GuiInputMethod.cpp"
