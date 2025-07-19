// -*- C++ -*-
/**
 * \file GuiToolbar.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author John Levon
 * \author Jean-Marc Lasgouttes
 * \author Angus Leeming
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUITOOLBAR_H
#define GUITOOLBAR_H

#include <QList>
#include <QToolBar>
#include <QToolButton>

class QSettings;

namespace lyx {

namespace frontend {

class Action;
class GuiCommandBuffer;
class GuiToolbar;
class GuiView;
class ToolbarInfo;
class ToolbarItem;

class MenuButtonBase : public QToolButton
{
	Q_OBJECT
public:
	///
	MenuButtonBase(GuiToolbar * bar, ToolbarItem const & item);

protected:
	///
	virtual void initialize() = 0;
	///
	GuiToolbar * bar_;
	///
	ToolbarItem const & tbitem_;

protected Q_SLOTS:
	///
	void actionTriggered(QAction * action);
	///
	virtual void updateTriggered() = 0;
};


class StaticMenuButton : public MenuButtonBase
{
	Q_OBJECT
public:
	///
	StaticMenuButton(GuiToolbar * bar, ToolbarItem const & item,
		bool const sticky = false);

protected:
	///
	void initialize() override;

protected Q_SLOTS:
	///
	void updateTriggered() override;
};


/// A menu which can be populated on the fly.
/// The 'type' of menu must be given in the toolbar file
/// (stdtoolbars.inc, usually) and must be one of:
///		dynamic-custom-insets
///		dynamic-char-styles
///		textstyle-apply
///		paste
/// To add a new one of these, you must add a routine, like 
/// loadFlexInsets, that will populate the menu, and call it from
/// updateTriggered. Make sure to add the new type to isMenuType().
class DynamicMenuButton : public MenuButtonBase
{
    Q_OBJECT
public:
	///
	DynamicMenuButton(GuiToolbar * bar, ToolbarItem const & item);
	///
	~DynamicMenuButton();
	///
	static bool isMenuType(std::string const & s);
	///
	static void resetIconCache();
protected:
	///
	void initialize() override;
	///
	void loadFlexInsets();
	/// pimpl so we don't have to include big files
	class Private;
	Private * d;
protected Q_SLOTS:
	///
	void updateTriggered() override;
private:
	/// These icons are needed at each updateTriggered() call, and
	/// therefore we have to cache them.
	static QIcon icon_textstyle_apply_;
	static QIcon icon_undo_;
	static QIcon icon_paste_;
};


class GuiToolbar : public QToolBar
{
	Q_OBJECT
public:
	///
	GuiToolbar(ToolbarInfo const &, GuiView &);

	/// Reimplemented from QToolbar to detect whether the
	/// toolbar is restored with MainWindow::restoreState().
	void setVisible(bool visible) override;

	///
	void setVisibility(int visibility);

	/// Add a button to the bar.
	void add(ToolbarItem const & item);

	/// Session key.
	/**
	 * This key must be used for any session setting.
	 **/
	QString sessionKey() const;
	/// Save session settings.
	void saveSession(QSettings & settings) const;
	/// Restore session settings.
	void restoreSession();

	///
	bool isRestored() const;

	///
	bool isVisibilityOn() const;
	int visibility() const { return visibility_; }

	/// Refresh the contents of the bar.
	void update(int context = 0);

	///
	void setState(std::string const & state);
	///
	void toggle();

	///
	void refill();

	/// toggles movability
	void movable(bool silent = false);

	///
	GuiCommandBuffer * commandBuffer() { return command_buffer_; }

	/// add item to toolbar.
	/// \param menu : when true, the item is for a menu entry, not a button.
	Action * addItem(ToolbarItem const & item, bool menu = false);
	///
	GuiView const & owner() { return owner_; }

Q_SIGNALS:
	///
	void updated();

private Q_SLOTS:
	void showContextMenu(QPoint pos);

private:
	// load flags with saved values
	void initFlags();
	///
	void fill();
	///
	void showEvent(QShowEvent *) override;

	///
	QList<Action *> actions_;
	/// initial visibility flags
	int visibility_;
	///
	GuiView & owner_;
	///
	GuiCommandBuffer * command_buffer_;
	///
	ToolbarInfo const & tbinfo_;
	///
	bool filled_;
	///
	bool restored_;
};

} // namespace frontend
} // namespace lyx

#endif // GUITOOLBAR_H
