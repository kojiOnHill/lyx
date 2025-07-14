// -*- C++ -*-
/**
 * \file TextMetrics.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author John Levon
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef TEXT_METRICS_H
#define TEXT_METRICS_H

#include "Font.h"
#include "LayoutEnums.h"
#include "ParagraphMetrics.h"

#include "support/types.h"

#include <map>

namespace lyx {

class BufferView;
class Cursor;
class CursorSlice;
class MetricsInfo;
class Text;

/// A map from a Text to the map of paragraphs metrics
class TextMetrics
{
	/// noncopyable
	TextMetrics(TextMetrics const &);
	void operator=(TextMetrics const &);
public:
	/// Default constructor (only here for STL containers).
	TextMetrics() {}
	/// The only useful constructor.
	TextMetrics(BufferView *, Text const *);

	/// A map from paragraph index number to paragraph metrics
	typedef std::map<pit_type, ParagraphMetrics> ParMetricsCache;

	///
	ParMetricsCache::const_iterator begin() const { return par_metrics_.begin(); }
	///
	ParMetricsCache::const_iterator end() const { return par_metrics_.end(); }

	///
	bool contains(pit_type pit) const;
	///
	void forget(pit_type pit);
	///
	std::pair<pit_type, ParagraphMetrics const *> first() const;
	///
	std::pair<pit_type, ParagraphMetrics const *> last() const;
	/// is this row the last in the text?
	bool isLastRow(Row const & row) const;
	/// is this row the first in the text?
	bool isFirstRow(Row const & row) const;
	///
	void setRowChanged(pit_type pit, pos_type pos);

	/// Dimension of the entire text.
	Dimension const & dim() const { return dim_; }
	/// Dimension of paragraph \c pit if it exists, empty dimension
	/// otherwise.
	Dimension const & dim(pit_type pit) const;
	///
	Point const & origin() const { return origin_; }

	/// Metrics of paragraph \c pit. If no metrics exist or if they
	/// are empty, recompute metrics for this paragraph.
	ParagraphMetrics & parMetrics(pit_type pit);
	/// Identical to the non-const version
	/// FIXME: a const method should not modify the object!
	ParagraphMetrics const & parMetrics(pit_type pit) const;

	///
	void newParMetricsDown();
	///
	void newParMetricsUp();

	/// Update metrics up to \c bv_height. Only usable for main text (for now).
	void updateMetrics(pit_type const anchor_pit, int const anchor_ypos,
                       int const bv_height);

	/// compute text metrics.
	bool metrics(MetricsInfo const & mi, Dimension & dim, int min_width = 0);

	/// The "nodraw" drawing stage for one single paragraph: set the
	/// positions of the insets contained in this paragraph in metrics
	/// cache. Related to BufferView::updatePosCache.
	void updatePosCache(pit_type pit) const;

	/// Gets the fully instantiated font at a given position in a paragraph.
	/// Basically the same routine as Paragraph::getFont() in Paragraph.cpp.
	/// The difference is that this one is used for displaying, and thus we
	/// are allowed to make cosmetic improvements. For instance make footnotes
	/// smaller. (Asger)
	Font displayFont(pit_type pit, pos_type pos) const;

	/// Gets the fully instantiated label font of a paragraph.
	/// Basically the same routine as displayFont, but specialized for
	/// a layout font.
	Font labelDisplayFont(pit_type pit) const;


	/// There are currently two font mechanisms in LyX:
	/// 1. The font attributes in a lyxtext, and
	/// 2. The inset-specific font properties, defined in an inset's
	/// metrics() and draw() methods and handed down the inset chain through
	/// the pi/mi parameters, and stored locally in a lyxtext in font_.
	/// This is where the two are integrated in the final fully realized
	/// font.
	void applyOuterFont(Font &) const;

	/// is this position in the paragraph right-to-left?
	bool isRTL(CursorSlice const & sl, bool boundary) const;
	/// is between pos-1 and pos an RTL<->LTR boundary?
	bool isRTLBoundary(pit_type pit, pos_type pos) const;
	/// would be a RTL<->LTR boundary between pos and the given font?
	bool isRTLBoundary(pit_type pit, pos_type pos, Font const & font) const;


	/// Rebreaks the given paragraph.
	/// \retval true if a full screen redraw is needed.
	/// \retval false if a single paragraph redraw is enough.
	bool redoParagraph(pit_type const pit, bool align_rows = true);
	/// Clear cache of paragraph metrics
	void clear() { par_metrics_.clear(); }
	/// Is cache of paragraph metrics empty ?
	bool empty() const { return par_metrics_.empty(); }

	///
	int ascent() const { return dim_.asc; }
	///
	int descent() const { return dim_.des; }
	/// current text width.
	int width() const { return dim_.wid; }
	/// current text height.
	int height() const { return dim_.height(); }

	/**
	 * Returns the left beginning of a row starting at \c pos.
	 * This information cannot be taken from the layout object, because
	 * in LaTeX the beginning of the text fits in some cases
	 * (for example sections) exactly the label-width.
	 * When \c ignore_contents is true, alignment properties related
	 * to insets in paragraph are not taken into account.
	 */
	int leftMargin(pit_type pit, pos_type pos, bool ignore_contents  = false) const;
	/// Return the left beginning of a row which is not the first one.
	/// This is the left margin when there is no indentation.
	int leftMargin(pit_type pit) const;

	///
	int rightMargin(ParagraphMetrics const & pm) const;
	int rightMargin(pit_type const pit) const;

	int maxWidth() { return max_width_; }
	///
	void draw(PainterInfo & pi, int x, int y) const;

	void drawParagraph(PainterInfo & pi, pit_type pit, int x, int y) const;

