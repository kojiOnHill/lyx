/**
 * \file GuiPrefs.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Bo Peng
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiPrefs.h"

#include "ColorCache.h"
#include "FileDialog.h"
#include "GuiApplication.h"
#include "GuiFontExample.h"
#include "GuiLyXFiles.h"
#include "qt_helpers.h"
#include "version.h"
#include "Validator.h"

#include "Author.h"
#include "BufferList.h"
#include "Color.h"
#include "ColorSet.h"
#include "ConverterCache.h"
#include "FontEnums.h"
#include "FuncRequest.h"
#include "KeySequence.h"
#include "Language.h"
#include "LengthCombo.h"
#include "LyXAction.h"
#include "LyX.h"
#include "PanelStack.h"

#include "support/debug.h"
#include "support/FileNameList.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/lstrings.h"
#include "support/Messages.h"
#include "support/os.h"
#include "support/Package.h"

#include "frontends/alert.h"
#include "frontends/Application.h"

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QColorDialog>
#include <QFile>
#include <QFontDatabase>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QString>
#include <QStyleFactory>
#if (defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN) || defined(Q_OS_MAC)) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QStyleHints>
#endif
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QValidator>

#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <math.h>

using namespace Ui;

using namespace std;
using namespace lyx::support;
using namespace lyx::support::os;

namespace lyx {

/////////////////////////////////////////////////////////////////////
//
// Helpers
//
/////////////////////////////////////////////////////////////////////

namespace frontend {

class GuiView;

QString const catLookAndFeel = N_("Look & Feel");
QString const catEditing = N_("Editing");
QString const catLanguage = N_("Language Settings");
QString const catOutput = N_("Output");
QString const catFiles = N_("File Handling");

static void parseFontName(QString const & mangled0,
	string & name, string & foundry)
{
	string mangled = fromqstr(mangled0);
	size_t const idx = mangled.find('[');
	if (idx == string::npos || idx == 0) {
		name = mangled;
		foundry.clear();
	} else {
		name = mangled.substr(0, idx - 1);
		foundry = mangled.substr(idx + 1, mangled.size() - idx - 2);
	}
}


static void setComboxFont(QComboBox * cb, string const & family,
	string const & foundry)
{
	QString fontname = toqstr(family);
	if (!foundry.empty())
		fontname += " [" + toqstr(foundry) + ']';

	for (int i = 0; i != cb->count(); ++i) {
		if (cb->itemText(i) == fontname) {
			cb->setCurrentIndex(i);
			return;
		}
	}

	// Try matching without foundry name

	// We count in reverse in order to prefer the Xft foundry
	for (int i = cb->count(); --i >= 0;) {
		string name, fnt_foundry;
		parseFontName(cb->itemText(i), name, fnt_foundry);
		if (compare_ascii_no_case(name, family) == 0) {
			cb->setCurrentIndex(i);
			return;
		}
	}

	// family alone can contain e.g. "Helvetica [Adobe]"
	string tmpname, tmpfoundry;
	parseFontName(toqstr(family), tmpname, tmpfoundry);

	// We count in reverse in order to prefer the Xft foundry
	for (int i = cb->count(); --i >= 0; ) {
		string name, fnt_foundry;
		parseFontName(cb->itemText(i), name, fnt_foundry);
		if (compare_ascii_no_case(name, fnt_foundry) == 0) {
			cb->setCurrentIndex(i);
			return;
		}
	}

	// Bleh, default fonts, and the names couldn't be found. Hack
	// for bug 1063.

	QFont font;

	QString const font_family = toqstr(family);
	if (font_family == guiApp->romanFontName()) {
		font.setStyleHint(QFont::Serif);
		font.setFamily(font_family);
	} else if (font_family == guiApp->sansFontName()) {
		font.setStyleHint(QFont::SansSerif);
		font.setFamily(font_family);
	} else if (font_family == guiApp->typewriterFontName()) {
		font.setStyleHint(QFont::TypeWriter);
		font.setFamily(font_family);
	} else {
		LYXERR0("FAILED to find the default font: '"
		       << foundry << "', '" << family << '\'');
		return;
	}

	QFontInfo info(font);
	string default_font_name, dummyfoundry;
	parseFontName(info.family(), default_font_name, dummyfoundry);
	LYXERR0("Apparent font is " << default_font_name);

	for (int i = 0; i < cb->count(); ++i) {
		LYXERR0("Looking at " << cb->itemText(i));
		if (compare_ascii_no_case(fromqstr(cb->itemText(i)),
				    default_font_name) == 0) {
			cb->setCurrentIndex(i);
			return;
		}
	}

	LYXERR0("FAILED to find the font: '"
	       << foundry << "', '" << family << '\'');
}


static void activatePrefsWindow(GuiPreferences * form_)
{
	if (guiApp->platformName() == "cocoa") {
		QWidget * dialog_ = form_->asQWidget();
		dialog_->raise();
		dialog_->activateWindow();
	}
}


/////////////////////////////////////////////////////////////////////
//
// PrefOutput
//
/////////////////////////////////////////////////////////////////////

PrefOutput::PrefOutput(GuiPreferences * form)
	: PrefModule(catOutput, N_("General[[settings]]"), form)
{
	setupUi(this);

	dviCB->setValidator(new NoNewLineValidator(dviCB));
	pdfCB->setValidator(new NoNewLineValidator(pdfCB));

	connect(plaintextLinelengthSB, SIGNAL(valueChanged(int)),
		this, SIGNAL(changed()));
	connect(overwriteCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(dviCB, SIGNAL(editTextChanged(QString)),
		this, SIGNAL(changed()));
	connect(pdfCB, SIGNAL(editTextChanged(QString)),
		this, SIGNAL(changed()));
	connect(printerPaperTypeED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(printerLandscapeED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(printerPaperSizeED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	printerPaperTypeED->setValidator(new NoNewLineValidator(printerPaperTypeED));
	printerLandscapeED->setValidator(new NoNewLineValidator(printerLandscapeED));
	printerPaperSizeED->setValidator(new NoNewLineValidator(printerPaperSizeED));

	dviCB->addItem("");
	dviCB->addItem("xdvi -sourceposition '$$n:\\ $$t' $$o");
	dviCB->addItem("yap -1 -s \"$$n $$t\" $$o");
	dviCB->addItem("okular --unique \"$$o#src:$$n $$f\"");
	dviCB->addItem("synctex view -i $$n:0:$$t -o $$o -x \"evince -i %{page+1} $$o\"");
	pdfCB->addItem("");
	pdfCB->addItem("CMCDDE SUMATRA control [ForwardSearch(\\\"$$o\\\",\\\"$$t\\\",$$n,0,0,1)]");
	pdfCB->addItem("SumatraPDF -reuse-instance \"$$o\" -forward-search \"$$t\" $$n");
	pdfCB->addItem("synctex view -i $$n:0:$$t -o $$o -x \"xpdf -raise -remote $$t.tmp $$o %{page+1}\"");
	pdfCB->addItem("okular --unique \"$$o#src:$$n $$f\"");
	pdfCB->addItem("qpdfview --unique \"$$o#src:$$f:$$n:0\"");
	pdfCB->addItem("synctex view -i $$n:0:$$t -o $$o -x \"evince -i %{page+1} $$o\"");
	pdfCB->addItem("/Applications/Skim.app/Contents/SharedSupport/displayline $$n $$o $$t");
}


void PrefOutput::applyRC(LyXRC & rc) const
{
	rc.plaintext_linelen = plaintextLinelengthSB->value();
	rc.forward_search_dvi = fromqstr(dviCB->currentText());
	rc.forward_search_pdf = fromqstr(pdfCB->currentText());

	switch (overwriteCO->currentIndex()) {
	case 0:
		rc.export_overwrite = NO_FILES;
		break;
	case 1:
		rc.export_overwrite = MAIN_FILE;
		break;
	case 2:
		rc.export_overwrite = ALL_FILES;
		break;
	}

	rc.print_paper_flag = fromqstr(printerPaperTypeED->text());
	rc.print_landscape_flag = fromqstr(printerLandscapeED->text());
	rc.print_paper_dimension_flag = fromqstr(printerPaperSizeED->text());
}


void PrefOutput::updateRC(LyXRC const & rc)
{
	plaintextLinelengthSB->setValue(rc.plaintext_linelen);
	dviCB->setEditText(toqstr(rc.forward_search_dvi));
	pdfCB->setEditText(toqstr(rc.forward_search_pdf));

	switch (rc.export_overwrite) {
	case NO_FILES:
		overwriteCO->setCurrentIndex(0);
		break;
	case MAIN_FILE:
		overwriteCO->setCurrentIndex(1);
		break;
	case ALL_FILES:
		overwriteCO->setCurrentIndex(2);
		break;
	}

	printerPaperTypeED->setText(toqstr(rc.print_paper_flag));
	printerLandscapeED->setText(toqstr(rc.print_landscape_flag));
	printerPaperSizeED->setText(toqstr(rc.print_paper_dimension_flag));
}


/////////////////////////////////////////////////////////////////////
//
// PrefInput
//
/////////////////////////////////////////////////////////////////////

PrefInput::PrefInput(GuiPreferences * form)
	: PrefModule(catEditing, N_("Keyboard/Mouse"), form)
{
	setupUi(this);

	connect(keymapCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(firstKeymapED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(secondKeymapED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(mouseWheelSpeedSB, SIGNAL(valueChanged(double)),
		this, SIGNAL(changed()));
	connect(scrollzoomEnableCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(scrollzoomValueCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(dontswapCB, SIGNAL(toggled(bool)),
		this, SIGNAL(changed()));
	connect(mmPasteCB, SIGNAL(toggled(bool)),
		this, SIGNAL(changed()));

	// reveal checkbox for switching Ctrl and Meta on Mac:
#ifdef Q_OS_MAC
	dontswapCB->setVisible(true);
#else
	dontswapCB->setVisible(false);
#endif
}


void PrefInput::applyRC(LyXRC & rc) const
{
	// FIXME: can derive CB from the two EDs
	rc.use_kbmap = keymapCB->isChecked();
	rc.primary_kbmap = internal_path(fromqstr(firstKeymapED->text()));
	rc.secondary_kbmap = internal_path(fromqstr(secondKeymapED->text()));
	rc.mouse_wheel_speed = mouseWheelSpeedSB->value();
	if (scrollzoomEnableCB->isChecked()) {
		switch (scrollzoomValueCO->currentIndex()) {
		case 0:
			rc.scroll_wheel_zoom = LyXRC::SCROLL_WHEEL_ZOOM_CTRL;
			break;
		case 1:
			rc.scroll_wheel_zoom = LyXRC::SCROLL_WHEEL_ZOOM_SHIFT;
			break;
		case 2:
			rc.scroll_wheel_zoom = LyXRC::SCROLL_WHEEL_ZOOM_ALT;
			break;
		}
	} else {
		rc.scroll_wheel_zoom = LyXRC::SCROLL_WHEEL_ZOOM_OFF;
	}
	rc.mac_dontswap_ctrl_meta  = dontswapCB->isChecked();
	rc.mouse_middlebutton_paste = mmPasteCB->isChecked();
}


void PrefInput::updateRC(LyXRC const & rc)
{
	// FIXME: can derive CB from the two EDs
	keymapCB->setChecked(rc.use_kbmap);
	firstKeymapED->setText(toqstr(external_path(rc.primary_kbmap)));
	secondKeymapED->setText(toqstr(external_path(rc.secondary_kbmap)));
	mouseWheelSpeedSB->setValue(rc.mouse_wheel_speed);
	switch (rc.scroll_wheel_zoom) {
	case LyXRC::SCROLL_WHEEL_ZOOM_OFF:
		scrollzoomEnableCB->setChecked(false);
		break;
	case LyXRC::SCROLL_WHEEL_ZOOM_CTRL:
		scrollzoomEnableCB->setChecked(true);
		scrollzoomValueCO->setCurrentIndex(0);
		break;
	case LyXRC::SCROLL_WHEEL_ZOOM_SHIFT:
		scrollzoomEnableCB->setChecked(true);
		scrollzoomValueCO->setCurrentIndex(1);
		break;
	case LyXRC::SCROLL_WHEEL_ZOOM_ALT:
		scrollzoomEnableCB->setChecked(true);
		scrollzoomValueCO->setCurrentIndex(2);
		break;
	}
	dontswapCB->setChecked(rc.mac_dontswap_ctrl_meta);
	mmPasteCB->setChecked(rc.mouse_middlebutton_paste);
}


QString PrefInput::testKeymap(QString const & keymap)
{
	return form_->browsekbmap(internalPath(keymap));
}


void PrefInput::on_firstKeymapPB_clicked(bool)
{
	QString const file = testKeymap(firstKeymapED->text());
	if (!file.isEmpty())
		firstKeymapED->setText(file);
}


void PrefInput::on_secondKeymapPB_clicked(bool)
{
	QString const file = testKeymap(secondKeymapED->text());
	if (!file.isEmpty())
		secondKeymapED->setText(file);
}


void PrefInput::on_keymapCB_toggled(bool keymap)
{
	firstKeymapLA->setEnabled(keymap);
	secondKeymapLA->setEnabled(keymap);
	firstKeymapED->setEnabled(keymap);
	secondKeymapED->setEnabled(keymap);
	firstKeymapPB->setEnabled(keymap);
	secondKeymapPB->setEnabled(keymap);
}


void PrefInput::on_scrollzoomEnableCB_toggled(bool enabled)
{
	scrollzoomValueCO->setEnabled(enabled);
}


/////////////////////////////////////////////////////////////////////
//
// PrefCompletion
//
/////////////////////////////////////////////////////////////////////

PrefCompletion::PrefCompletion(GuiPreferences * form)
	: PrefModule(catEditing, N_("Input Completion"), form)
{
	setupUi(this);

	connect(inlineDelaySB, SIGNAL(valueChanged(double)),
		this, SIGNAL(changed()));
	connect(inlineMathCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(inlineTextCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(inlineDotsCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(popupDelaySB, SIGNAL(valueChanged(double)),
		this, SIGNAL(changed()));
	connect(popupMathCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(autocorrectionCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(popupTextCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(popupAfterCompleteCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(cursorTextCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(minlengthSB, SIGNAL(valueChanged(int)),
			this, SIGNAL(changed()));
}


void PrefCompletion::on_inlineTextCB_clicked()
{
	enableCB();
}


void PrefCompletion::on_popupTextCB_clicked()
{
	enableCB();
}


void PrefCompletion::enableCB()
{
	cursorTextCB->setEnabled(
		popupTextCB->isChecked() || inlineTextCB->isChecked());
}


void PrefCompletion::applyRC(LyXRC & rc) const
{
	rc.completion_inline_delay = inlineDelaySB->value();
	rc.completion_inline_math = inlineMathCB->isChecked();
	rc.completion_inline_text = inlineTextCB->isChecked();
	rc.completion_inline_dots = inlineDotsCB->isChecked() ? 13 : -1;
	rc.completion_popup_delay = popupDelaySB->value();
	rc.completion_popup_math = popupMathCB->isChecked();
	rc.autocorrection_math = autocorrectionCB->isChecked();
	rc.completion_popup_text = popupTextCB->isChecked();
	rc.completion_cursor_text = cursorTextCB->isChecked();
	rc.completion_popup_after_complete =
		popupAfterCompleteCB->isChecked();
	rc.completion_minlength = minlengthSB->value();
}


void PrefCompletion::updateRC(LyXRC const & rc)
{
	inlineDelaySB->setValue(rc.completion_inline_delay);
	inlineMathCB->setChecked(rc.completion_inline_math);
	inlineTextCB->setChecked(rc.completion_inline_text);
	inlineDotsCB->setChecked(rc.completion_inline_dots != -1);
	popupDelaySB->setValue(rc.completion_popup_delay);
	popupMathCB->setChecked(rc.completion_popup_math);
	autocorrectionCB->setChecked(rc.autocorrection_math);
	popupTextCB->setChecked(rc.completion_popup_text);
	cursorTextCB->setChecked(rc.completion_cursor_text);
	popupAfterCompleteCB->setChecked(rc.completion_popup_after_complete);
	enableCB();
	minlengthSB->setValue(rc.completion_minlength);
}



/////////////////////////////////////////////////////////////////////
//
// PrefLatex
//
/////////////////////////////////////////////////////////////////////

PrefLatex::PrefLatex(GuiPreferences * form)
	: PrefModule(catOutput, N_("LaTeX"), form)
{
	setupUi(this);

	latexDviPaperED->setValidator(new NoNewLineValidator(latexDviPaperED));
	latexBibtexED->setValidator(new NoNewLineValidator(latexBibtexED));
	latexJBibtexED->setValidator(new NoNewLineValidator(latexJBibtexED));
	latexIndexED->setValidator(new NoNewLineValidator(latexIndexED));
	latexJIndexED->setValidator(new NoNewLineValidator(latexJIndexED));
	latexNomenclED->setValidator(new NoNewLineValidator(latexNomenclED));
	latexChecktexED->setValidator(new NoNewLineValidator(latexChecktexED));

	connect(latexChecktexED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(latexBibtexCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(latexBibtexED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(latexJBibtexCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(latexJBibtexED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(latexIndexCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(latexIndexED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(latexJIndexED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(latexAutoresetCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(latexDviPaperED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(latexNomenclED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

#if defined(__CYGWIN__) || defined(_WIN32)
	pathCB->setVisible(true);
	connect(pathCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
#else
	pathCB->setVisible(false);
#endif
}


void PrefLatex::on_latexBibtexCO_activated(int n)
{
	QString const bibtex = latexBibtexCO->itemData(n).toString();
	if (bibtex.isEmpty()) {
		latexBibtexED->clear();
		latexBibtexOptionsLA->setText(qt_("C&ommand:"));
		return;
	}
	for (LyXRC::CommandSet::const_iterator it = bibtex_alternatives.begin();
	     it != bibtex_alternatives.end(); ++it) {
		QString const bib = toqstr(*it);
		int ind = bib.indexOf(" ");
		QString sel_command = bib.left(ind);
		QString sel_options = ind < 0 ? QString() : bib.mid(ind + 1);
		if (bibtex == sel_command) {
			if (ind < 0)
				latexBibtexED->clear();
			else
				latexBibtexED->setText(sel_options.trimmed());
		}
	}
	latexBibtexOptionsLA->setText(qt_("&Options:"));
}


void PrefLatex::on_latexJBibtexCO_activated(int n)
{
	QString const jbibtex = latexJBibtexCO->itemData(n).toString();
	if (jbibtex.isEmpty()) {
		latexJBibtexED->clear();
		latexJBibtexOptionsLA->setText(qt_("Co&mmand:"));
		return;
	}
	for (LyXRC::CommandSet::const_iterator it = jbibtex_alternatives.begin();
	     it != jbibtex_alternatives.end(); ++it) {
		QString const bib = toqstr(*it);
		int ind = bib.indexOf(" ");
		QString sel_command = bib.left(ind);
		QString sel_options = ind < 0 ? QString() : bib.mid(ind + 1);
		if (jbibtex == sel_command) {
			if (ind < 0)
				latexJBibtexED->clear();
			else
				latexJBibtexED->setText(sel_options.trimmed());
		}
	}
	latexJBibtexOptionsLA->setText(qt_("Opt&ions:"));
}


void PrefLatex::on_latexIndexCO_activated(int n)
{
	QString const index = latexIndexCO->itemData(n).toString();
	if (index.isEmpty()) {
		latexIndexED->clear();
		latexIndexOptionsLA->setText(qt_("Co&mmand:"));
		return;
	}
	for (LyXRC::CommandSet::const_iterator it = index_alternatives.begin();
	     it != index_alternatives.end(); ++it) {
		QString const idx = toqstr(*it);
		int ind = idx.indexOf(" ");
		QString sel_command = idx.left(ind);
		QString sel_options = ind < 0 ? QString() : idx.mid(ind + 1);
		if (index == sel_command) {
			if (ind < 0)
				latexIndexED->clear();
			else
				latexIndexED->setText(sel_options.trimmed());
		}
	}
	latexIndexOptionsLA->setText(qt_("Op&tions:"));
}


void PrefLatex::applyRC(LyXRC & rc) const
{
	// If bibtex is not empty, bibopt contains the options, otherwise
	// it is a customized bibtex command with options.
	QString const bibtex = latexBibtexCO->itemData(
		latexBibtexCO->currentIndex()).toString();
	QString const bibopt = latexBibtexED->text();
	if (bibtex.isEmpty())
		rc.bibtex_command = fromqstr(bibopt);
	else if (bibopt.isEmpty())
		rc.bibtex_command = fromqstr(bibtex);
	else
		rc.bibtex_command = fromqstr(bibtex) + " " + fromqstr(bibopt);

	// If jbibtex is not empty, jbibopt contains the options, otherwise
	// it is a customized bibtex command with options.
	QString const jbibtex = latexJBibtexCO->itemData(
		latexJBibtexCO->currentIndex()).toString();
	QString const jbibopt = latexJBibtexED->text();
	if (jbibtex.isEmpty())
		rc.jbibtex_command = fromqstr(jbibopt);
	else if (jbibopt.isEmpty())
		rc.jbibtex_command = fromqstr(jbibtex);
	else
		rc.jbibtex_command = fromqstr(jbibtex) + " " + fromqstr(jbibopt);

	// If index is not empty, idxopt contains the options, otherwise
	// it is a customized index command with options.
	QString const index = latexIndexCO->itemData(
		latexIndexCO->currentIndex()).toString();
	QString const idxopt = latexIndexED->text();
	if (index.isEmpty())
		rc.index_command = fromqstr(idxopt);
	else if (idxopt.isEmpty())
		rc.index_command = fromqstr(index);
	else
		rc.index_command = fromqstr(index) + " " + fromqstr(idxopt);

	rc.chktex_command = fromqstr(latexChecktexED->text());
	rc.jindex_command = fromqstr(latexJIndexED->text());
	rc.nomencl_command = fromqstr(latexNomenclED->text());
	rc.auto_reset_options = latexAutoresetCB->isChecked();
	rc.view_dvi_paper_option = fromqstr(latexDviPaperED->text());
#if defined(__CYGWIN__) || defined(_WIN32)
	rc.windows_style_tex_paths = pathCB->isChecked();
#endif
}


void PrefLatex::updateRC(LyXRC const & rc)
{
	latexBibtexCO->clear();

	latexBibtexCO->addItem(qt_("Automatic"), "automatic");
	latexBibtexCO->addItem(qt_("Custom"), QString());
	for (LyXRC::CommandSet::const_iterator it = rc.bibtex_alternatives.begin();
			     it != rc.bibtex_alternatives.end(); ++it) {
		QString const command = toqstr(*it).left(toqstr(*it).indexOf(" "));
		latexBibtexCO->addItem(command, command);
	}

	bibtex_alternatives = rc.bibtex_alternatives;

	QString const bib = toqstr(rc.bibtex_command);
	int ind = bib.indexOf(" ");
	QString sel_command = bib.left(ind);
	QString sel_options = ind < 0 ? QString() : bib.mid(ind + 1);

	int pos = latexBibtexCO->findData(sel_command);
	if (pos != -1) {
		latexBibtexCO->setCurrentIndex(pos);
		latexBibtexED->setText(sel_options.trimmed());
		latexBibtexOptionsLA->setText(qt_("&Options:"));
	} else {
		latexBibtexED->setText(toqstr(rc.bibtex_command));
		latexBibtexCO->setCurrentIndex(0);
		latexBibtexOptionsLA->setText(qt_("C&ommand:"));
	}

	latexJBibtexCO->clear();

	latexJBibtexCO->addItem(qt_("Automatic"), "automatic");
	latexJBibtexCO->addItem(qt_("Custom"), QString());
	for (LyXRC::CommandSet::const_iterator it = rc.jbibtex_alternatives.begin();
			     it != rc.jbibtex_alternatives.end(); ++it) {
		QString const command = toqstr(*it).left(toqstr(*it).indexOf(" "));
		latexJBibtexCO->addItem(command, command);
	}

	jbibtex_alternatives = rc.jbibtex_alternatives;

	QString const jbib = toqstr(rc.jbibtex_command);
	ind = jbib.indexOf(" ");
	sel_command = jbib.left(ind);
	sel_options = ind < 0 ? QString() : jbib.mid(ind + 1);

	pos = latexJBibtexCO->findData(sel_command);
	if (pos != -1) {
		latexJBibtexCO->setCurrentIndex(pos);
		latexJBibtexED->setText(sel_options.trimmed());
		latexJBibtexOptionsLA->setText(qt_("Opt&ions:"));
	} else {
		latexJBibtexED->setText(toqstr(rc.bibtex_command));
		latexJBibtexCO->setCurrentIndex(0);
		latexJBibtexOptionsLA->setText(qt_("Co&mmand:"));
	}

	latexIndexCO->clear();

	latexIndexCO->addItem(qt_("Custom"), QString());
	for (LyXRC::CommandSet::const_iterator it = rc.index_alternatives.begin();
			     it != rc.index_alternatives.end(); ++it) {
		QString const command = toqstr(*it).left(toqstr(*it).indexOf(" "));
		latexIndexCO->addItem(command, command);
	}

	index_alternatives = rc.index_alternatives;

	QString const idx = toqstr(rc.index_command);
	ind = idx.indexOf(" ");
	sel_command = idx.left(ind);
	sel_options = ind < 0 ? QString() : idx.mid(ind + 1);

	pos = latexIndexCO->findData(sel_command);
	if (pos != -1) {
		latexIndexCO->setCurrentIndex(pos);
		latexIndexED->setText(sel_options.trimmed());
		latexIndexOptionsLA->setText(qt_("Op&tions:"));
	} else {
		latexIndexED->setText(toqstr(rc.index_command));
		latexIndexCO->setCurrentIndex(0);
		latexIndexOptionsLA->setText(qt_("Co&mmand:"));
	}

	latexChecktexED->setText(toqstr(rc.chktex_command));
	latexJIndexED->setText(toqstr(rc.jindex_command));
	latexNomenclED->setText(toqstr(rc.nomencl_command));
	latexAutoresetCB->setChecked(rc.auto_reset_options);
	latexDviPaperED->setText(toqstr(rc.view_dvi_paper_option));
#if defined(__CYGWIN__) || defined(_WIN32)
	pathCB->setChecked(rc.windows_style_tex_paths);
#endif
}


/////////////////////////////////////////////////////////////////////
//
// PrefScreenFonts
//
/////////////////////////////////////////////////////////////////////

PrefScreenFonts::PrefScreenFonts(GuiPreferences * form)
	: PrefModule(catLookAndFeel, N_("Screen Fonts"), form)
{
	setupUi(this);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	connect(screenRomanCO, SIGNAL(activated(QString)),
		this, SLOT(selectRoman(QString)));
	connect(screenSansCO, SIGNAL(activated(QString)),
		this, SLOT(selectSans(QString)));
	connect(screenTypewriterCO, SIGNAL(activated(QString)),
		this, SLOT(selectTypewriter(QString)));
#else
	connect(screenRomanCO, SIGNAL(textActivated(QString)),
		this, SLOT(selectRoman(QString)));
	connect(screenSansCO, SIGNAL(textActivated(QString)),
		this, SLOT(selectSans(QString)));
	connect(screenTypewriterCO, SIGNAL(textActivated(QString)),
		this, SLOT(selectTypewriter(QString)));
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	const QStringList families(QFontDatabase::families());
#else
	QFontDatabase fontdb;
	const QStringList families(fontdb.families());
#endif
	for (auto const & family : families) {
		screenRomanCO->addItem(family);
		screenSansCO->addItem(family);
		screenTypewriterCO->addItem(family);
	}
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	connect(screenRomanCO, SIGNAL(activated(QString)),
		this, SIGNAL(changed()));
	connect(screenSansCO, SIGNAL(activated(QString)),
		this, SIGNAL(changed()));
	connect(screenTypewriterCO, SIGNAL(activated(QString)),
		this, SIGNAL(changed()));
#else
	connect(screenRomanCO, SIGNAL(textActivated(QString)),
		this, SIGNAL(changed()));
	connect(screenSansCO, SIGNAL(textActivated(QString)),
		this, SIGNAL(changed()));
	connect(screenTypewriterCO, SIGNAL(textActivated(QString)),
		this, SIGNAL(changed()));
#endif
	connect(screenZoomSB, SIGNAL(valueChanged(int)),
		this, SIGNAL(changed()));
	connect(screenTinyED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenSmallestED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenSmallerED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenSmallED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenNormalED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenLargeED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenLargerED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenLargestED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenHugeED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenHugerED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	screenTinyED->setValidator(new QDoubleValidator(screenTinyED));
	screenSmallestED->setValidator(new QDoubleValidator(screenSmallestED));
	screenSmallerED->setValidator(new QDoubleValidator(screenSmallerED));
	screenSmallED->setValidator(new QDoubleValidator(screenSmallED));
	screenNormalED->setValidator(new QDoubleValidator(screenNormalED));
	screenLargeED->setValidator(new QDoubleValidator(screenLargeED));
	screenLargerED->setValidator(new QDoubleValidator(screenLargerED));
	screenLargestED->setValidator(new QDoubleValidator(screenLargestED));
	screenHugeED->setValidator(new QDoubleValidator(screenHugeED));
	screenHugerED->setValidator(new QDoubleValidator(screenHugerED));
}


void PrefScreenFonts::applyRC(LyXRC & rc) const
{
	LyXRC const oldrc = rc;

	parseFontName(screenRomanCO->currentText(),
		rc.roman_font_name, rc.roman_font_foundry);
	parseFontName(screenSansCO->currentText(),
		rc.sans_font_name, rc.sans_font_foundry);
	parseFontName(screenTypewriterCO->currentText(),
		rc.typewriter_font_name, rc.typewriter_font_foundry);

	rc.defaultZoom = screenZoomSB->value();
	rc.font_sizes[TINY_SIZE] = widgetToDoubleStr(screenTinyED);
	rc.font_sizes[SCRIPT_SIZE] = widgetToDoubleStr(screenSmallestED);
	rc.font_sizes[FOOTNOTE_SIZE] = widgetToDoubleStr(screenSmallerED);
	rc.font_sizes[SMALL_SIZE] = widgetToDoubleStr(screenSmallED);
	rc.font_sizes[NORMAL_SIZE] = widgetToDoubleStr(screenNormalED);
	rc.font_sizes[LARGE_SIZE] = widgetToDoubleStr(screenLargeED);
	rc.font_sizes[LARGER_SIZE] = widgetToDoubleStr(screenLargerED);
	rc.font_sizes[LARGEST_SIZE] = widgetToDoubleStr(screenLargestED);
	rc.font_sizes[HUGE_SIZE] = widgetToDoubleStr(screenHugeED);
	rc.font_sizes[HUGER_SIZE] = widgetToDoubleStr(screenHugerED);
}


void PrefScreenFonts::updateRC(LyXRC const & rc)
{
	setComboxFont(screenRomanCO, rc.roman_font_name,
			rc.roman_font_foundry);
	setComboxFont(screenSansCO, rc.sans_font_name,
			rc.sans_font_foundry);
	setComboxFont(screenTypewriterCO, rc.typewriter_font_name,
			rc.typewriter_font_foundry);

	selectRoman(screenRomanCO->currentText());
	selectSans(screenSansCO->currentText());
	selectTypewriter(screenTypewriterCO->currentText());

	screenZoomSB->setValue(rc.defaultZoom);
	updateScreenFontSizes(rc);
}


void PrefScreenFonts::updateScreenFontSizes(LyXRC const & rc)
{
	doubleToWidget(screenTinyED, rc.font_sizes[TINY_SIZE]);
	doubleToWidget(screenSmallestED, rc.font_sizes[SCRIPT_SIZE]);
	doubleToWidget(screenSmallerED, rc.font_sizes[FOOTNOTE_SIZE]);
	doubleToWidget(screenSmallED, rc.font_sizes[SMALL_SIZE]);
	doubleToWidget(screenNormalED, rc.font_sizes[NORMAL_SIZE]);
	doubleToWidget(screenLargeED, rc.font_sizes[LARGE_SIZE]);
	doubleToWidget(screenLargerED, rc.font_sizes[LARGER_SIZE]);
	doubleToWidget(screenLargestED, rc.font_sizes[LARGEST_SIZE]);
	doubleToWidget(screenHugeED, rc.font_sizes[HUGE_SIZE]);
	doubleToWidget(screenHugerED, rc.font_sizes[HUGER_SIZE]);
}


void PrefScreenFonts::selectRoman(const QString & name)
{
	screenRomanFE->set(QFont(name), name);
	screenFontsChanged();
}


void PrefScreenFonts::selectSans(const QString & name)
{
	screenSansFE->set(QFont(name), name);
	screenFontsChanged();
}


void PrefScreenFonts::selectTypewriter(const QString & name)
{
	screenTypewriterFE->set(QFont(name), name);
	screenFontsChanged();
}


void PrefScreenFonts::screenFontsChanged()
{
	int w = max(screenRomanFE->minWidth(), screenSansFE->minWidth());
	w = max(screenTypewriterFE->minWidth(), w);
	screenRomanFE->setFixedWidth(w);
	screenSansFE->setFixedWidth(w);
	screenTypewriterFE->setFixedWidth(w);
}


/////////////////////////////////////////////////////////////////////
//
// PrefColors
//
/////////////////////////////////////////////////////////////////////


PrefColors::PrefColors(GuiPreferences * form)
	: PrefModule(catLookAndFeel, N_("Colors"), form)
{
	setupUi(this);

	// FIXME: all of this initialization should be put into the controller.
	// See http://www.mail-archive.com/lyx-devel@lists.lyx.org/msg113301.html
	// for some discussion of why that is not trivial.
	for (int i = 0; i < Color_ignore; ++i) {
		ColorCode lc = static_cast<ColorCode>(i);
		if (lc == Color_none
		    || lc == Color_black
		    || lc == Color_white
		    || lc == Color_blue
		    || lc == Color_brown
		    || lc == Color_cyan
		    || lc == Color_darkgray
		    || lc == Color_gray
		    || lc == Color_green
		    || lc == Color_lightgray
		    || lc == Color_lime
		    || lc == Color_magenta
		    || lc == Color_olive
		    || lc == Color_orange
		    || lc == Color_pink
		    || lc == Color_purple
		    || lc == Color_red
		    || lc == Color_teal
		    || lc == Color_violet
		    || lc == Color_yellow
		    || lc == Color_inherit
		    || lc == Color_ignore)
			continue;
		lcolors_.push_back(lc);
	}
	sort(lcolors_.begin(), lcolors_.end(), ColorSorter);
	curcolors_.resize(lcolors_.size());
	newcolors_.resize(lcolors_.size());
	theme_colors_.resize(lcolors_.size());

	undo_stack_ = new QUndoStack(this);

	// setup shortcuts
	QShortcut* sc_undo = new QShortcut(QKeySequence(QKeySequence::Undo), this);
	QShortcut* sc_redo = new QShortcut(QKeySequence(QKeySequence::Redo), this);
	QShortcut* sc_search =
	        new QShortcut(QKeySequence(QKeySequence::Find), this);

	initializeColorsTV();
	initializeThemes();
	initializeThemeMenu();

	// End initialization

	connect(form_, SIGNAL(rejected()),
	        this, SLOT(onRejected()));
	connect(autoapplyCB, SIGNAL(toggled(bool)),
	        this, SLOT(changeAutoapply()));
	connect(bothColorResetPB, SIGNAL(clicked()),
	        this, SLOT(resetColors()));
	connect(clearFilterPB, SIGNAL(clicked()),
	        this, SLOT(clearFilter()));
	connect(colorChooserPB, SIGNAL(clicked()),
	        this, SLOT(openColorChooser()));
	connect(colorsTV, SIGNAL(clicked(QModelIndex)),
	        this, SLOT(onColorsTVClicked(QModelIndex)));
	connect(colorResetAllPB, SIGNAL(clicked()),
	        this, SLOT(resetAllColor()));
	connect(darkColorEditPB, SIGNAL(clicked()),
	        this, SLOT(editDarkColor()));
	connect(darkColorResetPB, SIGNAL(clicked()),
	        this, SLOT(resetDarkColor()));
	connect(lightColorEditPB, SIGNAL(clicked()),
	        this, SLOT(editLightColor()));
	connect(lightColorResetPB, SIGNAL(clicked()),
	        this, SLOT(resetLightColor()));
	connect(redoColorPB, SIGNAL(clicked()),
	        undo_stack_, SLOT(redo()));
	connect(removeThemePB, SIGNAL(clicked()),
	        this, SLOT(removeTheme()));
	connect(saveThemePB, SIGNAL(clicked()),
	        this, SLOT(saveTheme()));
	connect(sc_search, SIGNAL(activated()),
	        searchStringEdit, SLOT(setFocus()));
	connect(sc_redo, SIGNAL(activated()),
	        undo_stack_, SLOT(redo()));
	connect(sc_undo, SIGNAL(activated()),
	        undo_stack_, SLOT(undo()));
	connect(searchStringEdit, SIGNAL(textEdited(QString)),
	        this, SLOT(filterByColorName(QString)));
	connect(&selection_model_,
	        SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
	        this, SLOT(selectionChanged(QItemSelection,QItemSelection)));
	connect(syscolorsCB, SIGNAL(toggled(bool)),
	        this, SIGNAL(changed()));
	connect(syscolorsCB, SIGNAL(toggled(bool)),
	        this, SLOT(changeSysColor()));
	connect(themesMenuPB, SIGNAL(clicked()),
	        this, SLOT(openThemeMenu()));
	connect(themesLW, SIGNAL(itemClicked(QListWidgetItem*)),
	        this, SLOT(loadTheme(QListWidgetItem*)));
	connect(themesLW, SIGNAL(currentRowChanged(int)),
	        this, SLOT(loadTheme(int)));
	connect(undoColorPB, SIGNAL(clicked()),
	        undo_stack_, SLOT(undo()));
}


void PrefColors::applyRC(LyXRC & rc) const
{
	LyXRC oldrc = rc;

	for (unsigned int i = 0; i < lcolors_.size(); ++i)
		form_->setColor(lcolors_[i], newcolors_[i]);
	rc.use_system_colors = syscolorsCB->isChecked();

	if (toqstr(rc.ui_theme) != theme_name_)
		rc.ui_theme = theme_name_.toStdString();

	if (oldrc.use_system_colors != rc.use_system_colors)
		guiApp->colorCache().clear();
}


void PrefColors::updateRC(LyXRC const & rc)
{
	for (size_type i = 0; i < lcolors_.size(); ++i) {
		ColorPair colors =
		        guiApp->colorCache().getAll(lcolors_[i], false);
		theme_colors_[i].first  = newcolors_[i].first
		        = curcolors_[i].first  = colors.first.name();
		theme_colors_[i].second = newcolors_[i].second
		        = curcolors_[i].second = colors.second.name();
		setSwatches(i, colors);
	}

	syscolorsCB->setChecked(rc.use_system_colors);
	colorsTV->update();
}


void PrefColors::onRejected()
{
	initializeThemes();
}


void PrefColors::onColorsTVClicked(const QModelIndex index)
{
	if (!index.flags().testFlag(Qt::ItemIsEnabled))
		return;

	int const row = index.row();
	int const column = index.column();

	if (column < 2)
		changeColor(row, column);

	setResetButtonStatus(row);
}


void PrefColors::editLightColor()
{
	int last_selected_row = selected_indexes_.back().row();

	changeColor(last_selected_row, false);
}


void PrefColors::editDarkColor()
{
	int last_selected_row = selected_indexes_.back().row();

	changeColor(last_selected_row, true);
}


void PrefColors::selectionChanged(const QItemSelection &selected,
                                  const QItemSelection &deselected)
{
	selection_model_.select(deselected,
	                        QItemSelectionModel::Deselect);
	selection_model_.select(selected,
	                        QItemSelectionModel::Current |
	                        QItemSelectionModel::Select);
	selected_indexes_ = selection_model_.selectedIndexes();
	setEditButtonStatus();
}


void PrefColors::changeColor(int const &row, bool const &is_dark_mode)
{
	if (initial_edit_) {
		cacheAllThemes();
		initial_edit_ = false;
	}

	QString color;

	if (is_dark_mode)
		color = newcolors_[size_t(row)].second;
	else
		color = newcolors_[size_t(row)].first;

	QColor const c = form_->getColor(QColor(color));

	if (setColor(colorsTV_model_.item(row, is_dark_mode), c, color)) {
		if (is_dark_mode)
			newcolors_[size_t(row)].second = c.name();
		else
			newcolors_[size_t(row)].first  = c.name();
		findThemeFromColorSet();
		// emit signal
		changed();
	}
}


void PrefColors::findThemeFromColorSet()
{
	QString name = "";
	for (int theme_id = 0; theme_id < themesLW->count(); ++theme_id) {
		if (checkMatchWithTheme(theme_id)) {
			// all colors matched
			name = theme_names_cache_[theme_id];
			break;
		}
	}
	theme_name_ = name;
	selectCurrentTheme(theme_name_);
}


bool PrefColors::checkMatchWithTheme(int const theme_id)
{
	if (themes_cache_.empty())
		cacheAllThemes();

	for (int col = 0; col < colorsTV_model_.rowCount(); ++col) {
		if (newcolors_[col] != themes_cache_[theme_id][col])
			return false;
	}
	return true;
}


void PrefColors::dismissCurrentTheme()
{
	theme_name_ = theme_filename_ = "";
	themesLW->setCurrentRow(themesLW->currentRow(),
	                        QItemSelectionModel::Deselect);
}


bool PrefColors::setSwatch(QStandardItem *item, QColor const &color)
{
	QModelIndex index = colorsTV_model_.indexFromItem(item);

	return colorsTV_model_.setData(index, QVariant(color), Qt::DecorationRole);
}


bool PrefColors::setSwatches(size_type const &row, ColorPair colors)
{
	bool res1 = setSwatch(colorsTV_model_.item(row, 0), colors.first);
	bool res2 = setSwatch(colorsTV_model_.item(row, 1), colors.second);
	return res1 && res2;
}


void PrefColors::updateAllSwatches()
{
	for (size_type row = 0; row < lcolors_.size(); ++row)
		setSwatches(row, toqcolor(newcolors_[row]));
}


bool PrefColors::resetColor(bool const is_dark_color)
{
	bool result = true;
	bool item_result;

	for (QModelIndex selected : std::as_const(selected_indexes_))
	{
		int const row = selected.row();
		item_result = resetColor(row, is_dark_color);
		result &= item_result;
	}

	return result;
}


bool PrefColors::resetColor(int const row, bool const is_dark_color)
{
	QColor const c = getCurrentThemeColor(row, is_dark_color);
	if (!c.isValid()) return false;

	QStandardItem *item; // ColorColumn
	bool result;

	item = colorsTV_model_.item(row, is_dark_color);

	if (is_dark_color)
		result = setColor(item, c, newcolors_[size_t(row)].second);
	else
		result = setColor(item, c, newcolors_[size_t(row)].first);

	return result;
}


bool PrefColors::resetColors()
{
	bool result = true;
	result &= resetColor(true);
	result &= resetColor(false);
	return result;
}


bool PrefColors::resetAllColor()
{
	guiApp->setOverrideCursor(QCursor(Qt::WaitCursor));

	bool isChanged = false;

	colorResetAllPB->setDisabled(true);
	lightColorResetPB->setDisabled(true);
	darkColorResetPB->setDisabled(true);

	for (int row = 0, count = colorsTV_model_.rowCount(); row < count; ++row) {
		isChanged |= resetColor(row, false);
		isChanged |= resetColor(row, true);
	}

	if (isChanged) {
		// emit signal
		changed();
	}
	guiApp->restoreOverrideCursor();

	return isChanged;
}


bool PrefColors::setColor(QStandardItem *pitem,
                          QColor const &new_color, QString const &old_color)
{
	if (new_color.isValid() && new_color.name() != old_color) {
		QUndoCommand* setColorCmd =
		        new SetColor(pitem, new_color, old_color, newcolors_,
		                     autoapply_, this);
		undo_stack_->push(setColorCmd);
		return true;
	}
	return false;
}


void PrefColors::setUndoRedoButtonStatuses(bool isUndoing)
{
	// Undo button
	if ((!isUndoing && undo_stack_->index() >= 0) ||
	        (isUndoing && undo_stack_->index() >= 2))
		undoColorPB->setEnabled(true);
	else
		undoColorPB->setDisabled(true);

	// Redo button
	if ((!isUndoing &&
	     undo_stack_->index() < undo_stack_->count() - 1) ||
	        (isUndoing &&
	         undo_stack_->index() <= undo_stack_->count()))
		redoColorPB->setEnabled(true);
	else
		redoColorPB->setDisabled(true);
}


void PrefColors::setResetButtonStatus(int const &row, bool is_undoing)
{
	// light color
	QColor theme_color = getCurrentThemeColor(row, false);
	QColor new_color = (QColor)newcolors_[size_t(row)].first;
	if (new_color != theme_color)
		lightColorResetPB->setEnabled(true);
	else
		lightColorResetPB->setEnabled(false);

	// dark color
	theme_color = getCurrentThemeColor(row, true);
	new_color = (QColor)newcolors_[size_t(row)].second;
	if (new_color != theme_color)
		darkColorResetPB->setEnabled(true);
	else
		darkColorResetPB->setEnabled(false);

	// both color button
	if (lightColorResetPB->isEnabled() && darkColorResetPB->isEnabled())
		bothColorResetPB->setEnabled(true);
	else
		bothColorResetPB->setEnabled(false);

	// reset all button
	if ((!is_undoing && undo_stack_->index() < 0) ||
	        (is_undoing && undo_stack_->index() < 2))
		colorResetAllPB->setEnabled(false);
	else
		colorResetAllPB->setEnabled(true);

	changed();
}


void PrefColors::setResetButtonStatus(bool is_undoing)
{
	QModelIndex index = selection_model_.currentIndex();
	int const row = index.row();

	setResetButtonStatus(row, is_undoing);
}


void PrefColors::setEditButtonStatus()
{
	if (selection_model_.hasSelection()) {
		lightColorEditPB->setEnabled(true);
		darkColorEditPB->setEnabled(true);
	} else {
		lightColorEditPB->setEnabled(false);
		darkColorEditPB->setEnabled(false);
	}
}


void PrefColors::exportTheme()
{
	// ask for directory to export
	QString home_dir = toqstr(package().get_home_dir().absFileName());
	FileDialog dialog(qt_("Export a color theme"));
	FileDialog::Result result =
			dialog.save((home_dir == "") ? "/" : home_dir, {"*.theme", "*.*"},
						theme_filename_);
	if (result.first == FileDialog::Chosen)
		saveExportThemeCommon(result.second);
}


void PrefColors::saveTheme()
{
	while (true) {
		// this sets theme_filename_
		if (!askThemeName(false))
			break;

		std::string file_path =
				addName(
					addPath(package().user_support().absFileName(), "themes"),
					fromqstr(theme_filename_));

		QFile target_file(toqstr(file_path));
		if (!target_file.exists() || wantToOverwrite()) {
			saveExportThemeCommon(toqstr(file_path));
			initial_edit_ = true;
			break;
		}
	}
}


bool PrefColors::askThemeName(bool porting, QString name_suggestion)
{
	QString prompt = porting ?
	            qt_("Enter the name of the color theme.") :
	            qt_("Enter the name of the new color theme to save.");
	QString suggestion = (name_suggestion != "") ? name_suggestion : theme_name_;
	bool ok;
	theme_name_ =
	        QInputDialog::getText(this, qt_("Name the color theme"),
	                              prompt, QLineEdit::Normal,
	                              (suggestion == "") ? qt_("New theme name")
	                                                  : suggestion, &ok);

	if (ok && !theme_name_.isEmpty()) {
		// Makefile cannot handle spaces in filenames directly
		// so replace it with an underscore same as other places
		QString tmp_name = theme_name_;
		tmp_name.replace(' ', '_');
		theme_filename_ = tmp_name + ".theme";
	} else {
		activatePrefsWindow(form_);
		return false;
	}

	return true;
}


bool PrefColors::wantToOverwrite()
{
	QMessageBox msgBox;
	msgBox.setIcon(QMessageBox::Warning);
	msgBox.setText(qt_("A user color theme with the same name exists."));
	msgBox.setInformativeText(qt_("Do you want to overwrite the theme?"));
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	msgBox.setDefaultButton(QMessageBox::No);
	int ret = msgBox.exec();
	if (ret == QMessageBox::No)
		return false;
	else
		return true;
}


void PrefColors::saveExportThemeCommon(QString file_path)
{
	ofstream ofs(fromqstr(file_path));

	ofs << "#LyX version: " << lyx_version << "\n#\n" <<
	       "#   This is a definition file of color theme \"" <<
	            fromqstr(theme_name_) << "\" of LyX\n" <<
	       "#\n" <<
	       "#   Author: " << form_->rc().user_name <<
	                " (" << form_->rc().user_email << ")\n" <<
	       "#\n" <<
	       "#   Syntax: \\set_color \"color name\" \"light color\" " <<
	            "\"dark color\"\n" <<
	       "#\n";

	// sync with LyXRC::write
	for (size_type i = 0; i < lcolors_.size(); ++i) {
		ofs << "\\set_color \"" << lcolor.getLyXName(lcolors_[i]) << "\" \""
		    << fromqstr(newcolors_[i].first) << "\" \""
		    << fromqstr(newcolors_[i].second) << "\"\n";
	}
	ofs.close();
	initializeThemesLW();
	cacheAllThemes();
	selectCurrentTheme(theme_name_);
	activatePrefsWindow(form_);
}


void PrefColors::importTheme()
{
	QString home_dir = toqstr(package().get_home_dir().absFileName());
	FileDialog dialog(qt_("Import a color theme"));
	FileDialog::Result result =
	        dialog.open((home_dir == "") ? "/" : home_dir, {"*.theme", "*.*"});
	QString file_path;
	if (result.first == FileDialog::Chosen)
		file_path = result.second;
	else
		return;

	QFile import_file(file_path);
	std::string target_file_path;

	while (true) {
		if (askThemeName(true, removeExtension(onlyFileName(file_path)))) {
			target_file_path =
			        addName(
			            addPath(package().user_support().absFileName(), "themes"),
			            fromqstr(theme_filename_));
			QFile target_file(toqstr(target_file_path));
			if (!target_file.exists())
				break;
			else if (wantToOverwrite()) {
				target_file.remove();
				break;
			}
		} else
			return;
	}

	// copy to user theme dir
	import_file.copy(toqstr(target_file_path));

	// list up all theme files in the themes directory in themesLW
	initializeThemesLW();
	// cache all themes in themesLW
	cacheAllThemes();
	// update theme indicator
	selectCurrentTheme(theme_name_, true);

	ColorNamePairs colors = readTheme(FileName(fromqstr(file_path)));
	theme_colors_ = newcolors_ = colors;
	theme_filename_ = onlyFileName(toqstr(target_file_path));
	theme_name_ = removeExtension(theme_filename_).replace('_', ' ');
	initial_edit_ = true;

	loadImportThemeCommon(colors);

	return;
}


void PrefColors::loadTheme(int const row)
{
	if (row < 0) return;

	theme_colors_ = newcolors_ = themes_cache_[row];
	// variables below are used for suggestion in input dialogs
	theme_filename_ = onlyFileName(theme_fullpaths_[row]);
	theme_name_ = removeExtension(theme_filename_).replace('_', ' ');
	loadImportThemeCommon(theme_colors_);
}


void PrefColors::loadTheme(QListWidgetItem *item)
{
	loadTheme(themesLW->row(item));
}


void PrefColors::cacheAllThemes()
{
	guiApp->setOverrideCursor(QCursor(Qt::WaitCursor));
	themes_cache_.clear();
	theme_names_cache_.clear();
	// readTheme() changes form_->rc()
	LyXRC origrc = form_->rc();
	for (int id = 0; id < themesLW->count(); ++id) {
		FileName const fn(fromqstr(theme_fullpaths_[id]));
		themes_cache_.push_back(readTheme(fn));
		theme_names_cache_.push_back(themesLW->item(id)->text());
	}
	form_->rc() = origrc;
	guiApp->restoreOverrideCursor();
}


ColorNamePairs PrefColors::readTheme(FileName fullpath)
{
	ColorNamePairs colors;
	colors.resize(lcolors_.size());
	// read RC colors to extern ColorSet lcolor
	form_->rc().read(fullpath, true);
	for (size_type row = 0; row < lcolors_.size(); ++row) {
		// get colors from extern lcolor
		colors[size_t(row)] =
		    {getCurrentColor(lcolors_[row], false).name(),
		     getCurrentColor(lcolors_[row], true).name()};
	}
	return colors;
}


void PrefColors::loadImportThemeCommon(ColorNamePairs & colors)
{
	updateAllSwatches();

	if (autoapply_) {
		for (unsigned int i = 0; i < lcolors_.size(); ++i) {
			form_->setColor(lcolors_[i], colors[i]);
		}
		form_->dispatchParams();
	}

	// emit signal
	changed();
	activatePrefsWindow(form_);
}


void PrefColors::removeTheme()
{
	const QListWidgetItem* cur_item = themesLW->currentItem();
	// if theme name is empty, urge the user to select it from the dropdown menu
	if (cur_item == nullptr) {
		QMessageBox msgBox(this);
		msgBox.setIcon(QMessageBox::Critical);
		msgBox.setWindowTitle(qt_("Select a user theme"));
		msgBox.setText(qt_("Please select a user theme to remove "
		                   "from the \"Themes\" list menu."));
		msgBox.setStandardButtons(QMessageBox::Ok);
		msgBox.exec();
		return;
	}

	const int cur_row = themesLW->row(cur_item);
	const QString theme_name = cur_item->text();
	if (isSysThemes_[cur_row]) {
		QMessageBox msgBox(this);
		msgBox.setIcon(QMessageBox::Critical);
		msgBox.setWindowTitle(qt_("Not a user theme"));
		msgBox.setText(qt_("The selected theme is a system theme. "
		                   "It cannot be removed."));
		msgBox.setStandardButtons(QMessageBox::Ok);
		msgBox.exec();
		return;
	}

	// confirmation to delete
	QMessageBox msgBox(this);
	msgBox.setIcon(QMessageBox::Warning);
	msgBox.setWindowTitle(qt_("Confirm the removal"));
	msgBox.setText(toqstr(bformat(_("Do you really want to remove the theme \"%1$s\"?"),
				      qstring_to_ucs4(theme_name))));
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	msgBox.setDefaultButton(QMessageBox::No);

	if (msgBox.exec() == QMessageBox::Yes) {
		dismissCurrentTheme();
		QFile file(theme_fullpaths_[cur_row]);
		file.remove();
		theme_name_ = "";
		initializeThemesLW();
		cacheAllThemes();
		initial_edit_ = true;
	}
}


void PrefColors::initializeThemes()
{
	// note that form_->rc() is not initialized yet here when pref dialog is
	// opened for the first time
	if (form_->rc().ui_theme.empty()) {
		if (toqstr(lyxrc.ui_theme) != theme_name_ || theme_name_ == "")
			theme_name_ = toqstr(lyxrc.ui_theme);
	} else if (toqstr(form_->rc().ui_theme) != theme_name_ || theme_name_ == "") {
		theme_name_ = toqstr(form_->rc().ui_theme);
	}
	initializeThemesLW();
	updateRC(lyxrc);
	findThemeFromColorSet();
}


void PrefColors::initializeThemesLW()
{
	//
	// initialize themes list widget
	//
	// clear dataset
	theme_fullpaths_.clear();
	isSysThemes_.clear();

	const QIcon & sys_theme_icon =
	        QIcon(toqstr(addPathName(package().system_support().absFileName(),
	                                 "images/oxygen/float-insert_figure.svgz")));
	const QIcon & usr_theme_icon =
	        QIcon(toqstr(addPathName(package().system_support().absFileName(),
	                                 "images/oxygen/change-accept.svgz")));

	// key:   theme's sort key (usually equal to GUI name)
	// value: tuple<GUI name, theme's file name, bool if a user theme>
	std::map <QString, std::tuple<QString, QString, bool>> themes;

	FileName sys_theme_dir;
	FileName usr_theme_dir;
	sys_theme_dir.set(addName(package().system_support().absFileName(),
	                          "themes").c_str());
	usr_theme_dir.set(addName(package().user_support().absFileName(),
	                          "themes").c_str());
	const FileNameList sys_theme_files = sys_theme_dir.dirList("theme");
	const FileNameList usr_theme_files = usr_theme_dir.dirList("theme");
	for (const FileName & file : sys_theme_files) {
		QString guiname =
		        QString(file.onlyFileNameWithoutExt().c_str()).replace('_', ' ');
		QString sortkey = guiname;
		// if theme name matches the dictionary, use it for translation
		ThemeNameDic::iterator dic_it = theme_name_dic_.find(guiname);
		if (dic_it != theme_name_dic_.end()){
			sortkey = dic_it->second.first;
			guiname = dic_it->second.second;
		}

		const QString filename = file.absFileName().c_str();

		// four labeling chars are appended to guiname to gurantee both system
		// and user themes are listed even if they have the same name
		themes.emplace(sortkey + "_sys", make_tuple(guiname, filename, false));
	}
	for (const FileName & file : usr_theme_files) {
		const QString guiname =
		        QString(file.onlyFileNameWithoutExt().c_str()).replace('_', ' ');
		const QString filename = file.absFileName().c_str();
		// four labeling chars are appended to guiname to gurantee both system
		// and user themes are listed even if they have the same name
		themes.emplace(guiname + "_usr", std::make_tuple(guiname, filename, true));
	}
	themesLW->clear();

	// themes are already sorted with GUI name as std::map sorts its entries
	for (const auto & theme : themes) {
		QListWidgetItem* item = new QListWidgetItem;
		item->setText(std::get<0>(theme.second));
		if (std::get<2>(theme.second)) // if the theme is a user theme
			item->setIcon(usr_theme_icon);
		else
			item->setIcon(sys_theme_icon);
		themesLW->addItem(item);

		theme_fullpaths_.push_back(std::get<1>(theme.second));
		if (theme.first.right(3) == "sys")
			isSysThemes_.push_back(true);
		else
			isSysThemes_.push_back(false);
	}
}


void PrefColors::selectCurrentTheme(QString theme_name_en, bool user_theme_only)
{
	// note that themesLW->findItems() matches translated theme name
	// whereas theme_name_en contains untranslated one
	ThemeNameDic::iterator dic_it = theme_name_dic_.find(theme_name_en);
	QString translated_name;
	if (dic_it != theme_name_dic_.end())
		translated_name = dic_it->second.second;
	else
		translated_name = theme_name_en;

	QList<QListWidgetItem *> selected_items =
	        themesLW->findItems(translated_name, Qt::MatchExactly);
	if (selected_items.empty())
		dismissCurrentTheme();
	else if (user_theme_only) {
		bool found = false;
		for (auto item : std::as_const(selected_items))
			if (isSysThemes_[themesLW->row(item)] == false) {
				themesLW->setCurrentItem(item);
				found = true;
				break;
			}
		LATTEST(found);
	} else
		themesLW->setCurrentItem(selected_items.first());
}


void PrefColors::initializeThemeMenu()
{
	QAction* ac_export = new QAction(qt_("&Export..."), themesMenuPB);
	QAction* ac_import = new QAction(qt_("&Import..."), themesMenuPB);
	theme_menu_.addAction(ac_export);
	theme_menu_.addAction(ac_import);
	connect(ac_export, &QAction::triggered,
	        this, &PrefColors::exportTheme);
	connect(ac_import, &QAction::triggered,
	        this, &PrefColors::importTheme);
}


void PrefColors::openThemeMenu()
{
	QPoint pos = mapToGlobal(QPoint(themesMenuPB->x() + themesMenuPB->width(),
	                                themesMenuPB->y()));
	theme_menu_.exec(pos);
}


void PrefColors::initializeColorsTV()
{
	// The table
	// Only color names are listed here. Colors will be set at updateRC().
	for (int row=0; row<(int)lcolors_.size(); ++row) {
		for (int column = 0; column < header_labels_.length(); ++column) {
			QStandardItem* item = new QStandardItem();
			if (column == ColorNameColumn) {
				item->setText(toqstr(lcolor.getGUIName(lcolors_[row])));
				item->setTextAlignment(Qt::AlignLeft);
				item->setToolTip(item->text());
			} else if (column == LightColorColumn)
				item->setToolTip(qt_("Click here to change the color in the light mode"));
			else if (column == DarkColorColumn)
				item->setToolTip(qt_("Click here to change the color in the dark mode"));
			item->setEditable(false);
			colorsTV_model_.setItem(row, column, item);
		}
	}
	colorsTV_model_.setHorizontalHeaderLabels(header_labels_);

	colorsTV->setModel(&colorsTV_model_);

	ColorSwatchDelegate *delegate = new ColorSwatchDelegate(this);
	colorsTV->setItemDelegate(delegate);

	// Headers
	// we don't set a model for vertical header since it is hidden
	QHeaderView* vertical_header = new QHeaderView(Qt::Vertical);
	vertical_header->setDefaultSectionSize(swatch_height_);
	vertical_header->setSectionResizeMode(QHeaderView::ResizeToContents);
	vertical_header->setDefaultAlignment(Qt::AlignCenter);
	colorsTV->verticalHeader()->hide();

	QHeaderView * horizontal_header = new QHeaderView(Qt::Horizontal);
	horizontal_header->setModel(&colorsTV_model_);
	horizontal_header->setSectionResizeMode(QHeaderView::Fixed);
	horizontal_header->setSectionResizeMode(ColorNameColumn, QHeaderView::Stretch);
	horizontal_header->setDefaultAlignment(Qt::AlignCenter);
	horizontal_header->setDefaultSectionSize(swatch_width_);
	colorsTV->setHorizontalHeader(horizontal_header);

	QFont font;
	QFontMetrics fm(font);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	int col_width = max(fm.horizontalAdvance(header_labels_[LightColorColumn]),
	                    swatch_width_);
	colorsTV->setColumnWidth(LightColorColumn, col_width);
	col_width = max(fm.horizontalAdvance(header_labels_[DarkColorColumn]),
	                swatch_width_);
#else
	int col_width = max(fm.width(header_labels_[LightColorColumn]),
	                    swatch_width_);
	colorsTV->setColumnWidth(LightColorColumn, col_width);
	col_width = max(fm.width(header_labels_[DarkColorColumn]),
	                swatch_width_);
#endif
	colorsTV->setColumnWidth(DarkColorColumn, col_width);
	colorsTV->setSelectionMode(QAbstractItemView::ExtendedSelection);
	colorsTV->setSelectionBehavior(QAbstractItemView::SelectRows);

	colorsTV->resizeRowsToContents();
	for (int column=0; column < 2; ++column)
		colorsTV->resizeColumnToContents(column);

	// Selection
	selection_model_.setModel(&colorsTV_model_);
	colorsTV->setSelectionModel(&selection_model_);

	undo_stack_->clear();
	undo_stack_->setClean();

	// initialize control buttons
	if (undo_stack_->canUndo())
		undoColorPB->setEnabled(true);
	else
		undoColorPB->setDisabled(true);
	if (undo_stack_->canRedo())
		redoColorPB->setEnabled(true);
	else
		redoColorPB->setDisabled(true);
	// Below buttons are unabled as no colors are selected
	lightColorResetPB->setDisabled(true);
	darkColorResetPB->setDisabled(true);
	bothColorResetPB->setDisabled(true);
	colorResetAllPB->setDisabled(true);
	lightColorEditPB->setDisabled(true);
	darkColorEditPB->setDisabled(true);

	colorsTV->show();
}


QColor PrefColors::getCurrentColor(ColorCode color_code, bool is_dark_mode)
{
	return lcolor.getX11HexName(color_code, is_dark_mode).c_str();
}


QColor PrefColors::getCurrentThemeColor(int const &row,
                                        bool const &is_dark_color)
{
	QColor color;

	if (!theme_colors_.empty()) {
		if (is_dark_color)
			color = (QColor)theme_colors_[row].second;
		else
			color = (QColor)theme_colors_[row].first;
	}
	return color;
}


ColorPair PrefColors::getCurrentThemeColors(int const &row)
{
	return theme_colors_[row];
}


void PrefColors::changeSysColor()
{
	for (int row = 0 ; row < colorsTV_model_.rowCount() ; ++row) {
		// skip colors that are taken from system palette
		bool const disable = syscolorsCB->isChecked()
			&& guiApp->colorCache().isSystem(lcolors_[size_t(row)]);

		for (int column = 0 ; column <= ColorNameColumn ; ++column) {
			QStandardItem * const item = colorsTV_model_.item(row, column);
			Qt::ItemFlags const flags = item->flags();

			if (disable)
				item->setFlags(flags & ~Qt::ItemIsEnabled);
			else
				item->setFlags(flags | Qt::ItemIsEnabled);
		}
	}
	if (autoapply_) {
		applyRC(form_->rc());
		form_->dispatchParams();
	}
}


void PrefColors::changeAutoapply()
{
	autoapply_ = autoapplyCB->isChecked() ? true : false;
	if (autoapply_) {
		applyRC(form_->rc());
		form_->dispatchParams();
	}
}


void PrefColors::filterByColorName(const QString &text) const
{
	searchStringEdit->setStyleSheet("");

	const QList<QStandardItem *> items_found =
	        colorsTV_model_.findItems(text, Qt::MatchContains,
	                                  ColorNameColumn);
	if (!items_found.empty())
		filterCommon(items_found);
	else
		searchStringEdit->setStyleSheet("color: red;");

	form_->raise();
}


void PrefColors::filterByColor(const QColor &color)
{
	QList<QStandardItem *>rows_found;
	for (int i=0; i<colorsTV_model_.rowCount(); ++i) {
		if (colorsTV_model_.item(i, 0)->data(Qt::DecorationRole).value<QColor>().name()
		        == color.name() ||
		        colorsTV_model_.item(i, 1)->data(Qt::DecorationRole).value<QColor>().name()
		        == color.name()) {
			rows_found.push_back(colorsTV_model_.item(i));
		}
	}
	if (!rows_found.empty())
		filterCommon(rows_found);
	else {
		QMessageBox msgBox(form_);
		msgBox.setIcon(QMessageBox::Information);
		msgBox.setText(qt_("Such a color is not found"));
		msgBox.exec();
	}

	form_->raise();
}


void PrefColors::filterCommon(QList<QStandardItem *> const & items_found) const
{
	for (size_type row = 0; row < lcolors_.size(); ++row)
		colorsTV->hideRow(row);
	for (QStandardItem* item : std::as_const(items_found))
		colorsTV->showRow(item->row());
}


void PrefColors::clearFilter() {
	for (size_type row = 0; row < lcolors_.size(); ++row)
		colorsTV->showRow(row);
	searchStringEdit->clear();
	searchStringEdit->setStyleSheet("");
}


void PrefColors::openColorChooser()
{
	QColorDialog cdlg(form_);
	QColor initial_color = Qt::white;

	if (!selected_indexes_.empty()) {
		if (!guiApp->colorCache().isDarkMode())
			initial_color = theme_colors_[selected_indexes_.first().row()].first;
		else
			initial_color = theme_colors_[selected_indexes_.first().row()].second;
	}

	// open color dialog
	QColor color = cdlg.getColor(initial_color, form_);

	form_->raise();

	if (color.isValid())
		filterByColor(color);
}


ColorPair PrefColors::toqcolor(ColorNamePair colors)
{
	return {QColor(colors.first), QColor(colors.second)};
}


/////////////////////////////////////////////////////////////////////
//
// PrefDisplay
//
/////////////////////////////////////////////////////////////////////

PrefDisplay::PrefDisplay(GuiPreferences * form)
	: PrefModule(catLookAndFeel, N_("Display"), form)
{
	setupUi(this);
	connect(displayGraphicsCB, SIGNAL(toggled(bool)), this, SIGNAL(changed()));
	connect(instantPreviewCO, SIGNAL(activated(int)), this, SIGNAL(changed()));
	connect(previewSizeSB, SIGNAL(valueChanged(double)), this, SIGNAL(changed()));
	connect(paragraphMarkerCB, SIGNAL(toggled(bool)), this, SIGNAL(changed()));
	connect(ctAdditionsUnderlinedCB, SIGNAL(toggled(bool)), this, SIGNAL(changed()));
	connect(bookmarksCO, SIGNAL(activated(int)), this, SIGNAL(changed()));
	bookmarksCO->addItem(qt_("No[[bookmarks]]"), "none");
	bookmarksCO->addItem(qt_("Inline"), "inline");
	bookmarksCO->addItem(qt_("In Margin"), "margin");
	connect(justCB, SIGNAL(toggled(bool)), this, SIGNAL(changed()));
}


void PrefDisplay::on_instantPreviewCO_currentIndexChanged(int index)
{
	previewSizeSB->setEnabled(index != 0);
}


void PrefDisplay::applyRC(LyXRC & rc) const
{
	switch (instantPreviewCO->currentIndex()) {
		case 0:
			rc.preview = LyXRC::PREVIEW_OFF;
			break;
		case 1:
			rc.preview = LyXRC::PREVIEW_NO_MATH;
			break;
		case 2:
			rc.preview = LyXRC::PREVIEW_ON;
			break;
	}

	rc.display_graphics = displayGraphicsCB->isChecked();
	rc.preview_scale_factor = previewSizeSB->value();
	rc.paragraph_markers = paragraphMarkerCB->isChecked();
	rc.ct_additions_underlined = ctAdditionsUnderlinedCB->isChecked();
	rc.workarea_justify = justCB->isChecked();
	QString const bm = bookmarksCO->itemData(bookmarksCO->currentIndex()).toString();
	if (bm == "inline")
		rc.bookmarks_visibility = LyXRC::BMK_INLINE;
	else if (bm == "margin")
		rc.bookmarks_visibility = LyXRC::BMK_MARGIN;
	else
		rc.bookmarks_visibility = LyXRC::BMK_NONE;

	// FIXME!! The graphics cache no longer has a changeDisplay method.
#if 0
	if (old_value != rc.display_graphics) {
		graphics::GCache & gc = graphics::GCache::get();
		gc.changeDisplay();
	}
#endif
}


void PrefDisplay::updateRC(LyXRC const & rc)
{
	switch (rc.preview) {
	case LyXRC::PREVIEW_OFF:
		instantPreviewCO->setCurrentIndex(0);
		break;
	case LyXRC::PREVIEW_NO_MATH :
		instantPreviewCO->setCurrentIndex(1);
		break;
	case LyXRC::PREVIEW_ON :
		instantPreviewCO->setCurrentIndex(2);
		break;
	}

	displayGraphicsCB->setChecked(rc.display_graphics);
	previewSizeSB->setValue(rc.preview_scale_factor);
	paragraphMarkerCB->setChecked(rc.paragraph_markers);
	justCB->setChecked(rc.workarea_justify);
	ctAdditionsUnderlinedCB->setChecked(rc.ct_additions_underlined);
	bookmarksCO->setCurrentIndex(rc.bookmarks_visibility);
	switch (rc.bookmarks_visibility) {
		case LyXRC::BMK_INLINE:
			bookmarksCO->setCurrentIndex(bookmarksCO->findData("inline"));
			break;
		case LyXRC::BMK_MARGIN:
			bookmarksCO->setCurrentIndex(bookmarksCO->findData("margin"));
			break;
		case LyXRC::BMK_NONE:
			bookmarksCO->setCurrentIndex(bookmarksCO->findData("none"));
			break;
	}
	previewSizeSB->setEnabled(
		rc.display_graphics
		&& rc.preview != LyXRC::PREVIEW_OFF);
}


/////////////////////////////////////////////////////////////////////
//
// PrefPaths
//
/////////////////////////////////////////////////////////////////////

PrefPaths::PrefPaths(GuiPreferences * form)
	: PrefModule(QString(), N_("Paths"), form)
{
	setupUi(this);

	connect(workingDirPB, SIGNAL(clicked()), this, SLOT(selectWorkingdir()));
	connect(workingDirED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	connect(templateDirPB, SIGNAL(clicked()), this, SLOT(selectTemplatedir()));
	connect(templateDirED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	connect(exampleDirPB, SIGNAL(clicked()), this, SLOT(selectExampledir()));
	connect(exampleDirED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	connect(backupDirPB, SIGNAL(clicked()), this, SLOT(selectBackupdir()));
	connect(backupDirED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	connect(lyxserverDirPB, SIGNAL(clicked()), this, SLOT(selectLyxPipe()));
	connect(lyxserverDirED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	connect(thesaurusDirPB, SIGNAL(clicked()), this, SLOT(selectThesaurusdir()));
	connect(thesaurusDirED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	connect(tempDirPB, SIGNAL(clicked()), this, SLOT(selectTempdir()));
	connect(tempDirED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

#if defined(USE_HUNSPELL)
	connect(hunspellDirPB, SIGNAL(clicked()), this, SLOT(selectHunspelldir()));
	connect(hunspellDirED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
#else
	hunspellDirPB->setEnabled(false);
	hunspellDirED->setEnabled(false);
#endif

	connect(pathPrefixED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	connect(texinputsPrefixED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	pathPrefixED->setValidator(new NoNewLineValidator(pathPrefixED));
	texinputsPrefixED->setValidator(new NoNewLineValidator(texinputsPrefixED));
}


void PrefPaths::applyRC(LyXRC & rc) const
{
	rc.document_path = internal_path(fromqstr(workingDirED->text()));
	rc.example_path = internal_path(fromqstr(exampleDirED->text()));
	rc.template_path = internal_path(fromqstr(templateDirED->text()));
	rc.backupdir_path = internal_path(fromqstr(backupDirED->text()));
	rc.tempdir_path = internal_path(fromqstr(tempDirED->text()));
	rc.thesaurusdir_path = internal_path(fromqstr(thesaurusDirED->text()));
	rc.hunspelldir_path = internal_path(fromqstr(hunspellDirED->text()));
	rc.path_prefix = internal_path_list(fromqstr(pathPrefixED->text()));
	rc.texinputs_prefix = internal_path_list(fromqstr(texinputsPrefixED->text()));
	// FIXME: should be a checkbox only
	rc.lyxpipes = internal_path(fromqstr(lyxserverDirED->text()));
}


void PrefPaths::updateRC(LyXRC const & rc)
{
	workingDirED->setText(toqstr(external_path(rc.document_path)));
	exampleDirED->setText(toqstr(external_path(rc.example_path)));
	templateDirED->setText(toqstr(external_path(rc.template_path)));
	backupDirED->setText(toqstr(external_path(rc.backupdir_path)));
	tempDirED->setText(toqstr(external_path(rc.tempdir_path)));
	thesaurusDirED->setText(toqstr(external_path(rc.thesaurusdir_path)));
	hunspellDirED->setText(toqstr(external_path(rc.hunspelldir_path)));
	pathPrefixED->setText(toqstr(external_path_list(rc.path_prefix)));
	texinputsPrefixED->setText(toqstr(external_path_list(rc.texinputs_prefix)));
	// FIXME: should be a checkbox only
	lyxserverDirED->setText(toqstr(external_path(rc.lyxpipes)));
}


void PrefPaths::selectExampledir()
{
	QString file = form_->browseDir(internalPath(exampleDirED->text()),
		qt_("Select directory for example files"));
	if (!file.isEmpty())
		exampleDirED->setText(file);
}


void PrefPaths::selectTemplatedir()
{
	QString file = form_->browseDir(internalPath(templateDirED->text()),
		qt_("Select a document templates directory"));
	if (!file.isEmpty())
		templateDirED->setText(file);
}


void PrefPaths::selectTempdir()
{
	QString file = form_->browseDir(internalPath(tempDirED->text()),
		qt_("Select a temporary directory"));
	if (!file.isEmpty())
		tempDirED->setText(file);
}


void PrefPaths::selectBackupdir()
{
	QString file = form_->browseDir(internalPath(backupDirED->text()),
		qt_("Select a backups directory"));
	if (!file.isEmpty())
		backupDirED->setText(file);
}


void PrefPaths::selectWorkingdir()
{
	QString file = form_->browseDir(internalPath(workingDirED->text()),
		qt_("Select a document directory"));
	if (!file.isEmpty())
		workingDirED->setText(file);
}


void PrefPaths::selectThesaurusdir()
{
	QString file = form_->browseDir(internalPath(thesaurusDirED->text()),
		qt_("Set the path to the thesaurus dictionaries"));
	if (!file.isEmpty())
		thesaurusDirED->setText(file);
}


void PrefPaths::selectHunspelldir()
{
	QString file = form_->browseDir(internalPath(hunspellDirED->text()),
		qt_("Set the path to the Hunspell dictionaries"));
	if (!file.isEmpty())
		hunspellDirED->setText(file);
}


void PrefPaths::selectLyxPipe()
{
	QString file = form_->browse(internalPath(lyxserverDirED->text()),
		qt_("Give a filename for the LyX server pipe"));
	if (!file.isEmpty())
		lyxserverDirED->setText(file);
}


/////////////////////////////////////////////////////////////////////
//
// PrefSpellchecker
//
/////////////////////////////////////////////////////////////////////

PrefSpellchecker::PrefSpellchecker(GuiPreferences * form)
	: PrefModule(catLanguage, N_("Spellchecker"), form)
{
	setupUi(this);

// FIXME: this check should test the target platform (darwin)
#if defined(USE_MACOSX_PACKAGING)
	spellcheckerCB->addItem(qt_("Native"), QString("native"));
#define CONNECT_APPLESPELL
#else
#undef CONNECT_APPLESPELL
#endif
#if defined(USE_ASPELL)
	spellcheckerCB->addItem(qt_("Aspell"), QString("aspell"));
#endif
#if defined(USE_ENCHANT)
	spellcheckerCB->addItem(qt_("Enchant"), QString("enchant"));
#endif
#if defined(USE_HUNSPELL)
	spellcheckerCB->addItem(qt_("Hunspell"), QString("hunspell"));
#endif

	#if defined(CONNECT_APPLESPELL) || defined(USE_ASPELL) || defined(USE_ENCHANT) || defined(USE_HUNSPELL)
		connect(spellcheckerCB, SIGNAL(currentIndexChanged(int)),
			this, SIGNAL(changed()));
		connect(altLanguageED, SIGNAL(textChanged(QString)),
			this, SIGNAL(changed()));
		connect(escapeCharactersED, SIGNAL(textChanged(QString)),
			this, SIGNAL(changed()));
		connect(compoundWordCB, SIGNAL(clicked()),
			this, SIGNAL(changed()));
		connect(spellcheckContinuouslyCB, SIGNAL(clicked()),
			this, SIGNAL(changed()));
		connect(spellcheckNotesCB, SIGNAL(clicked()),
			this, SIGNAL(changed()));

		altLanguageED->setValidator(new NoNewLineValidator(altLanguageED));
		escapeCharactersED->setValidator(new NoNewLineValidator(escapeCharactersED));
	#else
		spellcheckerCB->setEnabled(false);
		altLanguageED->setEnabled(false);
		escapeCharactersED->setEnabled(false);
		compoundWordCB->setEnabled(false);
		spellcheckContinuouslyCB->setEnabled(false);
		spellcheckNotesCB->setEnabled(false);
	#endif
}


void PrefSpellchecker::applyRC(LyXRC & rc) const
{
	string const speller = fromqstr(spellcheckerCB->
		itemData(spellcheckerCB->currentIndex()).toString());
	if (!speller.empty())
		rc.spellchecker = speller;
	rc.spellchecker_alt_lang = fromqstr(altLanguageED->text());
	rc.spellchecker_esc_chars = qstring_to_ucs4(escapeCharactersED->text());
	rc.spellchecker_accept_compound = compoundWordCB->isChecked();
	rc.spellcheck_continuously = spellcheckContinuouslyCB->isChecked();
	rc.spellcheck_notes = spellcheckNotesCB->isChecked();
}


void PrefSpellchecker::updateRC(LyXRC const & rc)
{
	spellcheckerCB->setCurrentIndex(
		spellcheckerCB->findData(toqstr(rc.spellchecker)));
	altLanguageED->setText(toqstr(rc.spellchecker_alt_lang));
	escapeCharactersED->setText(toqstr(rc.spellchecker_esc_chars));
	compoundWordCB->setChecked(rc.spellchecker_accept_compound);
	spellcheckContinuouslyCB->setChecked(rc.spellcheck_continuously);
	spellcheckNotesCB->setChecked(rc.spellcheck_notes);
}


void PrefSpellchecker::on_spellcheckerCB_currentIndexChanged(int index)
{
	QString spellchecker = spellcheckerCB->itemData(index).toString();

	compoundWordCB->setEnabled(spellchecker == QString("aspell"));
}



/////////////////////////////////////////////////////////////////////
//
// PrefConverters
//
/////////////////////////////////////////////////////////////////////


PrefConverters::PrefConverters(GuiPreferences * form)
	: PrefModule(catFiles, N_("Converters"), form)
{
	setupUi(this);

	connect(converterNewPB, SIGNAL(clicked()),
		this, SLOT(updateConverter()));
	connect(converterRemovePB, SIGNAL(clicked()),
		this, SLOT(removeConverter()));
	connect(converterModifyPB, SIGNAL(clicked()),
		this, SLOT(updateConverter()));
	connect(convertersLW, SIGNAL(currentRowChanged(int)),
		this, SLOT(switchConverter()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	connect(converterFromCO, SIGNAL(activated(QString)),
		this, SLOT(changeConverter()));
	connect(converterToCO, SIGNAL(activated(QString)),
		this, SLOT(changeConverter()));
#else
	connect(converterFromCO, SIGNAL(textActivated(QString)),
		this, SLOT(changeConverter()));
	connect(converterToCO, SIGNAL(textActivated(QString)),
		this, SLOT(changeConverter()));
#endif
	connect(converterED, SIGNAL(textEdited(QString)),
		this, SLOT(changeConverter()));
	connect(converterFlagED, SIGNAL(textEdited(QString)),
		this, SLOT(changeConverter()));
	connect(converterNewPB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(converterRemovePB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(converterModifyPB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(maxAgeLE, SIGNAL(textEdited(QString)),
		this, SIGNAL(changed()));
	connect(needauthForbiddenCB, SIGNAL(toggled(bool)),
		this, SIGNAL(changed()));

	converterED->setValidator(new NoNewLineValidator(converterED));
	converterFlagED->setValidator(new NoNewLineValidator(converterFlagED));
	maxAgeLE->setValidator(new QDoubleValidator(0, HUGE_VAL, 6, maxAgeLE));
}


void PrefConverters::applyRC(LyXRC & rc) const
{
	rc.use_converter_cache = cacheCB->isChecked();
	rc.use_converter_needauth_forbidden = needauthForbiddenCB->isChecked();
	rc.use_converter_needauth = needauthCB->isChecked();
	rc.converter_cache_maxage = int(widgetToDouble(maxAgeLE) * 86400.0);
}


static void setCheckboxBlockSignals(QCheckBox *cb, bool checked) {
	cb->blockSignals(true);
	cb->setChecked(checked);
	cb->blockSignals(false);
}


void PrefConverters::updateRC(LyXRC const & rc)
{
	cacheCB->setChecked(rc.use_converter_cache);
	needauthForbiddenCB->setChecked(rc.use_converter_needauth_forbidden);
	setCheckboxBlockSignals(needauthCB, rc.use_converter_needauth);
	QString max_age;
	doubleToWidget(maxAgeLE, (double(rc.converter_cache_maxage) / 86400.0), 'g', 6);
	updateGui();
}


void PrefConverters::updateGui()
{
	QString const pattern("%1 -> %2");
	form_->formats().sort();
	form_->converters().update(form_->formats());
	// save current selection
	QString current =
		pattern
		.arg(converterFromCO->currentText())
		.arg(converterToCO->currentText());

	converterFromCO->clear();
	converterToCO->clear();

	for (Format const & f : form_->formats()) {
		QString const name = toqstr(translateIfPossible(f.prettyname()));
		converterFromCO->addItem(name);
		converterToCO->addItem(name);
	}

	// currentRowChanged(int) is also triggered when updating the listwidget
	// block signals to avoid unnecessary calls to switchConverter()
	convertersLW->blockSignals(true);
	convertersLW->clear();

	for (Converter const & c : form_->converters()) {
		QString const name =
			pattern
			.arg(toqstr(translateIfPossible(c.From()->prettyname())))
			.arg(toqstr(translateIfPossible(c.To()->prettyname())));
		int type = form_->converters().getNumber(c.From()->name(),
		                                         c.To()->name());
		new QListWidgetItem(name, convertersLW, type);
	}
	convertersLW->sortItems(Qt::AscendingOrder);
	convertersLW->blockSignals(false);

	// restore selection
	if (current != pattern.arg(QString()).arg(QString())) {
		QList<QListWidgetItem *> const item =
			convertersLW->findItems(current, Qt::MatchExactly);
		if (!item.isEmpty())
			convertersLW->setCurrentItem(item.at(0));
	}

	// select first element if restoring failed
	if (convertersLW->currentRow() == -1)
		convertersLW->setCurrentRow(0);

	updateButtons();
}


void PrefConverters::switchConverter()
{
	int const cnr = convertersLW->currentItem()->type();
	Converter const & c(form_->converters().get(cnr));
	converterFromCO->setCurrentIndex(form_->formats().getNumber(c.from()));
	converterToCO->setCurrentIndex(form_->formats().getNumber(c.to()));
	converterED->setText(toqstr(c.command()));
	converterFlagED->setText(toqstr(c.flags()));

	updateButtons();
}


void PrefConverters::changeConverter()
{
	updateButtons();
}


void PrefConverters::updateButtons()
{
	if (form_->formats().empty())
		return;
	Format const & from = form_->formats().get(converterFromCO->currentIndex());
	Format const & to = form_->formats().get(converterToCO->currentIndex());
	int const sel = form_->converters().getNumber(from.name(), to.name());
	bool const known = sel >= 0;
	bool const valid = !(converterED->text().isEmpty()
		|| from.name() == to.name());

	string old_command;
	string old_flag;

	if (convertersLW->count() > 0) {
		int const cnr = convertersLW->currentItem()->type();
		Converter const & c = form_->converters().get(cnr);
		old_command = c.command();
		old_flag = c.flags();
	}

	string const new_command = fromqstr(converterED->text());
	string const new_flag = fromqstr(converterFlagED->text());

	bool modified = (old_command != new_command || old_flag != new_flag);

	converterModifyPB->setEnabled(valid && known && modified);
	converterNewPB->setEnabled(valid && !known);
	converterRemovePB->setEnabled(known);

	maxAgeLE->setEnabled(cacheCB->isChecked());
	maxAgeLA->setEnabled(cacheCB->isChecked());
}


// FIXME: user must
// specify unique from/to or it doesn't appear. This is really bad UI
// this is why we can use the same function for both new and modify
void PrefConverters::updateConverter()
{
	Format const & from = form_->formats().get(converterFromCO->currentIndex());
	Format const & to = form_->formats().get(converterToCO->currentIndex());
	string const flags = fromqstr(converterFlagED->text());
	string const command = fromqstr(converterED->text());

	Converter const * old =
		form_->converters().getConverter(from.name(), to.name());
	form_->converters().add(from.name(), to.name(), command, flags);

	if (!old)
		form_->converters().updateLast(form_->formats());

	updateGui();

	// Remove all files created by this converter from the cache, since
	// the modified converter might create different files.
	ConverterCache::get().remove_all(from.name(), to.name());
}


void PrefConverters::removeConverter()
{
	Format const & from = form_->formats().get(converterFromCO->currentIndex());
	Format const & to = form_->formats().get(converterToCO->currentIndex());
	form_->converters().erase(from.name(), to.name());

	updateGui();

	// Remove all files created by this converter from the cache, since
	// a possible new converter might create different files.
	ConverterCache::get().remove_all(from.name(), to.name());
}


void PrefConverters::on_cacheCB_stateChanged(int state)
{
	maxAgeLE->setEnabled(state == Qt::Checked);
	maxAgeLA->setEnabled(state == Qt::Checked);
	changed();
}


void PrefConverters::on_needauthForbiddenCB_toggled(bool checked)
{
	needauthCB->setEnabled(!checked);
}


void PrefConverters::on_needauthCB_toggled(bool checked)
{
	if (checked) {
		changed();
		return;
	}

	int ret = frontend::Alert::prompt(
		_("SECURITY WARNING!"), _("Unchecking this option has the effect that potentially harmful converters would be run without asking your permission first. This is UNSAFE and NOT recommended, unless you know what you are doing. Are you sure you would like to proceed? The recommended and safe answer is NO!"),
		0, 0, _("&No"), _("&Yes"));
	activatePrefsWindow(form_);
	if (ret == 1)
		changed();
	else
		setCheckboxBlockSignals(needauthCB, true);
}


/////////////////////////////////////////////////////////////////////
//
// FormatValidator
//
/////////////////////////////////////////////////////////////////////

class FormatValidator : public QValidator
{
public:
	FormatValidator(QWidget *, Formats const & f);
	void fixup(QString & input) const override;
	QValidator::State validate(QString & input, int & pos) const override;
private:
	virtual QString toString(Format const & format) const = 0;
	int nr() const;
	Formats const & formats_;
};


FormatValidator::FormatValidator(QWidget * parent, Formats const & f)
	: QValidator(parent), formats_(f)
{
}


void FormatValidator::fixup(QString & input) const
{
	Formats::const_iterator cit = formats_.begin();
	Formats::const_iterator end = formats_.end();
	for (; cit != end; ++cit) {
		QString const name = toString(*cit);
		if (distance(formats_.begin(), cit) == nr()) {
			input = name;
			return;
		}
	}
}


QValidator::State FormatValidator::validate(QString & input, int & /*pos*/) const
{
	Formats::const_iterator cit = formats_.begin();
	Formats::const_iterator end = formats_.end();
	bool unknown = true;
	for (; unknown && cit != end; ++cit) {
		QString const name = toString(*cit);
		if (distance(formats_.begin(), cit) != nr())
			unknown = name != input;
	}

	if (unknown && !input.isEmpty())
		return QValidator::Acceptable;
	else
		return QValidator::Intermediate;
}


