/**
 * \file InsetParamsDialog.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetParamsDialog.h"

#include "GuiBox.h"
#include "GuiBranch.h"
#include "GuiBibitem.h"
#include "GuiCounter.h"
#include "GuiERT.h"
#include "GuiHSpace.h"
#include "GuiHyperlink.h"
#include "GuiInfo.h"
#include "GuiLabel.h"
#include "GuiLine.h"
#include "GuiNomenclature.h"
#include "GuiPrintNomencl.h"
#include "GuiTabular.h"
#include "GuiVSpace.h"
#include "FloatPlacement.h"

#include "InsetParamsWidget.h"
#include "qt_helpers.h"

#include "Buffer.h"
#include "buffer_funcs.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Cursor.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "LyX.h"

#include "support/debug.h"
#include "support/lstrings.h"

#include <QDialogButtonBox>

using namespace std;
using namespace lyx::support;

namespace lyx {
namespace frontend {

/////////////////////////////////////////////////////////////////
//
// InsetParamsDialog::Private
//
/////////////////////////////////////////////////////////////////

struct InsetParamsDialog::Private
{
	Private() : widget_(nullptr), inset_(nullptr), changed_(false) {}
	///
	InsetParamsWidget * widget_;
	/// The inset that was used at last Restore or Apply operation.
	Inset const * inset_;
	/// Set to true whenever the dialog is changed and set back to
	/// false when the dialog is applied or restored.
	bool changed_;
};

/////////////////////////////////////////////////////////////////
//
// InsetParamsDialog
//
/////////////////////////////////////////////////////////////////

InsetParamsDialog::InsetParamsDialog(GuiView & lv, InsetParamsWidget * widget)
	: DialogView(lv, toqstr(insetName(widget->insetCode())),
	widget->dialogTitle()), d(new Private)
{
	setupUi(this);
	// Remove margins since the widget is embedded in dialog which provides them
	widget->layout()->setContentsMargins(0, 0, 0, 0);
	setInsetParamsWidget(widget);
	immediateApplyCB->setChecked(false);
	synchronizedCB->setChecked(true);
	on_immediateApplyCB_stateChanged(false);
	setFocusProxy(widget);
	newPB = buttonBox->addButton(qt_("Ne&w Inset"),
			     QDialogButtonBox::ActionRole);
	// fix height to minimum
	setFixedHeight(sizeHint().height());
}

InsetParamsDialog::~InsetParamsDialog()
{
	delete d;
}


bool InsetParamsDialog::initialiseParams(std::string const & sdata)
{
	if (!d->widget_->initialiseParams(sdata))
		resetDialog();
	else {
		// Needed to keep the OK/Apply buttons
		// disabled (see #8927)
		d->changed_ = false;
		d->inset_ = inset(d->widget_->insetCode());
	}
	return true;
}


void InsetParamsDialog::setInsetParamsWidget(InsetParamsWidget * widget)
{
	d->widget_ = widget;
	stackedWidget->addWidget(widget);
	stackedWidget->setCurrentWidget(widget);
	connect(d->widget_, SIGNAL(changed()), this, SLOT(onWidget_changed()));
}


void InsetParamsDialog::on_buttonBox_clicked(QAbstractButton * button)
{
	switch (buttonBox->buttonRole(button)) {
	case QDialogButtonBox::AcceptRole: {// OK
		Inset const * i = inset(d->widget_->insetCode());
		if (i)
			applyView();
		else
			newInset();
		hide();
		break;
	}
	case QDialogButtonBox::ApplyRole:
		applyView();
		break;
	case QDialogButtonBox::RejectRole:// Cancel or Close
		hide();
		break;
	case QDialogButtonBox::ResetRole: {
		resetDialog();
		break;
	}
	case QDialogButtonBox::ActionRole:// New Inset
		newInset();
		break;
	default:
		break;
	}
}


void InsetParamsDialog::resetDialog()
{
	updateView(true);
	buttonBox->button(QDialogButtonBox::Reset)->setEnabled(false);
	d->changed_ = false;
	d->inset_ = inset(d->widget_->insetCode());
}


void InsetParamsDialog::newInset()
{
	docstring const argument = d->widget_->dialogToParams();
	dispatch(FuncRequest(d->widget_->creationCode(), argument));
}


bool InsetParamsDialog::newInsetAllowed() const
{
	docstring const argument = d->widget_->dialogToParams();
	FuncRequest const fr = FuncRequest(d->widget_->creationCode(), argument);
	FuncStatus const fs(getStatus(fr));
	return fs.enabled();
}


void InsetParamsDialog::on_immediateApplyCB_stateChanged(int state)
{
	checkWidgets(state == Qt::Checked);
}


void InsetParamsDialog::on_synchronizedCB_stateChanged(int)
{
	checkWidgets(false);
}


docstring InsetParamsDialog::checkWidgets(bool immediate)
{
	bool const read_only = buffer().isReadonly();
	bool const widget_ok = d->widget_->checkWidgets(read_only);
	Inset const * ins = inset(d->widget_->insetCode());
	docstring const argument = d->widget_->dialogToParams();
	bool valid_argument = !argument.empty();
	if (ins)
		valid_argument &= ins->validateModifyArgument(argument);
	FuncCode const code = immediate
		? d->widget_->creationCode() : LFUN_INSET_MODIFY;
	bool const lfun_ok = lyx::getStatus(FuncRequest(code, argument)).enabled();

	bool const changed_inset = ins && (ins != d->inset_ || d->changed_);
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!immediate && widget_ok
							    && !read_only && valid_argument
							    && (!ins || changed_inset));
	bool const can_be_restored = !immediate && !read_only && changed_inset;
	buttonBox->button(QDialogButtonBox::Reset)->setEnabled(can_be_restored);
	buttonBox->button(QDialogButtonBox::Apply)->setEnabled(!immediate
							       && lfun_ok && widget_ok
							       && !read_only && valid_argument
							       && changed_inset);
	immediateApplyCB->setEnabled(ins && !read_only);
	// This seems to be the only way to access custom buttons
	QList<QAbstractButton*> buttons = buttonBox->buttons();
	for (auto & button : buttons) {
		if (buttonBox->buttonRole(button) == QDialogButtonBox::ActionRole)
			button->setEnabled(widget_ok && !read_only
						  && valid_argument
						  && newInsetAllowed());
	}
	synchronizedCB->setEnabled(!immediate);
	return argument;
}


void InsetParamsDialog::onWidget_changed()
{
	d->changed_ = true;
	docstring const argument = checkWidgets(immediateApplyCB->isChecked());
	if (immediateApplyCB->isChecked()
	    && d->widget_->checkWidgets(buffer().isReadonly()))
		dispatch(FuncRequest(LFUN_INSET_MODIFY, argument));
}


void InsetParamsDialog::applyView()
{
	docstring const argument = checkWidgets(immediateApplyCB->isChecked());
	dispatch(FuncRequest(LFUN_INSET_MODIFY, argument));
	d->changed_ = false;
	d->inset_ = inset(d->widget_->insetCode());
	updateView(true);
}


void InsetParamsDialog::updateView(bool update_widget)
{
	if (update_widget) {
		Inset const * i = inset(d->widget_->insetCode());
		if (i) {
			d->widget_->blockSignals(true);
			d->widget_->paramsToDialog(i);
			d->widget_->blockSignals(false);
		}
	}
	checkWidgets(immediateApplyCB->isChecked());
}


void InsetParamsDialog::updateView()
{
	bool const update_widget =
		(synchronizedCB->isChecked() || immediateApplyCB->isChecked());
	updateView(update_widget);

	// Somewhere in the chain this can lose default status (#11417)
	buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
}

} // namespace frontend
} // namespace lyx

#include "moc_InsetParamsDialog.cpp"
