// -*- C++ -*-
/**
 * \file GuiErrorList.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alfredo Braunstein
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUIERRORLIST_H
#define GUIERRORLIST_H

#include "GuiDialog.h"
#include "ErrorList.h"
#include "ui_ErrorListUi.h"

namespace lyx {
namespace frontend {

class GuiErrorList : public GuiDialog, public Ui::ErrorListUi
{
	Q_OBJECT

public:
	GuiErrorList(GuiView & lv);

public Q_SLOTS:
	/// select an entry
	void select();
	/// open the LaTeX log
	void viewLog();
	/// show the output file despite compilation errors
	void showAnyway();

private:
	///
	void showEvent(QShowEvent *) override;
	///
	void paramsToDialog();
	///
	bool isBufferDependent() const override { return true; }
	///
	bool initialiseParams(std::string const & data) override;
	///
	void clearParams() override {}
	///
	void dispatchParams() override {}
	///
	bool canApply() const override { return true; }

	/// goto this error in the parent bv. Returns success.
	bool goTo(int item);
	///
	ErrorList const & errorList() const;
private:
	///
	std::string error_type_;
	///
	mutable ErrorList error_list_;
	///
	Buffer const * buf_;
	/// the parent document name
	docstring name_;
	///
	bool from_master_;
	///
	int item_ = 0;
};

} // namespace frontend
} // namespace lyx

#endif // GUIERRORLIST_H