int FormatValidator::nr() const
{
	QComboBox * p = qobject_cast<QComboBox *>(parent());
	return p->itemData(p->currentIndex()).toInt();
}


/////////////////////////////////////////////////////////////////////
//
// FormatNameValidator
//
/////////////////////////////////////////////////////////////////////

class FormatNameValidator : public FormatValidator
{
public:
	FormatNameValidator(QWidget * parent, Formats const & f)
		: FormatValidator(parent, f)
	{}
private:
	QString toString(Format const & format) const override
	{
		return toqstr(format.name());
	}
};


/////////////////////////////////////////////////////////////////////
//
// FormatPrettynameValidator
//
/////////////////////////////////////////////////////////////////////

class FormatPrettynameValidator : public FormatValidator
{
public:
	FormatPrettynameValidator(QWidget * parent, Formats const & f)
		: FormatValidator(parent, f)
	{}
private:
	QString toString(Format const & format) const override
	{
		return toqstr(translateIfPossible(format.prettyname()));
	}
};


/////////////////////////////////////////////////////////////////////
//
// PrefFileformats
//
/////////////////////////////////////////////////////////////////////

PrefFileformats::PrefFileformats(GuiPreferences * form)
	: PrefModule(catFiles, N_("File Formats"), form)
{
	setupUi(this);

	formatED->setValidator(new FormatNameValidator(formatsCB, form_->formats()));
	formatsCB->setValidator(new FormatPrettynameValidator(formatsCB, form_->formats()));
	extensionsED->setValidator(new NoNewLineValidator(extensionsED));
	shortcutED->setValidator(new NoNewLineValidator(shortcutED));
	editorED->setValidator(new NoNewLineValidator(editorED));
	viewerED->setValidator(new NoNewLineValidator(viewerED));
	copierED->setValidator(new NoNewLineValidator(copierED));

	connect(documentCB, SIGNAL(clicked()),
		this, SLOT(setFlags()));
	connect(vectorCB, SIGNAL(clicked()),
		this, SLOT(setFlags()));
	connect(exportMenuCB, SIGNAL(clicked()),
		this, SLOT(setFlags()));
	connect(formatsCB->lineEdit(), SIGNAL(editingFinished()),
		this, SLOT(updatePrettyname()));
	connect(formatsCB->lineEdit(), SIGNAL(textEdited(QString)),
		this, SIGNAL(changed()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	connect(defaultFormatCB, SIGNAL(activated(QString)),
		this, SIGNAL(changed()));
	connect(defaultOTFFormatCB, SIGNAL(activated(QString)),
		this, SIGNAL(changed()));
	connect(defaultPlatexFormatCB, SIGNAL(activated(QString)),
		this, SIGNAL(changed()));
#else
	connect(defaultFormatCB, SIGNAL(textActivated(QString)),
		this, SIGNAL(changed()));
	connect(defaultOTFFormatCB, SIGNAL(textActivated(QString)),
		this, SIGNAL(changed()));
	connect(defaultPlatexFormatCB, SIGNAL(textActivated(QString)),
		this, SIGNAL(changed()));
#endif
	connect(viewerCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(editorCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
}


namespace {

string const l10n_shortcut(docstring const & prettyname, string const & shortcut)
{
	if (shortcut.empty())
		return string();

	string l10n_format =
		to_utf8(_(to_utf8(prettyname) + '|' + shortcut));
	return split(l10n_format, '|');
}

} // namespace


void PrefFileformats::applyRC(LyXRC & rc) const
{
	QString const default_format = defaultFormatCB->itemData(
		defaultFormatCB->currentIndex()).toString();
	rc.default_view_format = fromqstr(default_format);
	QString const default_otf_format = defaultOTFFormatCB->itemData(
		defaultOTFFormatCB->currentIndex()).toString();
	rc.default_otf_view_format = fromqstr(default_otf_format);
	QString const default_platex_format = defaultPlatexFormatCB->itemData(
		defaultPlatexFormatCB->currentIndex()).toString();
	rc.default_platex_view_format = fromqstr(default_platex_format);
}


void PrefFileformats::updateRC(LyXRC const & rc)
{
	viewer_alternatives = rc.viewer_alternatives;
	editor_alternatives = rc.editor_alternatives;
	bool const init = defaultFormatCB->currentText().isEmpty();
	updateView();
	if (init) {
		int pos =
			defaultFormatCB->findData(toqstr(rc.default_view_format));
		defaultFormatCB->setCurrentIndex(pos);
		pos = defaultOTFFormatCB->findData(toqstr(rc.default_otf_view_format));
				defaultOTFFormatCB->setCurrentIndex(pos);
		defaultOTFFormatCB->setCurrentIndex(pos);
		pos = defaultPlatexFormatCB->findData(toqstr(rc.default_platex_view_format));
				defaultPlatexFormatCB->setCurrentIndex(pos);
		defaultPlatexFormatCB->setCurrentIndex(pos);
	}
}


void PrefFileformats::updateView()
{
	QString const current = formatsCB->currentText();
	QString const current_def = defaultFormatCB->currentText();
	QString const current_def_otf = defaultOTFFormatCB->currentText();
	QString const current_def_platex = defaultPlatexFormatCB->currentText();

	// update comboboxes with formats
	formatsCB->blockSignals(true);
	defaultFormatCB->blockSignals(true);
	defaultOTFFormatCB->blockSignals(true);
	defaultPlatexFormatCB->blockSignals(true);
	formatsCB->clear();
	defaultFormatCB->clear();
	defaultOTFFormatCB->clear();
	defaultPlatexFormatCB->clear();
	form_->formats().sort();
	for (Format const & f : form_->formats()) {
		QString const prettyname = toqstr(translateIfPossible(f.prettyname()));
		formatsCB->addItem(prettyname,
		                   QVariant(form_->formats().getNumber(f.name())));
		if (f.viewer().empty())
			continue;
		if (form_->converters().isReachable("xhtml", f.name())
		    || form_->converters().isReachable("dviluatex", f.name())
		    || form_->converters().isReachable("luatex", f.name())
		    || form_->converters().isReachable("xetex", f.name())) {
			defaultFormatCB->addItem(prettyname,
					QVariant(toqstr(f.name())));
			defaultOTFFormatCB->addItem(prettyname,
					QVariant(toqstr(f.name())));
		} else {
			if (form_->converters().isReachable("latex", f.name())
			    || form_->converters().isReachable("pdflatex", f.name()))
				defaultFormatCB->addItem(prettyname,
					QVariant(toqstr(f.name())));
			if (form_->converters().isReachable("platex", f.name()))
					defaultPlatexFormatCB->addItem(prettyname,
						QVariant(toqstr(f.name())));
		}
	}

	// restore selections
	int item = formatsCB->findText(current, Qt::MatchExactly);
	formatsCB->setCurrentIndex(item < 0 ? 0 : item);
	on_formatsCB_currentIndexChanged(item < 0 ? 0 : item);
	item = defaultFormatCB->findText(current_def, Qt::MatchExactly);
	defaultFormatCB->setCurrentIndex(item < 0 ? 0 : item);
	item = defaultOTFFormatCB->findText(current_def_otf, Qt::MatchExactly);
	defaultOTFFormatCB->setCurrentIndex(item < 0 ? 0 : item);
	item = defaultPlatexFormatCB->findText(current_def_platex, Qt::MatchExactly);
	defaultPlatexFormatCB->setCurrentIndex(item < 0 ? 0 : item);
	formatsCB->blockSignals(false);
	defaultFormatCB->blockSignals(false);
	defaultOTFFormatCB->blockSignals(false);
	defaultPlatexFormatCB->blockSignals(false);
}


void PrefFileformats::on_formatsCB_currentIndexChanged(int i)
{
	if (form_->formats().empty())
		return;
	int const nr = formatsCB->itemData(i).toInt();
	Format const f = form_->formats().get(nr);

	formatED->setText(toqstr(f.name()));
	copierED->setText(toqstr(form_->movers().command(f.name())));
	extensionsED->setText(toqstr(f.extensions()));
	mimeED->setText(toqstr(f.mime()));
	shortcutED->setText(
		toqstr(l10n_shortcut(f.prettyname(), f.shortcut())));
	documentCB->setChecked((f.documentFormat()));
	vectorCB->setChecked((f.vectorFormat()));
	exportMenuCB->setChecked((f.inExportMenu()));
	exportMenuCB->setEnabled((f.documentFormat()));
	updateViewers();
	updateEditors();
}


void PrefFileformats::setFlags()
{
	int flags = Format::none;
	if (documentCB->isChecked())
		flags |= Format::document;
	if (vectorCB->isChecked())
		flags |= Format::vector;
	if (exportMenuCB->isChecked())
		flags |= Format::export_menu;
	currentFormat().setFlags(flags);
	exportMenuCB->setEnabled(documentCB->isChecked());
	changed();
}


void PrefFileformats::on_copierED_textEdited(const QString & s)
{
	string const fmt = fromqstr(formatED->text());
	form_->movers().set(fmt, fromqstr(s));
	changed();
}


void PrefFileformats::on_extensionsED_textEdited(const QString & s)
{
	currentFormat().setExtensions(fromqstr(s));
	changed();
}


void PrefFileformats::on_viewerED_textEdited(const QString & s)
{
	currentFormat().setViewer(fromqstr(s));
	changed();
}


void PrefFileformats::on_editorED_textEdited(const QString & s)
{
	currentFormat().setEditor(fromqstr(s));
	changed();
}


void PrefFileformats::on_mimeED_textEdited(const QString & s)
{
	currentFormat().setMime(fromqstr(s));
	changed();
}


void PrefFileformats::on_shortcutED_textEdited(const QString & s)
{
	string const new_shortcut = fromqstr(s);
	if (new_shortcut == l10n_shortcut(currentFormat().prettyname(),
					  currentFormat().shortcut()))
		return;
	currentFormat().setShortcut(new_shortcut);
	changed();
}


void PrefFileformats::on_formatED_editingFinished()
{
	string const newname = fromqstr(formatED->displayText());
	string const oldname = currentFormat().name();
	if (newname == oldname)
		return;
	if (form_->converters().formatIsUsed(oldname)) {
		Alert::error(_("Format in use"),
			     _("You cannot change a format's short name "
			       "if the format is used by a converter. "
			       "Please remove the converter first."));
		activatePrefsWindow(form_);
		updateView();
		return;
	}

	currentFormat().setName(newname);
	changed();
}


void PrefFileformats::on_formatED_textChanged(const QString &)
{
	QString t = formatED->text();
	int p = 0;
	bool valid = formatED->validator()->validate(t, p) == QValidator::Acceptable;
	setValid(formatLA, valid);
}


void PrefFileformats::on_formatsCB_editTextChanged(const QString &)
{
	QString t = formatsCB->currentText();
	int p = 0;
	bool valid = formatsCB->validator()->validate(t, p) == QValidator::Acceptable;
	setValid(formatsLA, valid);
}


void PrefFileformats::updatePrettyname()
{
	QString const newname = formatsCB->currentText();
	if (newname == toqstr(translateIfPossible(currentFormat().prettyname())))
		return;

	currentFormat().setPrettyname(qstring_to_ucs4(newname));
	formatsChanged();
	updateView();
	changed();
}


namespace {
	void updateComboBox(LyXRC::Alternatives const & alts,
	                    string const & fmt, QComboBox * combo)
	{
		LyXRC::Alternatives::const_iterator it =
				alts.find(fmt);
		if (it != alts.end()) {
			LyXRC::CommandSet const & cmds = it->second;
			LyXRC::CommandSet::const_iterator sit =
					cmds.begin();
			LyXRC::CommandSet::const_iterator const sen =
					cmds.end();
			for (; sit != sen; ++sit) {
				QString const qcmd = toqstr(*sit);
				combo->addItem(qcmd, qcmd);
			}
		}
	}
} // namespace


void PrefFileformats::updateViewers()
{
	Format const f = currentFormat();
	viewerCO->blockSignals(true);
	viewerCO->clear();
	viewerCO->addItem(qt_("None"), QString());
	if (os::canAutoOpenFile(f.extension(), os::VIEW))
		viewerCO->addItem(qt_("System Default"), QString("auto"));
	updateComboBox(viewer_alternatives, f.name(), viewerCO);
	viewerCO->addItem(qt_("Custom"), QString("custom viewer"));
	viewerCO->blockSignals(false);

	int pos = viewerCO->findData(toqstr(f.viewer()));
	if (pos != -1) {
		viewerED->clear();
		viewerED->setEnabled(false);
		viewerCO->setCurrentIndex(pos);
	} else {
		viewerED->setEnabled(true);
		viewerED->setText(toqstr(f.viewer()));
		viewerCO->setCurrentIndex(viewerCO->findData(toqstr("custom viewer")));
	}
}


void PrefFileformats::updateEditors()
{
	Format const f = currentFormat();
	editorCO->blockSignals(true);
	editorCO->clear();
	editorCO->addItem(qt_("None"), QString());
	if (os::canAutoOpenFile(f.extension(), os::EDIT))
		editorCO->addItem(qt_("System Default"), QString("auto"));
	updateComboBox(editor_alternatives, f.name(), editorCO);
	editorCO->addItem(qt_("Custom"), QString("custom editor"));
	editorCO->blockSignals(false);

	int pos = editorCO->findData(toqstr(f.editor()));
	if (pos != -1) {
		editorED->clear();
		editorED->setEnabled(false);
		editorCO->setCurrentIndex(pos);
	} else {
		editorED->setEnabled(true);
		editorED->setText(toqstr(f.editor()));
		editorCO->setCurrentIndex(editorCO->findData(toqstr("custom editor")));
	}
}


void PrefFileformats::on_viewerCO_currentIndexChanged(int i)
{
	bool const custom = viewerCO->itemData(i).toString() == "custom viewer";
	viewerED->setEnabled(custom);
	if (!custom)
		currentFormat().setViewer(fromqstr(viewerCO->itemData(i).toString()));
}


void PrefFileformats::on_editorCO_currentIndexChanged(int i)
{
	bool const custom = editorCO->itemData(i).toString() == "custom editor";
	editorED->setEnabled(custom);
	if (!custom)
		currentFormat().setEditor(fromqstr(editorCO->itemData(i).toString()));
}


Format & PrefFileformats::currentFormat()
{
	int const i = formatsCB->currentIndex();
	int const nr = formatsCB->itemData(i).toInt();
	return form_->formats().get(nr);
}


void PrefFileformats::on_formatNewPB_clicked()
{
	form_->formats().add("", "", docstring(), "", "", "", "", Format::none);
	updateView();
	formatsCB->setCurrentIndex(0);
	formatsCB->setFocus(Qt::OtherFocusReason);
}


void PrefFileformats::on_formatRemovePB_clicked()
{
	int const i = formatsCB->currentIndex();
	int const nr = formatsCB->itemData(i).toInt();
	string const current_text = form_->formats().get(nr).name();
	if (form_->converters().formatIsUsed(current_text)) {
		Alert::error(_("Format in use"),
			     _("Cannot remove a Format used by a Converter. "
					    "Remove the converter first."));
		activatePrefsWindow(form_);
		return;
	}

	form_->formats().erase(current_text);
	formatsChanged();
	updateView();
	on_formatsCB_editTextChanged(formatsCB->currentText());
	changed();
}


/////////////////////////////////////////////////////////////////////
//
// PrefLanguage
//
/////////////////////////////////////////////////////////////////////

PrefLanguage::PrefLanguage(GuiPreferences * form)
	: PrefModule(catLanguage, N_("Language"), form)
{
	setupUi(this);

	connect(visualCursorRB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(logicalCursorRB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(markForeignCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(respectOSkbdCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(explicitDocLangBeginCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(explicitDocLangEndCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(languagePackageCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(languagePackageED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(globalCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(startCommandED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(endCommandED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(uiLanguageCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(defaultDecimalSepED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(defaultDecimalSepCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(defaultLengthUnitCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));

	languagePackageED->setValidator(new NoNewLineValidator(languagePackageED));
	startCommandED->setValidator(new NoNewLineValidator(startCommandED));
	endCommandED->setValidator(new NoNewLineValidator(endCommandED));

	defaultDecimalSepED->setValidator(new QRegularExpressionValidator(QRegularExpression("\\S"), this));
	defaultDecimalSepED->setMaxLength(1);

	defaultLengthUnitCO->addItem(lyx::qt_(unit_name_gui[Length::CM]), Length::CM);
	defaultLengthUnitCO->addItem(lyx::qt_(unit_name_gui[Length::IN]), Length::IN);

	QAbstractItemModel * language_model = guiApp->languageModel();
	language_model->sort(0);
	uiLanguageCO->blockSignals(true);
	uiLanguageCO->clear();
	uiLanguageCO->addItem(qt_("Default"), toqstr("auto"));
	for (int i = 0; i != language_model->rowCount(); ++i) {
		QModelIndex index = language_model->index(i, 0);
		// Filter the list based on the available translation and add
		// each language code only once
		string const name = fromqstr(index.data(Qt::UserRole).toString());
		Language const * lang = languages.getLanguage(name);
		if (!lang)
			continue;
		// never remove the currently selected language
		if (name != form->rc().gui_language
		    && name != lyxrc.gui_language
		    && (!Messages::available(lang->code())
		        || !lang->hasGuiSupport()))
			continue;
		uiLanguageCO->addItem(index.data(Qt::DisplayRole).toString(),
		                      index.data(Qt::UserRole).toString());
	}
	uiLanguageCO->blockSignals(false);

	// FIXME: restore this when it works (see discussion in #6450).
	respectOSkbdCB->hide();
}


void PrefLanguage::on_uiLanguageCO_currentIndexChanged(int)
{
	 QMessageBox::information(this, qt_("LyX needs to be restarted!"),
		 qt_("The change of user interface language will be fully "
		 "effective only after a restart."));
}


void PrefLanguage::on_languagePackageCO_currentIndexChanged(int i)
{
	if (i == 2)
		languagePackageED->setText(save_langpack_);
	else if (!languagePackageED->text().isEmpty()) {
		save_langpack_ = languagePackageED->text();
		languagePackageED->clear();
	}
	languagePackageED->setEnabled(i == 2);
}


void PrefLanguage::on_defaultDecimalSepCO_currentIndexChanged(int i)
{
	defaultDecimalSepED->setEnabled(i == 1);
}


void PrefLanguage::applyRC(LyXRC & rc) const
{
	rc.visual_cursor = visualCursorRB->isChecked();
	rc.mark_foreign_language = markForeignCB->isChecked();
	rc.respect_os_kbd_language = respectOSkbdCB->isChecked();
	rc.language_auto_begin = !explicitDocLangBeginCB->isChecked();
	rc.language_auto_end = !explicitDocLangEndCB->isChecked();
	int const p = languagePackageCO->currentIndex();
	if (p == 0)
		rc.language_package_selection = LyXRC::LP_AUTO;
	else if (p == 1)
		rc.language_package_selection = LyXRC::LP_BABEL;
	else if (p == 2)
		rc.language_package_selection = LyXRC::LP_CUSTOM;
	else if (p == 3)
		rc.language_package_selection = LyXRC::LP_NONE;
	rc.language_custom_package = fromqstr(languagePackageED->text());
	rc.language_global_options = globalCB->isChecked();
	rc.language_command_begin = fromqstr(startCommandED->text());
	rc.language_command_end = fromqstr(endCommandED->text());
	rc.gui_language = fromqstr(
		uiLanguageCO->itemData(uiLanguageCO->currentIndex()).toString());
	if (defaultDecimalSepCO->currentIndex() == 0)
		rc.default_decimal_sep = "locale";
	else
		rc.default_decimal_sep = fromqstr(defaultDecimalSepED->text());
	rc.default_length_unit = (Length::UNIT) defaultLengthUnitCO->itemData(defaultLengthUnitCO->currentIndex()).toInt();
}


void PrefLanguage::updateRC(LyXRC const & rc)
{
	if (rc.visual_cursor)
		visualCursorRB->setChecked(true);
	else
		logicalCursorRB->setChecked(true);
	markForeignCB->setChecked(rc.mark_foreign_language);
	respectOSkbdCB->setChecked(rc.respect_os_kbd_language);
	explicitDocLangBeginCB->setChecked(!rc.language_auto_begin);
	explicitDocLangEndCB->setChecked(!rc.language_auto_end);
	languagePackageCO->setCurrentIndex(rc.language_package_selection);
	if (languagePackageCO->currentIndex() == 2) {
		languagePackageED->setText(toqstr(rc.language_custom_package));
		languagePackageED->setEnabled(true);
	} else {
		languagePackageED->clear();
		save_langpack_ = toqstr(rc.language_custom_package);
		languagePackageED->setEnabled(false);
	}
	defaultDecimalSepED->setEnabled(defaultDecimalSepCO->currentIndex() == 1);
	globalCB->setChecked(rc.language_global_options);
	startCommandED->setText(toqstr(rc.language_command_begin));
	endCommandED->setText(toqstr(rc.language_command_end));
	if (rc.default_decimal_sep == "locale") {
		defaultDecimalSepCO->setCurrentIndex(0);
		defaultDecimalSepED->clear();
	} else {
		defaultDecimalSepCO->setCurrentIndex(1);
		defaultDecimalSepED->setText(toqstr(rc.default_decimal_sep));
	}
	int pos = defaultLengthUnitCO->findData(int(rc.default_length_unit));
	defaultLengthUnitCO->setCurrentIndex(pos);

	pos = uiLanguageCO->findData(toqstr(rc.gui_language));
	uiLanguageCO->blockSignals(true);
	uiLanguageCO->setCurrentIndex(pos);
	uiLanguageCO->blockSignals(false);
}


/////////////////////////////////////////////////////////////////////
//
// PrefUserInterface
//
/////////////////////////////////////////////////////////////////////

PrefUserInterface::PrefUserInterface(GuiPreferences * form)
	: PrefModule(catLookAndFeel, N_("User Interface"), form)
{
	setupUi(this);

	connect(uiFilePB, SIGNAL(clicked()),
		this, SLOT(selectUi()));
	connect(uiFileED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(iconSetCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(uiStyleCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
#if (defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN) || defined(Q_OS_MAC)) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	connect(colorSchemeCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
#endif
	connect(useSystemThemeIconsCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(lastfilesSB, SIGNAL(valueChanged(int)),
		this, SIGNAL(changed()));
	connect(tooltipCB, SIGNAL(toggled(bool)),
		this, SIGNAL(changed()));
	connect(toggleTabbarCB, SIGNAL(toggled(bool)),
		this, SIGNAL(changed()));
	connect(toggleMenubarCB, SIGNAL(toggled(bool)),
		this, SIGNAL(changed()));
	connect(toggleScrollbarCB, SIGNAL(toggled(bool)),
		this, SIGNAL(changed()));
	connect(toggleStatusbarCB, SIGNAL(toggled(bool)),
		this, SIGNAL(changed()));
	connect(toggleToolbarsCB, SIGNAL(toggled(bool)),
		this, SIGNAL(changed()));
	lastfilesSB->setMaximum(maxlastfiles);

	iconSetCO->addItem(qt_("Default"), QString());
	iconSetCO->addItem(qt_("Adwaita"), "adwaita");
	iconSetCO->addItem(qt_("Classic"), "classic");
	iconSetCO->addItem(qt_("Oxygen"), "oxygen");

#if (defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN) || defined(Q_OS_MAC)) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	colorSchemeCO->addItem(qt_("System Default"), "system");
	colorSchemeCO->addItem(qt_("Light Mode"), "light");
	colorSchemeCO->addItem(qt_("Dark Mode"), "dark");
#else
	colorSchemeCO->setVisible(false);
	colorSchemeLA->setVisible(false);
#endif

	uiStyleCO->addItem(qt_("Default"), toqstr("default"));
	for (auto const & style : QStyleFactory::keys())
		uiStyleCO->addItem(style, style.toLower());

	if (guiApp->platformName() != "xcb"
	    && !guiApp->platformName().contains("wayland"))
		useSystemThemeIconsCB->hide();
}


void PrefUserInterface::applyRC(LyXRC & rc) const
{
	rc.icon_set = fromqstr(iconSetCO->itemData(
		iconSetCO->currentIndex()).toString());

	QString const uistyle = uiStyleCO->itemData(
		uiStyleCO->currentIndex()).toString();
	if (rc.ui_style != fromqstr(uistyle)) {
		rc.ui_style = fromqstr(uistyle);
		if (rc.ui_style == "default") {
			// FIXME: This should work with frontend::GuiApplication::setStyle(QString())
			//        Qt bug https://bugreports.qt.io/browse/QTBUG-58268
			frontend::Alert::warning(_("Restart needed"),
						 _("Resetting the user interface style to 'Default'"
						   " requires a restart of LyX."));
			activatePrefsWindow(form_);
		}
		else
			frontend::GuiApplication::setStyle(uistyle);
	}
#if (defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN) || defined(Q_OS_MAC)) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	QString const color_scheme = colorSchemeCO->itemData(
		colorSchemeCO->currentIndex()).toString();
	if (rc.color_scheme != fromqstr(color_scheme)) {
		if (lyxrc.color_scheme == "dark")
			guiApp->styleHints()->setColorScheme(Qt::ColorScheme::Dark);
		else if (lyxrc.color_scheme == "light")
			guiApp->styleHints()->setColorScheme(Qt::ColorScheme::Light);
		else
			guiApp->styleHints()->unsetColorScheme();
	}
	rc.color_scheme = fromqstr(color_scheme);
#endif

	rc.ui_file = internal_path(fromqstr(uiFileED->text()));
	rc.use_system_theme_icons = useSystemThemeIconsCB->isChecked();
	rc.num_lastfiles = lastfilesSB->value();
	rc.use_tooltip = tooltipCB->isChecked();
	rc.full_screen_toolbars = toggleToolbarsCB->isChecked();
	rc.full_screen_scrollbar = toggleScrollbarCB->isChecked();
	rc.full_screen_statusbar = toggleStatusbarCB->isChecked();
	rc.full_screen_tabbar = toggleTabbarCB->isChecked();
	rc.full_screen_menubar = toggleMenubarCB->isChecked();
}


void PrefUserInterface::updateRC(LyXRC const & rc)
{
	int iconset = iconSetCO->findData(toqstr(rc.icon_set));
	if (iconset < 0)
		iconset = 0;
	iconSetCO->setCurrentIndex(iconset);
	int uistyle = uiStyleCO->findData(toqstr(rc.ui_style));
	if (uistyle < 0)
		uistyle = 0;
	uiStyleCO->setCurrentIndex(uistyle);
	useSystemThemeIconsCB->setChecked(rc.use_system_theme_icons);
	uiFileED->setText(toqstr(external_path(rc.ui_file)));
	lastfilesSB->setValue(rc.num_lastfiles);
	tooltipCB->setChecked(rc.use_tooltip);
	toggleScrollbarCB->setChecked(rc.full_screen_scrollbar);
	toggleStatusbarCB->setChecked(rc.full_screen_statusbar);
	toggleToolbarsCB->setChecked(rc.full_screen_toolbars);
	toggleTabbarCB->setChecked(rc.full_screen_tabbar);
	toggleMenubarCB->setChecked(rc.full_screen_menubar);
#if (defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN) || defined(Q_OS_MAC)) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	int colorscheme = colorSchemeCO->findData(toqstr(rc.color_scheme));
	if (colorscheme < 0)
		colorscheme = 0;
	colorSchemeCO->setCurrentIndex(colorscheme);
#endif
}


void PrefUserInterface::selectUi()
{
	QString file = form_->browseUI(internalPath(uiFileED->text()));
	if (!file.isEmpty())
		uiFileED->setText(file);
}


/////////////////////////////////////////////////////////////////////
//
// PrefDocumentHandling
//
/////////////////////////////////////////////////////////////////////

PrefDocHandling::PrefDocHandling(GuiPreferences * form)
	: PrefModule(catLookAndFeel, N_("Document Handling"), form)
{
	setupUi(this);

	connect(autoSaveCB, SIGNAL(toggled(bool)),
		autoSaveSB, SLOT(setEnabled(bool)));
	connect(autoSaveCB, SIGNAL(toggled(bool)),
		TextLabel1, SLOT(setEnabled(bool)));
	connect(openDocumentsInTabsCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(singleInstanceCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(singleCloseTabButtonCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(closeLastViewCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(restoreCursorCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(loadSessionCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(allowGeometrySessionCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(autoSaveSB, SIGNAL(valueChanged(int)),
		this, SIGNAL(changed()));
	connect(autoSaveCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(backupCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(saveCompressedCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(saveOriginCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
}


void PrefDocHandling::applyRC(LyXRC & rc) const
{
	rc.use_lastfilepos = restoreCursorCB->isChecked();
	rc.load_session = loadSessionCB->isChecked();
	rc.allow_geometry_session = allowGeometrySessionCB->isChecked();
	rc.autosave = autoSaveCB->isChecked() ?  autoSaveSB->value() * 60 : 0;
	rc.make_backup = backupCB->isChecked();
	rc.save_compressed = saveCompressedCB->isChecked();
	rc.save_origin = saveOriginCB->isChecked();
	rc.open_buffers_in_tabs = openDocumentsInTabsCB->isChecked();
	rc.single_instance = singleInstanceCB->isChecked();
	rc.single_close_tab_button = singleCloseTabButtonCB->isChecked();

	switch (closeLastViewCO->currentIndex()) {
	case 0:
		rc.close_buffer_with_last_view = "yes";
		break;
	case 1:
		rc.close_buffer_with_last_view = "no";
		break;
	case 2:
		rc.close_buffer_with_last_view = "ask";
		break;
	default:
		;
	}
}


void PrefDocHandling::updateRC(LyXRC const & rc)
{
	restoreCursorCB->setChecked(rc.use_lastfilepos);
	loadSessionCB->setChecked(rc.load_session);
	allowGeometrySessionCB->setChecked(rc.allow_geometry_session);
	// convert to minutes
	bool autosave = rc.autosave > 0;
	int mins = rc.autosave / 60;
	if (!mins)
		mins = 5;
	autoSaveSB->setValue(mins);
	autoSaveCB->setChecked(autosave);
	autoSaveSB->setEnabled(autosave);
	backupCB->setChecked(rc.make_backup);
	saveCompressedCB->setChecked(rc.save_compressed);
	saveOriginCB->setChecked(rc.save_origin);
	openDocumentsInTabsCB->setChecked(rc.open_buffers_in_tabs);
	singleInstanceCB->setChecked(rc.single_instance && !rc.lyxpipes.empty());
	singleInstanceCB->setEnabled(!rc.lyxpipes.empty());
	singleCloseTabButtonCB->setChecked(rc.single_close_tab_button);
	if (rc.close_buffer_with_last_view == "yes")
		closeLastViewCO->setCurrentIndex(0);
	else if (rc.close_buffer_with_last_view == "no")
		closeLastViewCO->setCurrentIndex(1);
	else if (rc.close_buffer_with_last_view == "ask")
		closeLastViewCO->setCurrentIndex(2);
	if (rc.backupdir_path.empty())
		backupCB->setToolTip(qt_("If this is checked, a backup of the document is created "
					 "in the current working directory. "
					 "The backup file has the same name but the suffix '.lyx~'. "
					 "Note that these files are hidden by default by some file managers. "
					 "A dedicated backup directory can be set in the 'Paths' section."));
	else {
		docstring const tip = bformat(_("If this is checked, a backup of the document is created "
						"in the backup directory (%1$s). "
						"The backup file has the full original path and name as file name "
						"and the suffix \'.lyx~\' (e.g., !mydir!filename.lyx~). "
						"Note that these files are hidden by default by some file managers."),
					      FileName(rc.backupdir_path).absoluteFilePath());
		backupCB->setToolTip(toqstr(tip));
	}
}


void PrefDocHandling::on_clearSessionPB_clicked()
{
	guiApp->clearSession();
}



/////////////////////////////////////////////////////////////////////
//
// PrefEdit
//
/////////////////////////////////////////////////////////////////////

PrefEdit::PrefEdit(GuiPreferences * form)
	: PrefModule(catEditing, N_("Control"), form)
{
	setupUi(this);

	connect(cursorFollowsCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(scrollBelowCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(macLikeCursorMovementCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(copyCTMarkupCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(sortEnvironmentsCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(groupEnvironmentsCB, SIGNAL(clicked()),
		this, SIGNAL(changed()));
	connect(macroEditStyleCO, SIGNAL(activated(int)),
		this, SIGNAL(changed()));
	connect(cursorWidthSB, SIGNAL(valueChanged(int)),
		this, SIGNAL(changed()));
	connect(citationSearchLE, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenWidthLE, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(screenWidthUnitCO, SIGNAL(selectionChanged(lyx::Length::UNIT)),
		this, SIGNAL(changed()));
}


void PrefEdit::on_screenLimitCB_toggled(bool const state)
{
	screenWidthLE->setEnabled(state);
	screenWidthUnitCO->setEnabled(state);
	changed();
}


void PrefEdit::on_citationSearchCB_toggled(bool const state)
{
	citationSearchLE->setEnabled(state);
	citationSearchLA->setEnabled(state);
	changed();
}


void PrefEdit::applyRC(LyXRC & rc) const
{
	rc.cursor_follows_scrollbar = cursorFollowsCB->isChecked();
	rc.scroll_below_document = scrollBelowCB->isChecked();
	rc.mac_like_cursor_movement = macLikeCursorMovementCB->isChecked();
	rc.ct_markup_copied = copyCTMarkupCB->isChecked();
	rc.sort_layouts = sortEnvironmentsCB->isChecked();
	rc.group_layouts = groupEnvironmentsCB->isChecked();
	switch (macroEditStyleCO->currentIndex()) {
		case 0: rc.macro_edit_style = LyXRC::MACRO_EDIT_INLINE_BOX; break;
		case 1:	rc.macro_edit_style = LyXRC::MACRO_EDIT_INLINE; break;
		case 2:	rc.macro_edit_style = LyXRC::MACRO_EDIT_LIST;	break;
	}
	rc.cursor_width = cursorWidthSB->value();
	rc.citation_search = citationSearchCB->isChecked();
	rc.citation_search_pattern = fromqstr(citationSearchLE->text());
	rc.screen_width = Length(widgetsToLength(screenWidthLE, screenWidthUnitCO));
	rc.screen_limit = screenLimitCB->isChecked();
}


void PrefEdit::updateRC(LyXRC const & rc)
{
	cursorFollowsCB->setChecked(rc.cursor_follows_scrollbar);
	scrollBelowCB->setChecked(rc.scroll_below_document);
	macLikeCursorMovementCB->setChecked(rc.mac_like_cursor_movement);
	copyCTMarkupCB->setChecked(rc.ct_markup_copied);
	sortEnvironmentsCB->setChecked(rc.sort_layouts);
	groupEnvironmentsCB->setChecked(rc.group_layouts);
	macroEditStyleCO->setCurrentIndex(rc.macro_edit_style);
	cursorWidthSB->setValue(rc.cursor_width);
	citationSearchCB->setChecked(rc.citation_search);
	citationSearchLE->setText(toqstr(rc.citation_search_pattern));
	citationSearchLE->setEnabled(rc.citation_search);
	citationSearchLA->setEnabled(rc.citation_search);
	lengthToWidgets(screenWidthLE, screenWidthUnitCO, rc.screen_width, Length::defaultUnit());
	screenWidthUnitCO->setEnabled(rc.screen_limit);
	screenLimitCB->setChecked(rc.screen_limit);
	screenWidthLE->setEnabled(rc.screen_limit);
}


/////////////////////////////////////////////////////////////////////
//
// PrefShortcuts
//
/////////////////////////////////////////////////////////////////////


GuiShortcutDialog::GuiShortcutDialog(QWidget * parent) : QDialog(parent)
{
	Ui::shortcutUi::setupUi(this);
	QDialog::setModal(true);
	lfunLE->setValidator(new NoNewLineValidator(lfunLE));
	on_lfunLE_textChanged();
}


void GuiShortcutDialog::on_lfunLE_textChanged()
{
	QPushButton * ok = buttonBox->button(QDialogButtonBox::Ok);
	ok->setEnabled(!lfunLE->text().isEmpty());
}


PrefShortcuts::PrefShortcuts(GuiPreferences * form)
	: PrefModule(catEditing, N_("Shortcuts"), form),
	  editItem_(nullptr), mathItem_(nullptr), bufferItem_(nullptr), layoutItem_(nullptr),
	  systemItem_(nullptr)
{
	setupUi(this);

	shortcutsTW->setColumnCount(2);
	shortcutsTW->headerItem()->setText(0, qt_("Function"));
	shortcutsTW->headerItem()->setText(1, qt_("Shortcut"));
	shortcutsTW->setSortingEnabled(true);
	// Multi-selection can be annoying.
	// shortcutsTW->setSelectionMode(QAbstractItemView::MultiSelection);

	connect(bindFilePB, SIGNAL(clicked()),
		this, SLOT(selectBind()));
	connect(bindFileED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	shortcut_ = new GuiShortcutDialog(this);
	shortcut_bc_.setPolicy(ButtonPolicy::OkCancelPolicy);
	shortcut_bc_.setOK(shortcut_->buttonBox->button(QDialogButtonBox::Ok));
	shortcut_bc_.setCancel(shortcut_->buttonBox->button(QDialogButtonBox::Cancel));

	connect(shortcut_->buttonBox, SIGNAL(accepted()),
		this, SIGNAL(changed()));
	connect(shortcut_->buttonBox, SIGNAL(rejected()),
		shortcut_, SLOT(reject()));
	connect(shortcut_->clearPB, SIGNAL(clicked()),
		this, SLOT(shortcutClearPressed()));
	connect(shortcut_->removePB, SIGNAL(clicked()),
		this, SLOT(shortcutRemovePressed()));
	connect(shortcut_->buttonBox, SIGNAL(accepted()),
		this, SLOT(shortcutOkPressed()));
	connect(shortcut_->buttonBox, SIGNAL(rejected()),
		this, SLOT(shortcutCancelPressed()));
}


void PrefShortcuts::applyRC(LyXRC & rc) const
{
	rc.bind_file = internal_path(fromqstr(bindFileED->text()));
	// write user_bind and user_unbind to .lyx/bind/user.bind
	FileName bind_dir(addPath(package().user_support().absFileName(), "bind"));
	if (!bind_dir.exists() && !bind_dir.createDirectory(0777)) {
		lyxerr << "LyX could not create the user bind directory '"
		       << bind_dir << "'. All user-defined key bindings will be lost." << endl;
		return;
	}
	if (!bind_dir.isDirWritable()) {
		lyxerr << "LyX could not write to the user bind directory '"
		       << bind_dir << "'. All user-defined key bindings will be lost." << endl;
		return;
	}
	FileName user_bind_file(bind_dir.absFileName() + "/user.bind");
	user_unbind_.write(user_bind_file.toFilesystemEncoding(), false, true);
	user_bind_.write(user_bind_file.toFilesystemEncoding(), true, false);
	// immediately apply the keybindings. Why this is not done before?
	// The good thing is that the menus are updated automatically.
	theTopLevelKeymap().clear();
	theTopLevelKeymap().read("site");
	theTopLevelKeymap().read(rc.bind_file, nullptr, KeyMap::Fallback);
	theTopLevelKeymap().read("user", nullptr, KeyMap::MissingOK);
}


void PrefShortcuts::updateRC(LyXRC const & rc)
{
	bindFileED->setText(toqstr(external_path(rc.bind_file)));
	//
	system_bind_.clear();
	user_bind_.clear();
	user_unbind_.clear();
	system_bind_.read("site");
	system_bind_.read(rc.bind_file);
	// \unbind in user.bind is added to user_unbind_
	user_bind_.read("user", &user_unbind_, KeyMap::MissingOK);
	updateShortcutsTW();
}


void PrefShortcuts::updateShortcutsTW()
{
	shortcutsTW->clear();

	editItem_ = new QTreeWidgetItem(shortcutsTW);
	editItem_->setText(0, qt_("Cursor, Mouse and Editing Functions"));
	editItem_->setFlags(editItem_->flags() & ~Qt::ItemIsSelectable);

	mathItem_ = new QTreeWidgetItem(shortcutsTW);
	mathItem_->setText(0, qt_("Mathematical Symbols"));
	mathItem_->setFlags(mathItem_->flags() & ~Qt::ItemIsSelectable);

	bufferItem_ = new QTreeWidgetItem(shortcutsTW);
	bufferItem_->setText(0, qt_("Document and Window"));
	bufferItem_->setFlags(bufferItem_->flags() & ~Qt::ItemIsSelectable);

	layoutItem_ = new QTreeWidgetItem(shortcutsTW);
	layoutItem_->setText(0, qt_("Font, Layouts and Textclasses"));
	layoutItem_->setFlags(layoutItem_->flags() & ~Qt::ItemIsSelectable);

	systemItem_ = new QTreeWidgetItem(shortcutsTW);
	systemItem_->setText(0, qt_("System and Miscellaneous"));
	systemItem_->setFlags(systemItem_->flags() & ~Qt::ItemIsSelectable);

	// listBindings(unbound=true) lists all bound and unbound lfuns
	// Items in this list are tagged by their source.
	KeyMap::BindingList bindinglist = system_bind_.listBindings(true,
		KeyMap::System);
	KeyMap::BindingList user_bindinglist = user_bind_.listBindings(false,
		KeyMap::UserBind);
	KeyMap::BindingList user_unbindinglist = user_unbind_.listBindings(false,
		KeyMap::UserUnbind);
	bindinglist.insert(bindinglist.end(), user_bindinglist.begin(),
			user_bindinglist.end());
	bindinglist.insert(bindinglist.end(), user_unbindinglist.begin(),
			user_unbindinglist.end());

	KeyMap::BindingList::const_iterator it = bindinglist.begin();
	KeyMap::BindingList::const_iterator it_end = bindinglist.end();
	for (; it != it_end; ++it)
		insertShortcutItem(it->request, it->sequence, it->tag);

	shortcutsTW->sortItems(0, Qt::AscendingOrder);
	on_shortcutsTW_itemSelectionChanged();
	on_searchLE_textEdited();
	shortcutsTW->resizeColumnToContents(0);
}


//static
KeyMap::ItemType PrefShortcuts::itemType(QTreeWidgetItem & item)
{
	return static_cast<KeyMap::ItemType>(item.data(0, Qt::UserRole).toInt());
}


//static
bool PrefShortcuts::isAlwaysHidden(QTreeWidgetItem & item)
{
	// Hide rebound system settings that are empty
	return itemType(item) == KeyMap::UserUnbind && item.text(1).isEmpty();
}


void PrefShortcuts::setItemType(QTreeWidgetItem * item, KeyMap::ItemType tag)
{
	item->setData(0, Qt::UserRole, QVariant(tag));
	QFont font;

	switch (tag) {
	case KeyMap::System:
		break;
	case KeyMap::UserBind:
		font.setBold(true);
		break;
	case KeyMap::UserUnbind:
		font.setStrikeOut(true);
		break;
	// this item is not displayed now.
	case KeyMap::UserExtraUnbind:
		font.setStrikeOut(true);
		break;
	}
	item->setHidden(isAlwaysHidden(*item));
	item->setFont(1, font);
}


QTreeWidgetItem * PrefShortcuts::insertShortcutItem(FuncRequest const & lfun,
		KeySequence const & seq, KeyMap::ItemType tag)
{
	FuncCode const action = lfun.action();
	string const action_name = lyxaction.getActionName(action);
	QString const lfun_name = toqstr(from_utf8(action_name)
			+ ' ' + lfun.argument());
	QString const shortcut = toqstr(seq.print(KeySequence::ForGui));

	QTreeWidgetItem * newItem = nullptr;
	// for unbind items, try to find an existing item in the system bind list
	if (tag == KeyMap::UserUnbind) {
		QList<QTreeWidgetItem*> const items = shortcutsTW->findItems(shortcut,
			Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive), 1);
		for (auto const & item : items) {
			if (item->text(0) == lfun_name || lfun == FuncRequest::unknown) {
				newItem = item;
				break;
			}
		}
		// if not found, this unbind item is KeyMap::UserExtraUnbind
		// Such an item is not displayed to avoid confusion (what is
		// unmatched removed?).
		if (!newItem) {
			return nullptr;
		}
	}
	if (!newItem) {
		switch(lyxaction.getActionType(action)) {
		case LyXAction::Hidden:
			return nullptr;
		case LyXAction::Edit:
			newItem = new QTreeWidgetItem(editItem_);
			break;
		case LyXAction::Math:
			newItem = new QTreeWidgetItem(mathItem_);
			break;
		case LyXAction::Buffer:
			newItem = new QTreeWidgetItem(bufferItem_);
			break;
		case LyXAction::Layout:
			newItem = new QTreeWidgetItem(layoutItem_);
			break;
		case LyXAction::System:
			newItem = new QTreeWidgetItem(systemItem_);
			break;
		default:
			// this should not happen
			newItem = new QTreeWidgetItem(shortcutsTW);
		}
		newItem->setText(0, lfun_name);
		newItem->setText(1, shortcut);
	}

	// record BindFile representation to recover KeySequence when needed.
	newItem->setData(1, Qt::UserRole, toqstr(seq.print(KeySequence::BindFile)));
	setItemType(newItem, tag);
	return newItem;
}


void PrefShortcuts::on_shortcutsTW_itemSelectionChanged()
{
	QList<QTreeWidgetItem*> items = shortcutsTW->selectedItems();
	removePB->setEnabled(!items.isEmpty() && !items[0]->text(1).isEmpty());
	modifyPB->setEnabled(!items.isEmpty());
	if (items.isEmpty())
		return;

	if (itemType(*items[0]) == KeyMap::UserUnbind)
		removePB->setText(qt_("Res&tore"));
	else
		removePB->setText(qt_("Remo&ve"));
}


void PrefShortcuts::on_shortcutsTW_itemDoubleClicked()
{
	modifyShortcut();
}


void PrefShortcuts::modifyShortcut()
{
	QTreeWidgetItem * item = shortcutsTW->currentItem();
	if (item->flags() & Qt::ItemIsSelectable) {
		shortcut_->lfunLE->setText(item->text(0));
		save_lfun_ = item->text(0).trimmed();
		shortcut_->shortcutWG->setText(item->text(1));
		KeySequence seq;
		seq.parse(fromqstr(item->data(1, Qt::UserRole).toString()));
		shortcut_->shortcutWG->setKeySequence(seq);
		shortcut_->shortcutWG->setFocus();
		shortcut_->exec();
	}
}


void PrefShortcuts::unhideEmpty(QString const & lfun, bool select)
{
	// list of items that match lfun
	QList<QTreeWidgetItem*> items = shortcutsTW->findItems(lfun,
	     Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive), 0);
	for (auto const & item : items) {
		if (isAlwaysHidden(*item)) {
			setItemType(item, KeyMap::System);
			if (select)
				shortcutsTW->setCurrentItem(item);
			return;
		}
	}
}


void PrefShortcuts::removeShortcut()
{
	// it seems that only one item can be selected, but I am
	// removing all selected items anyway.
	QList<QTreeWidgetItem*> items = shortcutsTW->selectedItems();
	for (auto & item : items) {
		string shortcut = fromqstr(item->data(1, Qt::UserRole).toString());
		string lfun = fromqstr(item->text(0));
		FuncRequest const func = lyxaction.lookupFunc(lfun);

		switch (itemType(*item)) {
		case KeyMap::System: {
			// for system bind, we do not touch the item
			// but add an user unbind item
			user_unbind_.bind(shortcut, func);
			setItemType(item, KeyMap::UserUnbind);
			removePB->setText(qt_("Res&tore"));
			break;
		}
		case KeyMap::UserBind: {
			// for user_bind, we remove this bind
			QTreeWidgetItem * parent = item->parent();
			int itemIdx = parent->indexOfChild(item);
			parent->takeChild(itemIdx);
			if (itemIdx > 0)
				shortcutsTW->scrollToItem(parent->child(itemIdx - 1));
			else
				shortcutsTW->scrollToItem(parent);
			user_bind_.unbind(shortcut, func);
			// If this user binding hid an empty system binding, unhide the
			// latter and select it.
			unhideEmpty(item->text(0), true);
			break;
		}
		case KeyMap::UserUnbind: {
			// for user_unbind, we remove the unbind, and the item
			// become KeyMap::System again.
			KeySequence seq;
			seq.parse(shortcut);
			// Ask the user to replace current binding
			if (!validateNewShortcut(func, seq, QString()))
				break;
			user_unbind_.unbind(shortcut, func);
			setItemType(item, KeyMap::System);
			removePB->setText(qt_("Remo&ve"));
			break;
		}
		case KeyMap::UserExtraUnbind: {
			// for user unbind that is not in system bind file,
			// remove this unbind file
			QTreeWidgetItem * parent = item->parent();
			parent->takeChild(parent->indexOfChild(item));
			user_unbind_.unbind(shortcut, func);
		}
		}
	}
}


void PrefShortcuts::deactivateShortcuts(QList<QTreeWidgetItem*> const & items)
{
	for (auto item : items) {
		string shortcut = fromqstr(item->data(1, Qt::UserRole).toString());
		string lfun = fromqstr(item->text(0));
		FuncRequest const func = lyxaction.lookupFunc(lfun);

		switch (itemType(*item)) {
		case KeyMap::System:
			// for system bind, we do not touch the item
			// but add an user unbind item
			user_unbind_.bind(shortcut, func);
			setItemType(item, KeyMap::UserUnbind);
			break;

		case KeyMap::UserBind: {
			// for user_bind, we remove this bind
			QTreeWidgetItem * parent = item->parent();
			int itemIdx = parent->indexOfChild(item);
			parent->takeChild(itemIdx);
			user_bind_.unbind(shortcut, func);
			unhideEmpty(item->text(0), false);
			break;
		}
		default:
			break;
		}
	}
}


void PrefShortcuts::selectBind()
{
	QString file = form_->browsebind(internalPath(bindFileED->text()));
	if (!file.isEmpty()) {
		bindFileED->setText(file);
		system_bind_ = KeyMap();
		system_bind_.read(fromqstr(file));
		updateShortcutsTW();
	}
}


void PrefShortcuts::on_modifyPB_pressed()
{
	modifyShortcut();
}


void PrefShortcuts::on_newPB_pressed()
{
	shortcut_->lfunLE->clear();
	shortcut_->shortcutWG->reset();
	save_lfun_ = QString();
	shortcut_->exec();
}


void PrefShortcuts::on_removePB_pressed()
{
	changed();
	removeShortcut();
}


void PrefShortcuts::on_searchLE_textEdited()
{
	if (searchLE->text().isEmpty()) {
		// show all hidden items
		QTreeWidgetItemIterator it(shortcutsTW, QTreeWidgetItemIterator::Hidden);
		for (; *it; ++it)
			(*it)->setHidden(isAlwaysHidden(**it));
		// close all categories
		for (int i = 0; i < shortcutsTW->topLevelItemCount(); ++i)
			shortcutsTW->collapseItem(shortcutsTW->topLevelItem(i));
		return;
	}
	// search both columns
	QList<QTreeWidgetItem *> matched = shortcutsTW->findItems(searchLE->text(),
		Qt::MatchFlags(Qt::MatchContains | Qt::MatchRecursive), 0);
	matched += shortcutsTW->findItems(searchLE->text(),
		Qt::MatchFlags(Qt::MatchContains | Qt::MatchRecursive), 1);

	// hide everyone (to avoid searching in matched QList repeatedly
	QTreeWidgetItemIterator it(shortcutsTW, QTreeWidgetItemIterator::Selectable);
	while (*it)
		(*it++)->setHidden(true);
	// show matched items
	for (auto & item : matched)
		if (!isAlwaysHidden(*item)) {
			item->setHidden(false);
			if (item->parent())
				item->parent()->setExpanded(true);
		}
}


docstring makeCmdString(FuncRequest const & f)
{
	docstring actionStr = from_ascii(lyxaction.getActionName(f.action()));
	if (!f.argument().empty())
		actionStr += " " + f.argument();
	return actionStr;
}


FuncRequest PrefShortcuts::currentBinding(KeySequence const & k)
{
	FuncRequest res = user_bind_.getBinding(k);
	if (res != FuncRequest::unknown)
		return res;
	res = system_bind_.getBinding(k);

	// Check if it is unbound. Note: user_unbind_ can only unbind one
	// FuncRequest per key sequence.
	if (user_unbind_.getBinding(k) == res)
		return FuncRequest::unknown;
	return res;
}


bool PrefShortcuts::validateNewShortcut(FuncRequest const & func,
                                        KeySequence const & k,
                                        QString const & lfun_to_modify)
{
	if (func.action() == LFUN_UNKNOWN_ACTION) {
		Alert::error(_("Failed to create shortcut"),
			_("Unknown or invalid LyX function"));
		activatePrefsWindow(form_);
		return false;
	}

	// It is not currently possible to bind Hidden lfuns such as self-insert. In
	// the future, to remove this limitation, see GuiPrefs::insertShortcutItem
	// and how it is used in GuiPrefs::shortcutOkPressed.
	if (lyxaction.getActionType(func.action()) == LyXAction::Hidden) {
		Alert::error(_("Failed to create shortcut"),
			_("This LyX function is hidden and cannot be bound."));
		activatePrefsWindow(form_);
		return false;
	}

	if (k.length() == 0) {
		Alert::error(_("Failed to create shortcut"),
			_("Invalid or empty key sequence"));
		activatePrefsWindow(form_);
		return false;
	}

	FuncRequest oldBinding = currentBinding(k);
	if (oldBinding == func)
		// nothing to change
		return false;

	// Check whether the key sequence is a prefix for other shortcuts.
	if (oldBinding == FuncRequest::prefix) {
		docstring const new_action_string = makeCmdString(func);
		docstring const text = bformat(_("Shortcut `%1$s' is already a prefix for other commands.\n"
						 "Are you sure you want to unbind these commands and bind it to %2$s?"),
									   k.print(KeySequence::ForGui), new_action_string);
		int ret = Alert::prompt(_("Redefine shortcut?"),
					text, 0, 1, _("&Redefine"), _("&Cancel"));
		activatePrefsWindow(form_);
		if (ret != 0)
			return false;
		QString const sequence_text = toqstr(k.print(KeySequence::ForGui));
		QList<QTreeWidgetItem*> items = shortcutsTW->findItems(sequence_text,
			Qt::MatchFlags(Qt::MatchStartsWith | Qt::MatchRecursive), 1);
		deactivateShortcuts(items);
		return true;
	}

	// make sure this key isn't already bound---and, if so, prompt user
	// (exclude the lfun the user already wants to modify)
	docstring const action_string = makeCmdString(oldBinding);
	if (oldBinding.action() != LFUN_UNKNOWN_ACTION
	    && lfun_to_modify != toqstr(action_string)) {
		docstring const new_action_string = makeCmdString(func);
		docstring const text = bformat(_("Shortcut `%1$s' is already bound to "
						 "%2$s.\n"
						 "Are you sure you want to unbind the "
						 "current shortcut and bind it to %3$s?"),
					       k.print(KeySequence::ForGui), action_string,
					       new_action_string);
		int ret = Alert::prompt(_("Redefine shortcut?"),
					text, 0, 1, _("&Redefine"), _("&Cancel"));
		activatePrefsWindow(form_);
		if (ret != 0)
			return false;
		QString const sequence_text = toqstr(k.print(KeySequence::ForGui));
		QList<QTreeWidgetItem*> items = shortcutsTW->findItems(sequence_text,
			Qt::MatchFlags(Qt::MatchExactly | Qt::MatchRecursive), 1);
		deactivateShortcuts(items);
	}
	return true;
}


void PrefShortcuts::shortcutOkPressed()
{
	QString const new_lfun = shortcut_->lfunLE->text();
	FuncRequest const func = lyxaction.lookupFunc(fromqstr(new_lfun));
	KeySequence k = shortcut_->shortcutWG->getKeySequence();

	// save_lfun_ contains the text of the lfun to modify, if the user clicked
	// "modify", or is empty if they clicked "new" (which I do not really like)
	if (!validateNewShortcut(func, k, save_lfun_))
		return;

	if (!save_lfun_.isEmpty()) {
		// real modification of the lfun's shortcut,
		// so remove the previous one
		QList<QTreeWidgetItem*> to_modify = shortcutsTW->selectedItems();
		deactivateShortcuts(to_modify);
	}

	shortcut_->accept();

	QTreeWidgetItem * item = insertShortcutItem(func, k, KeyMap::UserBind);
	if (item) {
		user_bind_.bind(&k, func);
		shortcutsTW->sortItems(0, Qt::AscendingOrder);
		if (item->parent())
			item->parent()->setExpanded(true);
		shortcutsTW->setCurrentItem(item);
		shortcutsTW->scrollToItem(item);
	} else {
		Alert::error(_("Failed to create shortcut"),
			_("Can not insert shortcut to the list"));
		activatePrefsWindow(form_);
		return;
	}
}


void PrefShortcuts::shortcutCancelPressed()
{
	shortcut_->shortcutWG->reset();
}


void PrefShortcuts::shortcutClearPressed()
{
	shortcut_->shortcutWG->reset();
}


void PrefShortcuts::shortcutRemovePressed()
{
	shortcut_->shortcutWG->removeFromSequence();
}


/////////////////////////////////////////////////////////////////////
//
// PrefIdentity
//
/////////////////////////////////////////////////////////////////////

PrefIdentity::PrefIdentity(GuiPreferences * form)
	: PrefModule(QString(), N_("Identity"), form)
{
	setupUi(this);

	connect(nameED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(emailED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(initialsED, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));

	nameED->setValidator(new NoNewLineValidator(nameED));
	emailED->setValidator(new NoNewLineValidator(emailED));
	initialsED->setValidator(new NoNewLineValidator(initialsED));
}


void PrefIdentity::applyRC(LyXRC & rc) const
{
	rc.user_name = fromqstr(nameED->text());
	rc.user_email = fromqstr(emailED->text());
	rc.user_initials = fromqstr(initialsED->text());
}


void PrefIdentity::updateRC(LyXRC const & rc)
{
	nameED->setText(toqstr(rc.user_name));
	emailED->setText(toqstr(rc.user_email));
	initialsED->setText(toqstr(rc.user_initials));
}



/////////////////////////////////////////////////////////////////////
//
// GuiPreferences
//
/////////////////////////////////////////////////////////////////////

GuiPreferences::GuiPreferences(GuiView & lv)
	: GuiDialog(lv, "prefs", qt_("Preferences"))
{
	setupUi(this);

	QDialog::setModal(false);

	connect(buttonBox, SIGNAL(clicked(QAbstractButton *)),
		this, SLOT(slotButtonBox(QAbstractButton *)));

	addModule(new PrefUserInterface(this));
	addModule(new PrefDocHandling(this));
	addModule(new PrefEdit(this));
	addModule(new PrefShortcuts(this));
	PrefScreenFonts * screenfonts = new PrefScreenFonts(this);
	connect(this, SIGNAL(prefsApplied(LyXRC const &)),
			screenfonts, SLOT(updateScreenFontSizes(LyXRC const &)));
	addModule(screenfonts);
	addModule(new PrefColors(this));
	addModule(new PrefDisplay(this));
	addModule(new PrefInput(this));
	addModule(new PrefCompletion(this));

	addModule(new PrefPaths(this));

	addModule(new PrefIdentity(this));

	addModule(new PrefLanguage(this));
	addModule(new PrefSpellchecker(this));

	PrefOutput * output = new PrefOutput(this);
	addModule(output);
	addModule(new PrefLatex(this));

	PrefConverters * converters = new PrefConverters(this);
	PrefFileformats * formats = new PrefFileformats(this);
	connect(formats, SIGNAL(formatsChanged()),
			converters, SLOT(updateGui()));
	addModule(converters);
	addModule(formats);

	prefsPS->setCurrentPanel("User Interface");

	bc().setPolicy(ButtonPolicy::PreferencesPolicy);
	bc().setOK(buttonBox->button(QDialogButtonBox::Ok));
	bc().setApply(buttonBox->button(QDialogButtonBox::Apply));
	bc().setCancel(buttonBox->button(QDialogButtonBox::Cancel));
	bc().setRestore(buttonBox->button(QDialogButtonBox::Reset));

	guilyxfiles_ = new GuiLyXFiles(lv);
	connect(guilyxfiles_, SIGNAL(fileSelected(QString)),
			this, SLOT(slotFileSelected(QString)));
}


void GuiPreferences::addModule(PrefModule * module)
{
	LASSERT(module, return);
	if (module->category().isEmpty())
		prefsPS->addPanel(module, module->title());
	else
		prefsPS->addPanel(module, module->title(), module->category());
	connect(module, SIGNAL(changed()), this, SLOT(change_adaptor()));
	modules_.push_back(module);
}


void GuiPreferences::change_adaptor()
{
	changed();
}


void GuiPreferences::applyRC(LyXRC & rc) const
{
	size_t end = modules_.size();
	for (size_t i = 0; i != end; ++i)
		modules_[i]->applyRC(rc);
}


void GuiPreferences::updateRC(LyXRC const & rc)
{
	size_t const end = modules_.size();
	for (size_t i = 0; i != end; ++i)
		modules_[i]->updateRC(rc);
}


void GuiPreferences::applyView()
{
	applyRC(rc());
}


bool GuiPreferences::initialiseParams(string const &)
{
	rc_ = lyxrc;
	formats_ = theFormats();
	converters_ = theConverters();
	converters_.update(formats_);
	movers_ = theMovers();
	colors_.clear();

	updateRC(rc_);
	// Make sure that the bc is in the INITIAL state
	if (bc().policy().buttonStatus(ButtonPolicy::RESTORE))
		bc().restore();

	return true;
}


void GuiPreferences::dispatchParams()
{
	ostringstream ss;
	rc_.write(ss, true);
	dispatch(FuncRequest(LFUN_LYXRC_APPLY, ss.str()));
	// issue prefsApplied signal. This will update the
	// localized screen font sizes.
	prefsApplied(rc_);
	// FIXME: these need lfuns
	// FIXME UNICODE
	Author const & author =
		Author(from_utf8(rc_.user_name), from_utf8(rc_.user_email),
		       from_utf8(rc_.user_initials));
	theBufferList().recordCurrentAuthor(author);

	theFormats() = formats_;

	theConverters() = converters_;
	theConverters().update(formats_);
	theConverters().buildGraph();
	theBufferList().invalidateConverterCache();

	theMovers() = movers_;

	for (string const & color : colors_)
		dispatch(FuncRequest(LFUN_SET_COLOR, color));
	colors_.clear();

	// Save permanently
	if (!tempSaveCB->isChecked())
		dispatch(FuncRequest(LFUN_PREFERENCES_SAVE));
}


void GuiPreferences::setColor(ColorCode col, ColorNamePair const & hex)
{
	colors_.push_back(lcolor.getLyXName(col) + ' ' +
	                  fromqstr(hex.first) + ' ' + fromqstr(hex.second));
}


void GuiPreferences::slotFileSelected(QString const file)
{
	uifile_ = file;
}


QString GuiPreferences::browseLibFile(QString const & dir,
	QString const & name, QString const & ext)
{
	uifile_.clear();

	guilyxfiles_->passParams(fromqstr(dir));
	guilyxfiles_->selectItem(name);
	guilyxfiles_->exec();

	activatePrefsWindow(this);

	QString const result = uifile_;

	// remove the extension if it is the default one
	QString noextresult;
	if (getExtension(result) == ext)
		noextresult = removeExtension(result);
	else
		noextresult = result;

	// remove the directory, if it is the default one
	QString const file = onlyFileName(noextresult);
	if (toqstr(libFileSearch(dir, file, ext).absFileName()) == result)
		return file;
	else
		return noextresult;
}


QString GuiPreferences::browsebind(QString const & file)
{
	return browseLibFile("bind", file, "bind");
}


QString GuiPreferences::browseUI(QString const & file)
{
	return browseLibFile("ui", file, "ui");
}


QString GuiPreferences::browsekbmap(QString const & file)
{
	return browseLibFile("kbd", file, "kmap");
}


QString GuiPreferences::browse(QString const & file,
	QString const & title)
{
	return browseFile(file, title, QStringList(), true);
}


/////////////////////////////////////////////////////////////////////
//
// SetColor
//
/////////////////////////////////////////////////////////////////////

// This class enables undo/redo of setColor()
SetColor::SetColor(QStandardItem *item, QColor const &new_color,
                   QString const &old_color, ColorNamePairs &new_color_list,
                   bool const autoapply, PrefColors* color_module,
                   QUndoCommand* uc_parent)
    : PrefColors(color_module->form_), QUndoCommand(uc_parent),
      autoapply_(autoapply), item_(*item), new_color_(new_color),
      old_color_(old_color), newcolors_(new_color_list), parent_(color_module)
{
	setText(QString("Color %1 is changed to %2 (light) and %3 (dark)")
	        .arg(lcolors_[item_.row()])
	        .arg(newcolors_[item_.row()].first, newcolors_[item_.row()].second));
}


void SetColor::redo()
{
	setColor(new_color_);

	// set button statuses
	parent_->setResetButtonStatus(false);
	parent_->setUndoRedoButtonStatuses(false);
	parent_->findThemeFromColorSet();
}


void SetColor::undo()
{
	setColor(old_color_);

	// set button statuses
	parent_->setResetButtonStatus(true);
	parent_->setUndoRedoButtonStatuses(true);
	parent_->findThemeFromColorSet();
}


void SetColor::setColor(QColor const &color)
{
	if (item_.column() == 1)
		newcolors_[size_t(item_.row())].second = color.name();
	else
		newcolors_[size_t(item_.row())].first = color.name();
	setSwatch(&item_, color);

	parent_->form_->update();

	// emit signal
	changed();

	if (autoapply_) {
		parent_->form_->setColor(lcolors_[item_.row()], newcolors_[item_.row()]);
		parent_->form_->dispatchParams();
	}
}


/////////////////////////////////////////////////////////////////////
//
// ColorSwatchDelegate
//
/////////////////////////////////////////////////////////////////////

ColorSwatchDelegate::ColorSwatchDelegate(QObject *parent)
    : QStyledItemDelegate(parent), button_(new QPushButton)
{
	pane_ = static_cast<PrefColors*>(parent);
}


void ColorSwatchDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
	Q_ASSERT(index.isValid());

	QStyleOptionViewItem opt = option;

	initStyleOption(&opt, index);

	const QWidget *widget = option.widget;
	QStyle *style = widget ? widget->style() : QApplication::style();
	int const column = index.column();

	if (column < 2) {
		opt.rect = option.rect;
		QPixmap pixmap(pane_->swatch_width_, pane_->swatch_height_);
		QColor color =
		        pane_->colorsTV_model_.data(index, Qt::DecorationRole).
		        value<QColor>();
		if (index.flags().testFlag(Qt::ItemIsEnabled))
			pixmap.fill(color);
		else
			pixmap.fill(Qt::transparent);
		style->drawItemPixmap(painter, opt.rect, Qt::AlignCenter, pixmap);
	} else {
		opt.displayAlignment = Qt::AlignVCenter;
		style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
	}
}


} // namespace frontend
} // namespace lyx

#include "moc_GuiPrefs.cpp"
