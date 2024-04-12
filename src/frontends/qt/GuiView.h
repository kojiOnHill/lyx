// -*- C++ -*-
/**
 * \file GuiView.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjornes
 * \author John Levon
 * \author Abdelrazak Younes
 * \author Peter Kümmel
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUI_VIEW_H
#define GUI_VIEW_H

#include "frontends/Delegates.h"

#include "support/strfwd.h"

#include <QMainWindow>
#include <QLabel>
#include <QMenu>
#include <QSvgWidget>

class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QPushButton;
class QShowEvent;
class QSlider;


namespace lyx {

namespace support { class FileName; }

class Buffer;
class BufferView;
class Cursor;
class DispatchResult;
class FuncStatus;
class FuncRequest;
class Inset;

namespace frontend {

class Dialog;
class LayoutBox;
class GuiClickableLabel;
class GuiToolbar;
class GuiWorkArea;
class TabWorkArea;
class TocModels;
class ToolbarInfo;

/**
 * GuiView - Qt main LyX window
 *
 * This class represents the main LyX window and provides
 * accessor functions to its content.
 *
 * Note: a QObject emits a destroyed(QObject *) Qt signal when it
 * is deleted. This might be useful for closing other dialogs
 * depending on a given GuiView.
 */
class GuiView : public QMainWindow, public GuiBufferViewDelegate,
	public GuiBufferDelegate
{
	Q_OBJECT

public:
	/// create a main window of the given dimensions
	GuiView(int id);

	~GuiView();

	/// closes the view such that the view knows that is closed
	/// programmatically and not by the user clicking the x.
	bool closeScheduled();

	/// Things that need to be done when the OSes session manager
	/// requests a log out.
	bool prepareAllBuffersForLogout();

	int id() const { return id_; }

	/// are we busy ?
	bool busy() const;

	/// Signal that the any "auto" minibuffer can be closed now.
	void resetCommandExecute();

	/// \name Generic accessor functions
	//@{
	/// The current BufferView refers to the BufferView that has the focus,
	/// including for example the one that is created when you use the
	/// advanced search and replace pane.
	/// \return the currently selected buffer view.
	BufferView * currentBufferView();
	BufferView const * currentBufferView() const;

	/// The document BufferView always refers to the view's main document
	/// BufferView. So, even if the BufferView in e.g., the advanced
	/// search and replace pane has the focus.
	/// \return the current document buffer view.
	BufferView * documentBufferView();
	BufferView const * documentBufferView() const;

	void newDocument(std::string const & filename,
			 std::string templatefile = std::string(),
			 bool fromTemplate = false);

	/// display a message in the view
	/// could be called from any thread
	void message(docstring const &) override;

	bool getStatus(FuncRequest const & cmd, FuncStatus & flag);
	/// dispatch command.
	/// \return true if the \c FuncRequest has been dispatched.
	void dispatch(FuncRequest const & cmd, DispatchResult & dr);

	void restartCaret();
	/// Update the completion popup and the inline completion state.
	/// If \c start is true, then a new completion might be started.
	/// If \c keep is true, an active completion will be kept active
	/// even though the cursor moved. The update flags of \c cur might
	/// be changed.
	void updateCompletion(Cursor & cur, bool start, bool keep);

	///
	void setFocus();
	void setFocus(Qt::FocusReason reason);
	bool hasFocus() const;

	///
	void focusInEvent(QFocusEvent * e) override;
	/// Add a Buffer to the View
	/// \param b Buffer to set.
	/// \param switch_to Whether to set it to the current workarea.
	void setBuffer(Buffer * b, bool switch_to = true);

	/// load a document into the current workarea.
	Buffer * loadDocument(
		support::FileName const &  name, ///< File to load.
		bool tolastfiles = true  ///< append to the "Open recent" menu?
		);

	/// add toolbar, if newline==true, add a toolbar break before the toolbar
	GuiToolbar * makeToolbar(ToolbarInfo const & tbinfo, bool newline);
	void updateStatusBar();

	/// updates the possible layouts selectable
	void updateLayoutList();
	void updateToolbars();

	///
	LayoutBox * getLayoutDialog() const;

	/// hides the workarea and makes sure it is clean
	bool hideWorkArea(GuiWorkArea * wa);
	/// closes workarea; close buffer only if no other workareas point to it
	bool closeWorkArea(GuiWorkArea * wa);
	/// closes the buffer
	bool closeBuffer(Buffer & buf);
	///
	void openDocuments(std::string const & filename, int origin);
	///
	void importDocument(std::string const &);

	/// \name GuiBufferDelegate.
	//@{
	void resetAutosaveTimers() override;
	// shows an error list
	// if from_master is true, show master's error list
	void errors(std::string const &, bool from_master = false) override;
	void structureChanged() override;
	void updateTocItem(std::string const &, DocIterator const &) override;
	//@}

	///
	TocModels & tocModels();

	/// called on timeout
	void autoSave();

	/// check for external change of any opened buffer, mainly for svn usage
	void checkExternallyModifiedBuffers();

	/** redraw \c inset in all the BufferViews in which it is currently
	 *  visible. If successful return a pointer to the owning Buffer.
	 */
	Buffer const * updateInset(Inset const *);

	/// \return the \c Workarea associated to \p  Buffer
	/// \retval 0 if no \c WorkArea is found.
	GuiWorkArea * workArea(Buffer & buffer);
	/// \return the \c Workarea at index \c index
	GuiWorkArea * workArea(int index);

	/// Add a \c WorkArea
	/// \return the \c Workarea associated to \p  Buffer
	/// \retval 0 if no \c WorkArea is found.
	GuiWorkArea * addWorkArea(Buffer & buffer);
	/// \param work_area The current \c WorkArea, or \c NULL
	void setCurrentWorkArea(GuiWorkArea * work_area);
	///
	void removeWorkArea(GuiWorkArea * work_area);
	/// return true if \c wa is one of the visibles workareas of this view
	bool hasVisibleWorkArea(GuiWorkArea * wa) const;
	/// return the current WorkArea (the one that has the focus).
	GuiWorkArea const * currentWorkArea() const;
	/// return the current WorkArea (the one that has the focus).
	GuiWorkArea * currentWorkArea();

	/// return the current document WorkArea (it may not have the focus).
	GuiWorkArea const * currentMainWorkArea() const;
	/// return the current document WorkArea (it may not have the focus).
	GuiWorkArea * currentMainWorkArea();

	/// Current ratio between physical pixels and device-independent pixels
	double pixelRatio() const;

Q_SIGNALS:
	void closing(int);
	void triggerShowDialog(QString const & qname, QString const & qdata, Inset * inset);
	// emitted when the work area or its buffer view changed
	void bufferViewChanged();
	/// emitted when zoom is modified
	void currentZoomChanged(int);
	/// emitted when script is killed (e.g., user cancels export)
	void scriptKilled();
	/// emitted when track changes status toggled
	void changeTrackingToggled(bool);
	///
	void dockWidgetVisibilityChanged();

public Q_SLOTS:
	///
	void setBusy(bool) override;
	/// idle timeout.
	/// clear any temporary message and replace with current status.
	void clearMessage();
	/// show documents stats in toolbar and trigger new iteration
	void showStats();
	///
	void updateWindowTitle(GuiWorkArea * wa);
	///
	void disableShellEscape();
	///
	void onDockWidgetVisibilityChanged() { Q_EMIT dockWidgetVisibilityChanged(); }

private Q_SLOTS:
	///
	void resetWindowTitle();

	void flatGroupBoxes(const QObject * object, bool flag);

	///
	void checkCancelBackground();
	///
	void statsPressed();
	///
	void zoomSliderMoved(int);
	///
	void zoomValueChanged(int);
	///
	void zoomInPressed();
	///
	void zoomOutPressed();
	///
	void showZoomContextMenu();
	///
	void showStatusBarContextMenu();
	///
	void on_currentWorkAreaChanged(GuiWorkArea *);
	///
	void onBufferViewChanged();
	///
	void on_lastWorkAreaRemoved();

	/// For completion of autosave or export threads.
	void processingThreadStarted();
	void processingThreadFinished();
	void autoSaveThreadFinished();

	/// must be called in GUI thread
	void doShowDialog(QString const & qname, QString const & qdata,
	Inset * inset);

	/// must be called from GUI thread
	void updateStatusBarMessage(QString const & str);
	void clearMessageText();

private:
	/// Open given child document in current buffer directory.
	void openChildDocument(std::string const & filename);
	/// Close current document buffer.
	bool closeBuffer();
	/// Close all document buffers.
	bool closeBufferAll();
	///
	TabWorkArea * addTabWorkArea();

	///
	void scheduleRedrawWorkAreas();

	/// connect to signals in the given BufferView
	void connectBufferView(BufferView & bv);
	/// disconnect from signals in the given BufferView
	void disconnectBufferView();
	/// connect to signals in the given buffer
	void connectBuffer(Buffer & buf);
	/// disconnect from signals in the given buffer
	void disconnectBuffer();
	///
	void dragEnterEvent(QDragEnterEvent * ev) override;
	///
	void dropEvent(QDropEvent * ev) override;
	/// make sure we quit cleanly
	void closeEvent(QCloseEvent * e) override;
	///
	void showEvent(QShowEvent *) override;

	/// in order to catch Tab key press.
	bool event(QEvent * e) override;
	bool focusNextPrevChild(bool) override;

	///
	bool goToFileRow(std::string const & argument);

	///
	class GuiViewPrivate;
	GuiViewPrivate & d;

public:
	///
	/// dialogs for this view
	///

	///
	void resetDialogs();

	/// Hide all visible dialogs
	void hideAll() const;

	/// Update all visible dialogs.
	/**
	 *  Check the status of all visible dialogs and disable or re-enable
	 *  them as appropriate.
	 *
	 *  Disabling is needed for example when a dialog is open and the
	 *  cursor moves to a position where the corresponding inset is not
	 *  allowed.
	 */
	void updateDialogs();

	/** Show dialog could be called from arbitrary threads.
	    \param name == "bibtex", "citation" etc; an identifier used to
	    launch a particular dialog.
	    \param data is a string representation of the Inset contents.
	    It is often little more than the output from Inset::write.
	    It is passed to, and parsed by, the frontend dialog.
	    Several of these dialogs do not need any data,
	    so it defaults to string().
	    \param inset ownership is _not_ passed to the frontend dialog.
	    It is stored internally and used by the kernel to ascertain
	    what to do with the FuncRequest dispatched from the frontend
	    dialog on 'Apply'; should it be used to create a new inset at
	    the current cursor position or modify an existing, 'open' inset?
	*/
	void showDialog(std::string const & name,
		std::string const & data, Inset * inset = 0) override;

	/** \param name == "citation", "bibtex" etc; an identifier used
	    to reset the contents of a particular dialog with \param data.
	    See the comments to 'show', above.
	*/
	void updateDialog(std::string const & name, std::string const & data) override;

	/** All Dialogs of the given \param name will be closed if they are
	    connected to the given \param inset.
	*/
	void hideDialog(std::string const & name, Inset * inset);
	///
	void disconnectDialog(std::string const & name);
	///
	bool develMode() const { return devel_mode_; }
	///
	void setCurrentZoom(int const v);

private:
	/// Saves the layout and geometry of the window
	void saveLayout() const;
	/// Saves the settings of toolbars and all dialogs
	void saveUISettings() const;
	///
	bool restoreLayout();
	///
	GuiToolbar * toolbar(std::string const & name);
	///
	void constructToolbars();
	///
	void initToolbars();
	///
	void initToolbar(std::string const & name);
	/// Update lock (all) toolbars position
	void updateLockToolbars();
	/// refill the toolbars (dark/light mode switch)
	void refillToolbars();
	///
	bool lfunUiToggle(std::string const & ui_component);
	///
	/// kill the script and hide export-in-progress status bar icons
	void cancelExport();
	///
	void toggleFullScreen();
	/// \return whether we did anything
	bool insertLyXFile(docstring const & fname, bool ignorelang = false);
	///
	/// Open Export As ... dialog. \p iformat is the format the
	/// filter is initially set to.
	bool exportBufferAs(Buffer & b, docstring const & iformat);

	///
	enum RenameKind {
		LV_WRITE_AS,
		LV_WRITE_AS_TEMPLATE,
		LV_VC_RENAME,
		LV_VC_COPY,
	};
	/// Get a path for LFUN_BUFFER_WRITE_AS_TEMPLATE
	std::string const getTemplatesPath(Buffer & buf);
	/// Save a buffer as a new file.
	/**
	 * Write a buffer to a new file name and rename the buffer
	 * according to the new file name.
	 *
	 * This function is e.g. used by menu callbacks and
	 * LFUN_BUFFER_WRITE_AS.
	 *
	 * If 'newname' is empty, the user is asked via a
	 * dialog for the buffer's new name and location.
	 *
	 * If 'newname' is non-empty and has an absolute path, that is used.
	 * Otherwise the base directory of the buffer is used as the base
	 * for any relative path in 'newname'.
	 *
	 * \p kind controls what is done besides the pure renaming:
	 * LV_WRITE_AS  => The buffer is written without version control actions.
	 * LV_VC_RENAME => The file is renamed in version control.
	 * LV_VC_COPY   => The file is copied in version control.
	 */
	bool renameBuffer(Buffer & b, docstring const & newname,
	                  RenameKind kind = LV_WRITE_AS);
	///
	bool saveBuffer(Buffer & b);
	/// save and rename buffer to fn. If fn is empty, the buffer
	/// is just saved as the filename it already has.
	bool saveBuffer(Buffer & b, support::FileName const & fn);
	/// closes a workarea, if close_buffer is true the buffer will
	/// also be released, otherwise the buffer will be hidden.
	bool closeWorkArea(GuiWorkArea * wa, bool close_buffer);
	/// closes the tabworkarea and all tabs. If we are in a close event,
	/// all buffers will be closed, otherwise they will be hidden.
	bool closeTabWorkArea(TabWorkArea * twa);
	/// gives the user the possibility to save their work
	/// or to discard the changes. If hiding is true, the
	/// document will be reloaded.
	bool saveBufferIfNeeded(Buffer & buf, bool hiding);
	/// closes all workareas
	bool closeWorkAreaAll();
	/// write all open workareas into the session file
	void writeSession() const;
	/// is the buffer in this workarea also shown in another tab ?
	/// This tab can either be in the same view or in another one.
	bool inMultiTabs(GuiWorkArea * wa);
	/// is the buffer shown in some other view ?
	bool inOtherView(Buffer & buf);
	///
	enum NextOrPrevious {
		NEXT,
		PREV
	};
	///
	void gotoNextOrPreviousBuffer(NextOrPrevious np, bool const move);
	///
	void gotoNextTabWorkArea(NextOrPrevious np);

	/// Is the dialog currently visible?
	bool isDialogVisible(std::string const & name) const;
	///
	Dialog * find(std::string const & name, bool hide_it) const;
	///
	Dialog * findOrBuild(std::string const & name, bool hide_it);
	///
	Dialog * build(std::string const & name);
	///
	bool reloadBuffer(Buffer & buffer);
	///
	void dispatchVC(FuncRequest const & cmd, DispatchResult & dr);
	///
	void dispatchToBufferView(FuncRequest const & cmd, DispatchResult & dr);
	///
	void showMessage();
	/// Check whether any of the stats is enabled in status bar
	bool statsEnabled() const;

	/// This view ID.
	int id_;

	/// flag to avoid two concurrent close events.
	bool closing_;
	/// if the view is busy the cursor shouldn't blink for instance.
	/// This counts the number of times more often we called
	/// setBusy(true) compared to setBusy(false), so we can nest
	/// functions that call setBusy;
	int busy_;

	/// Request to open the command toolbar if it is "auto"
	bool command_execute_;
	/// Request to give focus to minibuffer
	bool minibuffer_focus_;

	/// Statusbar widget that shows shell-escape status
	QLabel * shell_escape_;
	/// Statusbar widget that shows read-only status
	QLabel * read_only_;
	/// Statusbar widget that shows version control status
	QLabel * version_control_;
	/// Statusbar widget that document count statistics
	QLabel * stat_counts_;
	/// Word count info feature can be disabled by context menu
	bool word_count_enabled_;
	/// Char count info feature can be disabled by context menu
	bool char_count_enabled_;
	/// Char count info feature can be disabled by context menu
	/// This excludes blanks
	bool char_nb_count_enabled_;
	/// Statusbar widget that shows zoom value
	GuiClickableLabel * zoom_value_;
	/// The zoom widget
	QWidget * zoom_widget_;
	/// The zoom slider widget
	QSlider * zoom_slider_;
	/// Zoom in ("+") Button
	GuiClickableLabel * zoom_in_;
	/// Zoom out ("-") Button
	GuiClickableLabel * zoom_out_;

	/// The rate from which the actual zoom value is calculated
	/// from the default zoom pref
	double zoom_ratio_ = 1.0;
	/// Minimum zoom percentage
	int const zoom_min_ = 10;
	/// Maximum zoom percentage
	int const zoom_max_ = 1000;

	// movability flag of all toolbars
	bool toolbarsMovable_;

	// developer mode
	bool devel_mode_;

	// initial zoom for pinch gesture
	int initialZoom_;
};


class SEMenu : public QMenu
{
	Q_OBJECT
public:
	explicit SEMenu(QWidget * parent);

public Q_SLOTS:
	void showMenu(QPoint const &) { exec(QCursor::pos()); }
};


class PressableSvgWidget : public QSvgWidget
{
	Q_OBJECT
public:
    explicit PressableSvgWidget(const QString &file, QWidget * parent = nullptr)
	: QSvgWidget(file, parent) {};
protected:
    void mousePressEvent(QMouseEvent *event) override;
Q_SIGNALS:
    void pressed();
};

} // namespace frontend
} // namespace lyx

#endif // GUIVIEW_H