private:

	/// the minimum space a manual label needs on the screen in pixels
	int labelFill(Row const & row) const;

	// Turn paragraph oh index \c pit into a single row
	Row tokenizeParagraph(pit_type pit) const;

	// Compute metrics of inset element
	// Returns the extra width that can be necessary for things like InsetMathHull
	int redoInset(Row::Element & elt, DocIterator & parPos, int w) const;

	// Break the row produced by tokenizeParagraph() into a list of rows.
	Rows breakParagraph(Row const & row) const;

	// Expands the alignment of row \param row in paragraph \param par
	LyXAlignment getAlign(Paragraph const & par, Row const & row) const;
	/// Aligns properly the row contents (computes spaces and fills)
	void setRowAlignment(Row & row, int width) const;

	/// Set the height of the row (without space above/below paragraph)
	void setRowHeight(Row & row) const;
	// Compute the space on top of a paragraph
	int parTopSpacing(pit_type pit) const;
	// Compute the space below a a paragraph
	int parBottomSpacing(pit_type pit) const;

	// Helper function for the other checkInsetHit method.
	Row::Element const * checkInsetHit(Row const & row, int x) const;

	/// Returns the paragraph number closest to screen y-coordinate.
	/// This method uses the paragraph metrics to locate the
	/// paragraph. The y-coordinate is allowed to be off-screen and
	/// the metrics will be automatically updated if needed. This is
	/// the reason why we need a non const BufferView.
	/// FIXME: check whether this is still needed
	pit_type getPitNearY(int y);

	/// returns the row near the specified y-coordinate in a given paragraph
	/// (relative to the screen).
	Row const * getRowNearY(int & y);

public:
	/// returns the position near the specified x-coordinate of the row.
	/// x is an absolute screen coord, it is set to the real beginning
	/// of this column. This takes in account horizontal cursor row scrolling.
	std::pair<pos_type, bool> getPosNearX(Row const & row, int & x) const;

	/** sets cursor recursively descending into nested editable insets
	\return the inset pointer if x,y is covering that inset
	\param x,y are absolute screen coordinates.
	\retval inset is null if the cursor is positioned over normal
	       text in the current Text object. Otherwise it is the inset
	       that the cursor points to, like for Inset::editXY.
	*/
	Inset * editXY(Cursor & cur, int x, int y);

	/// sets cursor only within this Text.
	/// x,y are screen coordinates
	void setCursorFromCoordinates(Cursor & cur, int x, int y);

	///
	int cursorX(CursorSlice const & sl, bool boundary) const;
	///
	int cursorY(CursorSlice const & sl, bool boundary) const;

	///
	bool cursorHome(Cursor & cur);
	///
	bool cursorEnd(Cursor & cur);
	///
	void deleteLineForward(Cursor & cur);

	/// Returns an inset if inset was hit, or 0 if not.
	/// \warning This method is not recursive! It will return the
	/// outermost inset within this Text.
	/// \sa BufferView::getCoveringInset() to get the innermost inset.
	Inset * checkInsetHit(int x, int y);

	/// calculates the position of a completion popup
	void completionPosAndDim(Cursor const & cur, int & x, int & y,
		Dimension & dim) const;

private:

	/// The BufferView owner.
	BufferView * bv_ = nullptr;

	/// The text contents (the model).
	Text const * text_ = nullptr;

	/// The input method instance
	frontend::InputMethod * im_ = nullptr;

	/// FIXME: This can be changed even when TextMetrics is const
	mutable ParMetricsCache par_metrics_;
	Dimension dim_;
	int max_width_ = 0;
	/// if true, do not expand insets to max width artificially
	bool tight_ = false;
	mutable Point origin_;

// temporary public:
public:
	/// our 'outermost' font.
	/// This is handed down from the surrounding
	/// inset through the pi/mi parameter (pi.base.font)
	/// It is used in applyOuterFont() and setCharFont() for reasons
	/// that are not clear... to hand hand the outermost language and
	/// also for char style apparently.
	Font font_;
};

/// return the default height of a row in pixels, considering font zoom
int defaultRowHeight();

} // namespace lyx

#endif // TEXT_METRICS_H
