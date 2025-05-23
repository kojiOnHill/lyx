/**
 * \file GuiSymbols.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiSymbols.h"

#include "GuiView.h"
#include "qt_helpers.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Cursor.h"
#include "Encoding.h"
#include "FuncRequest.h"

#include "support/debug.h"
#include "support/gettext.h"

#include <QChar>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QString>

#include <cstdio>

using namespace std;

namespace lyx {
namespace frontend {


namespace {

/// name of unicode block, start and end code point
struct UnicodeBlocks {
	char const * name;
	QString qname;
	char_type start;
	char_type end;
};


/// all unicode blocks with start and end code point
UnicodeBlocks unicode_blocks[] = {
	{ N_("Basic Latin"), QString(), 0x0000, 0x007f },
	{ N_("Latin-1 Supplement"), QString(), 0x0080, 0x00ff },
	{ N_("Latin Extended-A"), QString(), 0x0100, 0x017f },
	{ N_("Latin Extended-B"), QString(), 0x0180, 0x024f },
	{ N_("IPA Extensions"), QString(), 0x0250, 0x02af },
	{ N_("Spacing Modifier Letters"), QString(), 0x02b0, 0x02ff },
	{ N_("Combining Diacritical Marks"), QString(), 0x0300, 0x036f },
	{ N_("Greek"), QString(), 0x0370, 0x03ff },
	{ N_("Cyrillic"), QString(), 0x0400, 0x04ff },
	{ N_("Armenian"), QString(), 0x0530, 0x058f },
	{ N_("Hebrew"), QString(), 0x0590, 0x05ff },
	{ N_("Arabic"), QString(), 0x0600, 0x06ff },
	{ N_("Devanagari"), QString(), 0x0900, 0x097f },
	{ N_("Bengali"), QString(), 0x0980, 0x09ff },
	{ N_("Gurmukhi"), QString(), 0x0a00, 0x0a7f },
	{ N_("Gujarati"), QString(), 0x0a80, 0x0aff },
	{ N_("Oriya"), QString(), 0x0b00, 0x0b7f },
	{ N_("Tamil"), QString(), 0x0b80, 0x0bff },
	{ N_("Telugu"), QString(), 0x0c00, 0x0c7f },
	{ N_("Kannada"), QString(), 0x0c80, 0x0cff },
	{ N_("Malayalam"), QString(), 0x0d00, 0x0d7f },
	{ N_("Thai"), QString(), 0x0e00, 0x0e7f },
	{ N_("Lao"), QString(), 0x0e80, 0x0eff },
	{ N_("Tibetan"), QString(), 0x0f00, 0x0fbf },
	{ N_("Georgian"), QString(), 0x10a0, 0x10ff },
	{ N_("Hangul Jamo"), QString(), 0x1100, 0x11ff },
	{ N_("Phonetic Extensions"), QString(), 0x1d00, 0x1d7f },
	{ N_("Latin Extended Additional"), QString(), 0x1e00, 0x1eff },
	{ N_("Greek Extended"), QString(), 0x1f00, 0x1fff },
	{ N_("General Punctuation"), QString(), 0x2000, 0x206f },
	{ N_("Superscripts and Subscripts"), QString(), 0x2070, 0x209f },
	{ N_("Currency Symbols"), QString(), 0x20a0, 0x20cf },
	{ N_("Combining Diacritical Marks for Symbols"), QString(), 0x20d0, 0x20ff },
	{ N_("Letterlike Symbols"), QString(), 0x2100, 0x214f },
	{ N_("Number Forms"), QString(), 0x2150, 0x218f },
	{ N_("Arrows"), QString(), 0x2190, 0x21ff },
	{ N_("Mathematical Operators"), QString(), 0x2200, 0x22ff },
	{ N_("Miscellaneous Technical"), QString(), 0x2300, 0x23ff },
	{ N_("Control Pictures"), QString(), 0x2400, 0x243f },
	{ N_("Optical Character Recognition"), QString(), 0x2440, 0x245f },
	{ N_("Enclosed Alphanumerics"), QString(), 0x2460, 0x24ff },
	{ N_("Box Drawing"), QString(), 0x2500, 0x257f },
	{ N_("Block Elements"), QString(), 0x2580, 0x259f },
	{ N_("Geometric Shapes"), QString(), 0x25a0, 0x25ff },
	{ N_("Miscellaneous Symbols"), QString(), 0x2600, 0x26ff },
	{ N_("Dingbats"), QString(), 0x2700, 0x27bf },
	{ N_("Miscellaneous Mathematical Symbols-A"), QString(), 0x27c0, 0x27ef },
	{ N_("CJK Symbols and Punctuation"), QString(), 0x3000, 0x303f },
	{ N_("Hiragana"), QString(), 0x3040, 0x309f },
	{ N_("Katakana"), QString(), 0x30a0, 0x30ff },
	{ N_("Bopomofo"), QString(), 0x3100, 0x312f },
	{ N_("Hangul Compatibility Jamo"), QString(), 0x3130, 0x318f },
	{ N_("Kanbun"), QString(), 0x3190, 0x319f },
	{ N_("Enclosed CJK Letters and Months"), QString(), 0x3200, 0x32ff },
	{ N_("CJK Compatibility"), QString(), 0x3300, 0x33ff },
	{ N_("CJK Unified Ideographs"), QString(), 0x4e00, 0x9fa5 },
	{ N_("Hangul Syllables"), QString(), 0xac00, 0xd7a3 },
	{ N_("High Surrogates"), QString(), 0xd800, 0xdb7f },
	{ N_("Private Use High Surrogates"), QString(), 0xdb80, 0xdbff },
	{ N_("Low Surrogates"), QString(), 0xdc00, 0xdfff },
	{ N_("Private Use Area"), QString(), 0xe000, 0xf8ff },
	{ N_("CJK Compatibility Ideographs"), QString(), 0xf900, 0xfaff },
	{ N_("Alphabetic Presentation Forms"), QString(), 0xfb00, 0xfb4f },
	{ N_("Arabic Presentation Forms-A"), QString(), 0xfb50, 0xfdff },
	{ N_("Combining Half Marks"), QString(), 0xfe20, 0xfe2f },
	{ N_("CJK Compatibility Forms"), QString(), 0xfe30, 0xfe4f },
	{ N_("Small Form Variants"), QString(), 0xfe50, 0xfe6f },
	{ N_("Arabic Presentation Forms-B"), QString(), 0xfe70, 0xfeff },
	{ N_("Halfwidth and Fullwidth Forms"), QString(), 0xff00, 0xffef },
	{ N_("Specials"), QString(), 0xfff0, 0xffff },
	{ N_("Linear B Syllabary"), QString(), 0x10000, 0x1007f },
	{ N_("Linear B Ideograms"), QString(), 0x10080, 0x100ff },
	{ N_("Aegean Numbers"), QString(), 0x10100, 0x1013f },
	{ N_("Ancient Greek Numbers"), QString(), 0x10140, 0x1018f },
	{ N_("Old Italic"), QString(), 0x10300, 0x1032f },
	{ N_("Gothic"), QString(), 0x10330, 0x1034f },
	{ N_("Ugaritic"), QString(), 0x10380, 0x1039f },
	{ N_("Old Persian"), QString(), 0x103a0, 0x103df },
	{ N_("Deseret"), QString(), 0x10400, 0x1044f },
	{ N_("Shavian"), QString(), 0x10450, 0x1047f },
	{ N_("Osmanya"), QString(), 0x10480, 0x104af },
	{ N_("Cypriot Syllabary"), QString(), 0x10800, 0x1083f },
	{ N_("Kharoshthi"), QString(), 0x10a00, 0x10a5f },
	{ N_("Byzantine Musical Symbols"), QString(), 0x1d000, 0x1d0ff },
	{ N_("Musical Symbols"), QString(), 0x1d100, 0x1d1ff },
	{ N_("Ancient Greek Musical Notation"), QString(), 0x1d200, 0x1d24f },
	{ N_("Tai Xuan Jing Symbols"), QString(), 0x1d300, 0x1d35f },
	{ N_("Mathematical Alphanumeric Symbols"), QString(), 0x1d400, 0x1d7ff },
	{ N_("CJK Unified Ideographs Extension B"), QString(), 0x20000, 0x2a6d6 },
	{ N_("CJK Compatibility Ideographs Supplement"), QString(), 0x2f800, 0x2fa1f },
	{ N_("Tags"), QString(), 0xe0000, 0xe007f },
	{ N_("Variation Selectors Supplement"), QString(), 0xe0100, 0xe01ef },
	{ N_("Supplementary Private Use Area-A"), QString(), 0xf0000, 0xffffd },
	{ N_("Supplementary Private Use Area-B"), QString(), 0x100000, 0x10fffd }
};

const int no_blocks = sizeof(unicode_blocks) / sizeof(UnicodeBlocks);


QString getBlock(char_type c)
{
	// store an educated guess for the next search
	// 0 <= lastBlock < no_blocks
	// FIXME THREAD
	static int lastBlock = 0;

	// "clever reset"
	if (c < 0x7f)
		lastBlock = 0;

	// c falls into a covered area, and we can guess which
	if (c >= unicode_blocks[lastBlock].start
	    && c <= unicode_blocks[lastBlock].end)
		return unicode_blocks[lastBlock].qname;

	// c falls into an uncovered area, but we can guess which
	if (c > unicode_blocks[lastBlock].end
	    && (lastBlock == no_blocks-1 || c < unicode_blocks[lastBlock + 1].start))
		return QString();

	// guessing was wrong so far. do a real search.
	int i = 0;
	while (i < no_blocks && c > unicode_blocks[i].end)
		++i;

	if (i == no_blocks)
		return QString();

	if (c < unicode_blocks[i].start) {
		// 0 < i < no_blocks
		// cache the previous block for guessing next time
		lastBlock = i - 1;
		return QString();
	}

	// 0 <= i < no_blocks
	// cache the last block for guessing next time
	lastBlock = i;
	return unicode_blocks[lastBlock].qname;
}


} // namespace


/////////////////////////////////////////////////////////////////////
//
// GuiSymbols::Model
//
/////////////////////////////////////////////////////////////////////

class GuiSymbols::Model : public QAbstractListModel
{
public:
	Model(GuiSymbols * parent)
		: QAbstractListModel(parent), encoding_(nullptr)
	{}

	QModelIndex index(int row, int column, QModelIndex const &) const override
	{
		return createIndex(row, column);
	}

	QModelIndex parent(QModelIndex const &) const override
	{
		return QModelIndex();
	}

	int rowCount(QModelIndex const &) const override
	{
		return symbols_.count();
	}

	QVariant data(QModelIndex const & index, int role) const override
	{
		if (!index.isValid())
			return QVariant();

		char_type c = symbols_.at(index.row());

		switch (role) {
		case Qt::TextAlignmentRole:
			return QVariant(Qt::AlignCenter);
		case Qt::DisplayRole:
			return toqstr(c);
		case Qt::ToolTipRole: {
			QString latex;
			if (encoding_) {
				// how is the character output in the current encoding?
				docstring const code = encoding_->latexChar(c).first;
				// only show it when it is not coded by itself
				if (code != docstring(1, c))
					latex = qt_("<p>LaTeX code: %1</p>").arg(toqstr(code));
			}
			return formatToolTip(QString("<p align=center><span "
			                             "style=\"font-size: xx-large;\">%1"
			                             "</span><br>U+%2</p>%3")
			                     .arg(toqstr(c))
			                     .arg(QString("%1").arg(c, 0, 16).toUpper())
			                     .arg(latex));
		}
		case Qt::SizeHintRole:
			// Fix many symbols not displaying in combination with
			// setUniformItemSizes
			return QSize(1000,1000);
		default:
			return QVariant();
		}
	}

	void setSymbols(QList<char_type> const & symbols, Encoding const * encoding)
	{
		beginResetModel();
		symbols_ = symbols;
		encoding_ = encoding;
		endResetModel();
	}

private:
	friend class GuiSymbols;

	QList<char_type> symbols_;
	Encoding const * encoding_;
};


/////////////////////////////////////////////////////////////////////
//
// GuiSymbols
//
/////////////////////////////////////////////////////////////////////

GuiSymbols::GuiSymbols(GuiView & lv)
	: DialogView(lv, "symbols", qt_("Symbols")), encoding_("ascii"),
		model_(new Model(this))
{
	setupUi(this);

	// Translate names
	for (int i = 0 ; i < no_blocks; ++i)
		unicode_blocks[i].qname = qt_(unicode_blocks[i].name);

	setFocusProxy(symbolsLW);

	symbolsLW->setViewMode(QListView::IconMode);
	symbolsLW->setLayoutMode(QListView::Batched);
	symbolsLW->setBatchSize(1000);
	symbolsLW->setUniformItemSizes(true);

	// increase the display size of the symbols a bit
	QFont font = symbolsLW->font();
	const int size = font.pointSize() + 3;
	font.setPointSize(size);
	symbolsLW->setFont(font);
	QFontMetrics fm(font);
	const int cellHeight = fm.height() + 6;
	// FIXME: using at least cellHeight because of
	// QFontMetrics::maxWidth() is returning 0 with Qt/Cocoa on Mac OS
	const int cellWidth = max(cellHeight - 2, fm.maxWidth() + 4);
	symbolsLW->setGridSize(QSize(cellWidth, cellHeight));
	symbolsLW->setModel(model_);
}


void GuiSymbols::updateView()
{
	chosenLE->clear();

	string new_encoding = bufferview()->cursor().getEncoding()->name();
	if (buffer().params().inputenc != "auto-legacy" &&
	    buffer().params().inputenc != "auto-legacy-plain")
		new_encoding = buffer().params().encoding().name();
	if (new_encoding == encoding_)
		// everything up to date
		return;
	if (!new_encoding.empty())
		encoding_ = new_encoding;
	bool const utf8 = toqstr(encoding_).startsWith("utf8");
	if (utf8)
		categoryFilterCB->setChecked(false);
	//categoryFilterCB->setEnabled(!utf8);
	updateSymbolList();
}


void GuiSymbols::enableView(bool enable)
{
	chosenLE->setEnabled(enable);
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enable);
	buttonBox->button(QDialogButtonBox::Apply)->setEnabled(enable);
	if (enable)
		buttonBox->button(QDialogButtonBox::Close)->setText(qt_("Cancel"));
	else
		buttonBox->button(QDialogButtonBox::Close)->setText(qt_("Close"));
}


void GuiSymbols::on_buttonBox_clicked(QAbstractButton * button)
{
	QDialogButtonBox * bbox = qobject_cast<QDialogButtonBox*>(sender());
	switch (bbox->standardButton(button)) {
	case QDialogButtonBox::Ok:
		slotOK();
		break;
	case QDialogButtonBox::Apply:
		dispatchParams();
		break;
	case QDialogButtonBox::Close:
		hide();
		break;
	default:
		break;
	}
}


void GuiSymbols::slotOK()
{
	dispatchParams();
	hide();
}


void GuiSymbols::on_symbolsLW_activated(QModelIndex const &)
{
	slotOK();
}


void GuiSymbols::on_chosenLE_textChanged(QString const & text)
{
	bool const empty_sel = text.isEmpty();
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!empty_sel);
	buttonBox->button(QDialogButtonBox::Apply)->setEnabled(!empty_sel);
}


void GuiSymbols::on_chosenLE_returnPressed()
{
	slotOK();
}


void GuiSymbols::on_symbolsLW_clicked(QModelIndex const & index)
{
	QString const text = model_->data(index, Qt::DisplayRole).toString();
	if (text.isEmpty())
		return;
	if (chosenLE->isEnabled())
		chosenLE->insert(text);
	if (categoryFilterCB->isChecked()) {
		QString const category = getBlock(text.data()->unicode());
		categoryCO->setCurrentIndex(categoryCO->findText(category));
	}
}


void GuiSymbols::on_categoryCO_activated(int)
{
	if (!categoryFilterCB->isChecked())
		updateSymbolList(false);
	else
		scrollToItem(categoryCO->currentText());
}


void GuiSymbols::on_categoryFilterCB_toggled(bool on)
{
	updateSymbolList(on);
	if (on)
		scrollToItem(categoryCO->currentText());
}


void GuiSymbols::scrollToItem(QString const & category)
{
	if (used_blocks.find(category) == used_blocks.end())
		return;
	int row = used_blocks[category];
	QModelIndex index = symbolsLW->model()->index(row, 0, QModelIndex());
	symbolsLW->scrollTo(index, QAbstractItemView::PositionAtTop);
}


void GuiSymbols::updateSymbolList(bool update_combo)
{
	QString const category = categoryCO->currentText();
	char_type range_start = 0x0000;
	char_type range_end = 0x110000;
	QList<char_type> s;
	if (update_combo) {
		used_blocks.clear();
		categoryCO->clear();
	}
	bool const show_all = categoryFilterCB->isChecked();

	Encoding const * const enc = encodings.fromLyXName(encoding_);

	if (symbols_.empty() || update_combo)
		symbols_ = enc->symbolsList();

	if (!show_all) {
		for (int i = 0 ; i < no_blocks; ++i)
			if (unicode_blocks[i].qname == category) {
				range_start = unicode_blocks[i].start;
				range_end = unicode_blocks[i].end;
				break;
			}
	}

	int numItem = 0;
	for (char_type c : symbols_) {
		if (!update_combo && !show_all && (c < range_start || c > range_end))
			continue;
		QChar::Category const cat = QChar::category(uint(c));
		// we do not want control or space characters
		if (cat == QChar::Other_Control || cat == QChar::Separator_Space)
			continue;
		++numItem;
		if (show_all || (c >= range_start && c <= range_end))
			s.append(c);
		if (update_combo)
			used_blocks.insert({getBlock(c), numItem});
	}
	model_->setSymbols(s, enc);

	if (update_combo) {
		// update category combo
		for (UsedBlocks::iterator it = used_blocks.begin();
		     it != used_blocks.end(); ++it) {
			categoryCO->addItem(it->first);
		}
	}

	int old = categoryCO->findText(category);
	if (old != -1)
		categoryCO->setCurrentIndex(old);
	else if (update_combo) {
		// restart with a non-empty block
		// this happens when the encoding changes when moving the cursor
		categoryCO->setCurrentIndex(0);
		updateSymbolList(false);
	}
}


void GuiSymbols::dispatchParams()
{
	dispatch(FuncRequest(getLfun(), fromqstr(chosenLE->text())));
}


} // namespace frontend
} // namespace lyx

#include "moc_GuiSymbols.cpp"
