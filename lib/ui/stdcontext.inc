# -*- text -*-

# file stdcontext.inc
# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.

# author The LyX Team

# Full author contact details are available in file CREDITS.

# The interface is designed (partially) following the KDE Human Interface
# Guidelines (http://techbase.kde.org/Projects/Usability/HIG)

# Casing Rules:
# Capitalize all words in the element, with the following exceptions: 
# * Articles: a, an, the.
# * Conjunctions: and, but, for, not, so, yet ...
# * Prepositions of three or fewer letters: at, for, by, in, to ...
#   (except when the preposition is part of a verb phrase, such as "Check In")
# (http://library.gnome.org/devel/hig-book/stable/design-text-labels.html.en)

Format 5

Menuset

#
# InsetMath context menu
#
	Menu "insert_math_context"
		Item "Array Environment|y" "math-matrix 2 2"
		Item "Cases Environment|C" "math-insert \cases"
		Item "Aligned Environment|l" "command-sequence math-insert \aligned; tabular-feature append-column"
		Item "AlignedAt Environment|v" "command-sequence math-insert \alignedat; tabular-feature append-column"
		Item "Gathered Environment|h" "math-insert \gathered"
		Item "Split Environment|S" "command-sequence math-insert \split; tabular-feature append-column"
		Separator
		Item "Delimiters...|r" "dialog-show mathdelimiter"
		Item "Matrix...|x" "dialog-show mathmatrix"
		Item "Macro|o" "math-macro newmacroname newcommand"
	End

	Menu "ams_environment"
		Item "AMS align Environment|a" "math-mutate align"
		Item "AMS alignat Environment|t" "math-mutate alignat"
		Item "AMS flalign Environment|f" "math-mutate flalign"
		Item "AMS gather Environment|g" "math-mutate gather"
		Item "AMS multline Environment|m" "math-mutate multline"
	End

	Menu "context-math"
		Item "Inline Formula|I" "math-mutate simple"
		Item "Displayed Formula|D" "math-mutate equation"
		Item "Eqnarray Environment|E" "math-mutate eqnarray"
		Submenu "AMS Environment|A" "ams_environment"
		Separator
		OptItem "Number Whole Formula|N" "math-number-toggle"
		OptItem "Number This Line|u" "math-number-line-toggle"
		OptItem "Equation Label|L" "label-insert"
		OptItem "Copy as Reference|R" "label-copy-as-reference"
		Separator
		OptItem "Cut" "cut"
		OptItem "Copy" "copy"
		OptItem "Paste" "paste"
		OptSubmenu "Paste Recent|e" "edit_pasterecent"
		Separator
		Submenu "Insert|s" "insert_math_context"
		Separator
		Item "Split Cell|C" "cell-split"
		OptSubmenu "Rows & Columns| " "edit_tabular_features"
		OptItem "Add Line Above|o" "tabular-feature add-hline-above"
		OptItem "Add Line Below|B" "tabular-feature add-hline-below"
		OptItem "Delete Line Above|v" "tabular-feature delete-hline-above"
		OptItem "Delete Line Below|w" "tabular-feature delete-hline-below"
		Separator
		OptItem "Add Line to Left" "tabular-feature add-vline-left"
		OptItem "Add Line to Right" "tabular-feature add-vline-right"
		OptItem "Delete Line to Left" "tabular-feature delete-vline-left"
		OptItem "Delete Line to Right" "tabular-feature delete-vline-right"
		Separator
		Item "Show Math Toolbar" "toolbar-toggle math toggle"
		Item "Show Math-Panels Toolbar" "toolbar-toggle math_panels toggle"
		Item "Show Table Toolbar" "toolbar-toggle table toggle"
		Separator
		Submenu "Use Computer Algebra System|m" "edit_math_extern"
	End


#
# InsetMathRef context menu
#
	Menu "context-mathref"
		Item "Next Cross-Reference or Label with the Same Target|N" "reference-next"
		Item "Next Cross-Reference|C" "reference-next anyref"
		Item "Go to Label|G" "label-goto"
		Separator
		Item "<Reference>|R" "inset-modify changetype ref"
		Item "(<Reference>)|e" "inset-modify changetype eqref"
		Item "<Page>|P" "inset-modify changetype pageref"
		OptItem "p. <Page>|." "inset-modify changetype cpageref"
		Item "On Page <Page>|O" "inset-modify changetype vpageref"
		Item "<Reference> on Page <Page>|f" "inset-modify changetype vref"
		Item "Formatted Reference|t" "inset-modify changetype formatted"
		Item "Textual Reference|x" "inset-modify changetype nameref"
		Item "Label Only|L" "inset-modify changetype labelonly"
		Separator
		Item "Settings...|S" "inset-settings"
	End


#
# InsetRef context menu
#
	Menu "context-ref"
		Item "Next Cross-Reference or Label with the Same Target|N" "reference-next"
		Item "Next Cross-Reference|C" "reference-next anyref"
		Item "Go to Label|G" "label-goto"
		Separator
		Item "<Reference>|R" "inset-modify changetype ref"
		Item "(<Reference>)|e" "inset-modify changetype eqref"
		Item "<Page>|P" "inset-modify changetype pageref"
		OptItem "p. <Page>|." "inset-modify changetype cpageref"
		Item "On Page <Page>|O" "inset-modify changetype vpageref"
		Item "<Reference> on Page <Page>|f" "inset-modify changetype vref"
		Item "Formatted Reference|t" "inset-modify changetype formatted"
		Item "Textual Reference|x" "inset-modify changetype nameref"
		Item "Label Only|L" "inset-modify changetype labelonly"
		Separator
		OptItem "Plural|a" "inset-modify ref toggle-plural"
		OptItem "Capitalize|C" "inset-modify ref toggle-caps"
		OptItem "No Prefix" "inset-modify ref toggle-noprefix"
		OptItem "No Hyperlink" "inset-modify ref toggle-nolink"
		Separator
		Item "Settings...|S" "inset-settings"
	End

#
# InsetLabel context menu
#
	Menu "context-label"
		Item "Next Cross-Reference that Points to This Label|N" "reference-next"
		Item "Go Back|G" "bookmark-goto 0"
		Separator
		Item "Copy as Reference|C" "label-copy-as-reference"
		Separator
		Item "Settings...|S" "inset-settings"
	End


#
# InsetCitation context menu
#
	Menu "context-citation"
		CiteStyles
		Separator
		Item "Settings...|S" "inset-settings"
		OptItem "Try to Open Citation Content...|O" "inset-edit"
	End


#
# InsetBibtex context menu
#
	Menu "context-bibtex"
		Item "Settings...|S" "inset-settings"
		Separator
		Item "Edit Database(s) Externally...|x" "inset-edit"
	End

#
# InsetBibItem context menu
#
	Menu "context-bibitem"
		Item "Settings...|S" "inset-settings"
	End

#
# InsetCollapsible context menu
#
	Menu "context-collapsible"
		OptItem "Open Inset|O" "inset-toggle open"
		OptItem "Close Inset|C" "inset-toggle close"
		Separator
		Item "Dissolve Inset|D" "inset-dissolve"
		OptItem "Settings...|S" "inset-settings"
	End

	Menu "context-conglomerate"
		Item "Show Label|L" "inset-toggle"
		Item "Dissolve Inset|D" "inset-dissolve charstyle"
		Separator
		OptItem "Settings...|S" "inset-settings"
	End

#
# InsetArgument context menu
#

	Menu "context-argument"
		Separator
		SwitchArguments
	End

	Menu "context-argument-conglomerate"
		Item "Show Label|L" "inset-toggle"
		Item "Dissolve Inset|D" "inset-dissolve"
		Separator
		SwitchArguments
	End

#
# InsetBox context menu
#

	Menu "context-box"
		Item "Frameless|l" "inset-modify changetype Frameless"
		Item "Simple Frame|F" "inset-modify changetype Boxed"
		Item "Simple Frame, Page Breaks|P" "inset-modify changetype Framed"
		Item "Oval, Thin|a" "inset-modify changetype ovalbox"
		Item "Oval, Thick|v" "inset-modify changetype Ovalbox"
		Item "Drop Shadow|w" "inset-modify changetype Shadowbox"
		Item "Shaded Background|B" "inset-modify changetype Shaded"
		Item "Double Frame|u" "inset-modify changetype Doublebox"
	End


#
# InsetCaption context menu
#
	Menu "context-caption"
		SwitchCaptions
	End


#
# InsetNote context menu
#

	Menu "context-note"
		Item "LyX Note|N" "inset-modify note Note Note"
		Item "Comment|m" "inset-modify note Note Comment"
		Item "Greyed Out|y" "inset-modify note Note Greyedout"
		Separator
		Item "Open All Notes|A" "inset-forall Note inset-toggle open"
		Item "Close All Notes|l" "inset-forall Note inset-toggle close"
	End

#
# InsetPhantom context menu
#

	Menu "context-phantom"
		Item "Phantom|P" "inset-modify phantom Phantom Phantom"
		Item "Horizontal Phantom|H" "inset-modify phantom Phantom HPhantom"
		Item "Vertical Phantom|V" "inset-modify phantom Phantom VPhantom"
	End


#
# InsetQuotes context menu
#
	Menu "context-quote"
		SwitchQuotes
	End

#
# InsetSpace context menu
#
	Menu "context-space"
		Item "Normal Space|e" "inset-modify space \space{}"
		Item "Non-Breaking Normal Space|o" "inset-modify space ~"
		Item "Visible Space|a" "inset-modify space \textvisiblespace{}"
		Item "Non-Breaking Thin Space (1/6 em)|T" "inset-modify space \thinspace{}"
		Item "Non-Breaking Medium Space (3/9 em)|M" "inset-modify space \medspace{}"
		Item "Non-Breaking Thick Space (5/18 em)|i" "inset-modify space \thickspace{}"
		Item "Non-Breaking Negative Thin Space (-1/6 em)|N" "inset-modify space \negthinspace{}"
		Item "Non-Breaking Negative Medium Space (-2/9 em)|v" "inset-modify space \negmedspace{}"
		Item "Non-Breaking Negative Thick Space (-5/18 em)|h" "inset-modify space \negthickspace{}"
		Item "Half Quad Space (1/2 em)|l" "inset-modify space \enskip{}"
		Item "Non-Breaking Half Quad Space (1/2 em)|k" "inset-modify space \enspace{}"
		Item "Quad Space (1 em)|Q" "inset-modify space \quad{}"
		Item "Double Quad Space (2 em)|u" "inset-modify space \qquad{}"
		Item "Horizontal Fill|F" "inset-modify space \hfill{}"
		Item "Non-Breaking Horizontal Fill|z" "inset-modify space \hspace*{\fill}"
		Item "Horizontal Fill (Dots)|D" "inset-modify space \dotfill{}"
		Item "Horizontal Fill (Rule)|R" "inset-modify space \hrulefill{}"
		Item "Horizontal Fill (Left Arrow)|w" "inset-modify space \leftarrowfill{}"
		Item "Horizontal Fill (Right Arrow)|g" "inset-modify space \rightarrowfill{}"
		Item "Horizontal Fill (Up Brace)|p" "inset-modify space \upbracefill{}"
		Item "Horizontal Fill (Down Brace)|B" "inset-modify space \downbracefill{}"
		Item "Custom Length|C" "command-sequence inset-modify space \hspace{}; inset-settings"
		Separator
		Item "Settings...|S" "inset-settings"
	End

#
# InsetMathSpace context menu
#
	Menu "context-mathspace"
		Item "Thin Space|T" "inset-modify mathspace \thinspace{}"
		Item "Medium Space|M" "inset-modify mathspace \medspace{}"
		Item "Thick Space|i" "inset-modify mathspace \thickspace{}"
		Item "Negative Thin Space|N" "inset-modify mathspace \negthinspace{}"
		Item "Negative Medium Space|v" "inset-modify mathspace \negmedspace{}"
		Item "Negative Thick Space|h" "inset-modify mathspace \negthickspace{}"
		Item "Half Quad Space (Enskip)|k" "inset-modify mathspace \enskip{}"
		Item "Quad Space|Q" "inset-modify mathspace \quad{}"
		Item "Double Quad Space|u" "inset-modify mathspace \qquad{}"
		Item "Custom Length|C" "command-sequence inset-modify mathspace \hspace{} \length 1in; inset-settings"
		Separator
		Item "Settings...|S" "inset-settings"
	End

#
# InsetVSpace context menu
#
	Menu "context-vspace"
		Item "Default Skip|D" "inset-modify vspace defskip"
		Item "Small Skip|S" "inset-modify vspace smallskip"
		Item "Medium Skip|M" "inset-modify vspace medskip"
		Item "Big Skip|B" "inset-modify vspace bigskip"
		Item "Half line height|H" "inset-modify vspace halfline"
		Item "Line height|L" "inset-modify vspace fullline"
		Item "Vertical Fill|F" "inset-modify vspace vfill"
		Item "Custom|C" "command-sequence inset-modify vspace custom; inset-settings"
		Separator
		Item "Settings...|e" "inset-settings"
	End

#
# InsetInclude context menu
#
	Menu "context-include"
		Item "Include|c" "inset-modify changetype include"
		Item "Input|p" "inset-modify changetype input"
		Item "Verbatim|V" "inset-modify changetype verbatiminput"
		Item "Verbatim (marked blanks)|b" "inset-modify changetype verbatiminput*"
		Item "Listing|L" "inset-modify changetype lstinputlisting"
		Separator
		Item "Settings...|S" "inset-settings"
		Separator
		Item "Edit Included File...|E" "inset-edit"
	End

#
# InsetNewpage context menu
#
	Menu "context-newpage"
		Item "New Page|N" "inset-modify newpage newpage"
		Item "Page Break|a" "inset-modify newpage pagebreak"
		Item "No Page Break|g" "inset-modify newpage nopagebreak"
		Item "Clear Page|C" "inset-modify newpage clearpage"
		Item "Clear Double Page|D" "inset-modify newpage cleardoublepage"
	End

#
# InsetNewline context menu
#
	Menu "context-newline"
		Item "Ragged Line Break|R" "inset-modify newline newline"
		Item "Justified Line Break|J" "inset-modify newline linebreak"
	End

#
# InsetNewline context menu
#
	Menu "context-separator"
		Item "Plain Separator|P" "inset-modify separator plain"
		Item "Paragraph Break|B" "inset-modify separator parbreak"
	End

#
# Edit context menu
#
	Menu "context-edit"
		spellingsuggestions
		Separator
		OptItem "Edit Externally..." "inset-edit"
		OptItem "End Editing Externally" "inset-end-edit"
		Separator
		Item "Cut" "cut"
		Item "Copy" "copy"
		Item "Paste" "paste"
		Submenu "Paste Recent|e" "edit_pasterecent"
		Separator
		OptItem "Split Inset|i" "inset-split"
		Separator
		Item "Jump Back to Saved Bookmark|B" "bookmark-goto 0"
		OptItem "Forward Search|F" "forward-search"
		Separator
		Item "Move Paragraph Up|h" "paragraph-move-up"
		Item "Move Paragraph Down|v" "paragraph-move-down"
		Separator
		EnvironmentSeparatorsContext
		Separator
		OptItem "Promote Section|r" "outline-out"
		OptItem "Demote Section|m" "outline-in"
		OptItem "Move Section Down|D" "outline-down"
		OptItem "Move Section Up|U" "outline-up"
		Arguments
		OptSubmenu "Insert Regular Expression" "context-edit-regexp"
		Separator
		OptItem "Accept Change|c" "change-accept"
		OptItem "Reject Change|j" "change-reject"
		Separator
		Submenu "Text Properties|x" "edit_textprops"
		OptSubmenu "Custom Text Styles|y" "edit_textstyles"
		Item "Paragraph Settings...|P" "layout-paragraph"
		OptItem "Unify Graphics Groups|U" "graphics-unify"
		LanguageSelector
		Separator
		Item "Fullscreen Mode" "ui-toggle fullscreen"
		OptItem "Close Current View" "tab-group-close"
	End


#
# Regular Expression context menu
#
	Menu "context-edit-regexp"
		Item "Anything|A" "command-sequence regexp-mode; self-insert .*"
		Item "Anything Non-Empty|o" "command-sequence regexp-mode; self-insert .+"
		Item "Any Word|W" "command-sequence regexp-mode; self-insert \w+"
		Item "Any Number|N" "command-sequence regexp-mode; self-insert [0-9]+"
		Item "User Defined|U" "regexp-mode"
	End


#
# Math Macro context menu
#

	Menu "context-math-macro-definition"
		Item "Append Argument" "math-macro-add-param"
		Item "Remove Last Argument" "math-macro-remove-param"
		Separator
		Item "Make First Non-Optional Into Optional Argument" "math-macro-make-optional"
		Item "Make Last Optional Into Non-Optional Argument" "math-macro-make-nonoptional"
		Item "Insert Optional Argument" "math-macro-add-optional-param"
		Item "Remove Optional Argument" "math-macro-remove-optional-param"
		Separator
		Item "Append Argument Eating From the Right" "math-macro-append-greedy-param"
		Item "Append Optional Argument Eating From the Right" "math-macro-add-greedy-optional-param"
		Item "Remove Last Argument Spitting Out to the Right" "math-macro-remove-greedy-param"
	End

#
# InsetGraphics context menu
#

	Menu "context-graphics"
		Item "Settings...|S" "inset-settings"
		Item "Reload|R" "graphics-reload"
		Separator
		Item "Edit Externally...|x" "inset-edit"
		Separator
		GraphicsGroups
	End

#
# InsetExternal context menu
#

	Menu "context-external"
		Item "Settings...|S" "inset-settings"
		Separator
		Item "Edit Externally...|x" "inset-edit"
	End

#
# InsetTabular context menu
#

	Menu "table-borders"
		Item "Top|T" "tabular-feature toggle-line-top"
		Item "Bottom|B" "tabular-feature toggle-line-bottom"
		Item "Left|L" "tabular-feature toggle-line-left"
		Item "Right|R" "tabular-feature toggle-line-right"
		OptItem "Reset Formal Defaults|F" "tabular-feature reset-formal-default"
	End

	Menu "table-alignment"
		Item "Left|f" "command-alternatives tabular-feature m-align-left;tabular-feature align-left"
		Item "Center|C" "command-alternatives tabular-feature m-align-center;tabular-feature align-center"
		Item "Right|h" "command-alternatives tabular-feature m-align-right;tabular-feature align-right"
		Item "Decimal" "tabular-feature align-decimal"
		Separator
		Item "Top" "tabular-feature valign-top"
		Item "Middle" "tabular-feature valign-middle"
		Item "Bottom" "tabular-feature valign-bottom"
	End

	Menu "table-cols-rows"
		Item "Multicolumn|u" "tabular-feature multicolumn"
		Item "Multirow|w" "tabular-feature multirow"
		Separator
		Item "Append Row|A" "tabular-feature append-row"
		Item "Delete Row|D" "tabular-feature delete-row"
		Item "Copy Row|o" "tabular-feature copy-row"
		Item "Move Row Up" "tabular-feature move-row-up"
		Item "Move Row Down" "tabular-feature move-row-down"
		Separator
		Item "Append Column|p" "tabular-feature append-column"
		Item "Delete Column|e" "tabular-feature delete-column"
		Item "Copy Column|y" "tabular-feature copy-column"
		Item "Move Column Right|v" "tabular-feature move-column-right"
		Item "Move Column Left" "tabular-feature move-column-left"
	End

	Menu "context-tabular"
		Item "Multi-page Table|g" "tabular-feature toggle-longtabular"
		Item "Formal Style|m" "tabular-feature toggle-booktabs"
		Separator
		Submenu "Borders|d" "table-borders"
		Submenu "Alignment|i" "table-alignment"
		Submenu "Columns/Rows|C" "table-cols-rows"
		Separator
		Item "Settings...|S" "inset-settings tabular"
	End

#
# InsetInfo context menu
#

	Menu "context-info"
		InfoArguments
		Separator
		Item "Transform Field to Static Text|T" "inset-dissolve"
		Item "Copy Text|o" "inset-copy-as"
		Item "Settings...|S" "inset-settings info"
	End

#
# InsetBranch context menu
#

	Menu "context-branch"
		OptItem "Activate Branch|A" "branch-activate"
		OptItem "Deactivate Branch|e" "branch-deactivate"
		OptItem "Activate Branch in Master|M" "branch-master-activate"
		OptItem "Deactivate Branch in Master|v" "branch-master-deactivate"
		OptItem "Invert Inset|I" "branch-invert"
		OptItem "Add Unknown Branch|w" "branch-add"
	End

#
# Toc Labels and References context menu
#

	Menu "context-toc-label"
		OptItem "Copy as Reference|C" "label-copy-as-reference"
		OptItem "Insert Reference at Cursor Position|I" "label-insert-as-reference"
		Separator
		Item "Settings...|S" "inset-settings"
	End

#
# Toc Branches context menu
#

	Menu "context-toc-branch"
		OptItem "Activate Branch|A" "branch-activate"
		OptItem "Deactivate Branch|e" "branch-deactivate"
		Separator
		Item "Settings...|S" "inset-settings"
	End

#
# Toc Graphics context menu
#

	Menu "context-toc-graphics"
		Item "Settings...|S" "inset-settings"
		Separator
		Item "Edit Externally...|x" "inset-edit"
	End

#
# Toc Citation context menu
#

	Menu "context-toc-citation"
		Item "Settings...|S" "inset-settings"
	End

#
# Toc Figures context menu
#

	Menu "context-toc-figure"
		Item "Settings...|S" "inset-settings"
		Separator
		SubMenu "Insert Cross-Reference to this Item|C" "context-toc-ref-to-par"
	End


#
# Toc Footnotes context menu
#

	Menu "context-toc-footnote"
		SubMenu "Insert Cross-Reference to this Item|C" "context-toc-ref-to-par"
	End

#
# Toc Listings context menu
#

	Menu "context-toc-listing"
		Item "Settings...|S" "inset-settings"
	End

#
# Toc Tables context menu
#

	Menu "context-toc-table"
		Item "Settings...|S" "inset-settings"
		Separator
		SubMenu "Insert Cross-Reference to this Item|C" "context-toc-ref-to-par"
	End

#
# Toc Childs context menu
#
	Menu "context-toc-child"
		Item "Include|c" "inset-modify changetype include"
		Item "Input|p" "inset-modify changetype input"
		Item "Verbatim|V" "inset-modify changetype verbatiminput"
		Item "Verbatim (marked blanks)|b" "inset-modify changetype verbatiminput*"
		Item "Listing|L" "inset-modify changetype lstinputlisting"
		Separator
		Item "Settings...|S" "inset-settings"
		Separator
		Item "Edit Included File...|E" "inset-edit"
	End

#
# Toc Indices context menu
#

	Menu "context-toc-index"
		OptItem "Insert Copy at Cursor Position|I" "inset-insert-copy"
		Separator
		OptItem "Settings...|S" "inset-settings"
	End

#
# Index context menu
#

	Menu "context-index"
		IndicesContext
		Separator
		Item "Index All Occurrences of this Word|W" "index-tag-all"
		Separator
		OptItem "Single Page (No Page Range)|P" "inset-modify changeparam range none"
		OptItem "Start Page Range|t" "inset-modify changeparam range start"
		OptItem "End Page Range|E" "inset-modify changeparam range end"
		Separator
		OptItem "No Page Formatting|N" "inset-modify changeparam pagefmt default"
		OptItem "Bold Page Formatting|B" "inset-modify changeparam pagefmt textbf"
		OptItem "Italic Page Formatting|I" "inset-modify changeparam pagefmt textit"
		OptItem "Emphasized Page Formatting|z" "inset-modify changeparam pagefmt emph"
		OptItem "Custom Page Formatting...|u" "inset-modify changeparam pagefmt custom"
	End
	
	Menu "context-edit-index"
		OptItem "Insert Subentry|n" "indexmacro-insert subentry"
		OptItem "Insert Sortkey|k" "indexmacro-insert sortkey"
		OptItem "Insert See Reference|cf" "indexmacro-insert see"
		OptItem "Insert See also Reference|a" "indexmacro-insert seealso"
	End

#
# IndexMacro context menu
#

	Menu "context-indexmacro"
		OptItem "See|e" "inset-modify changetype see"
		OptItem "See also|a" "inset-modify changetype seealso"
	End


#
# Index Lists context menu
#

	Menu "context-indexprint"
		Item "All Indexes|A" "inset-modify check-printindex*"
		IndicesListsContext
		Separator
		Item "Subindex|b" "inset-modify toggle-subindex"
		Separator
		OptItem "Settings...|S" "inset-settings"
	End

#
# Nomencl List context menu
#

	Menu "context-nomenclprint"
		OptItem "Settings...|S" "inset-settings"
	End


# InsetPreview context menu
#

	Menu "context-preview"
		Item "Dissolve Inset|D" "inset-dissolve"
	End


#
# Toc Changes context menu
#

	Menu "context-toc-change"
		Item "Accept Change|c" "change-accept"
		Item "Reject Change|R" "change-reject"
	End
	
#
# Toc Table of Context context menu
#

        Menu "context-toc-ref-to-par"
        	Item "<Reference>|R" "reference-to-paragraph ref"
		Item "(<Reference>)|e" "reference-to-paragraph eqref"
		Item "<Page>|P" "reference-to-paragraph pageref"
		OptItem "p. <Page>|." "reference-to-paragraph cpageref"
		Item "On Page <Page>|O" "reference-to-paragraph vpageref"
		Item "<Reference> on Page <Page>|f" "reference-to-paragraph vref"
		Item "Formatted Reference|t" "reference-to-paragraph formatted"
		Item "Textual Reference|x" "reference-to-paragraph nameref"
		Item "Label Only|L" "reference-to-paragraph labelonly"
        
        End

	Menu "context-toc-tableofcontents"
		Item "Promote Section|P" "outline-out"
		Item "Demote Section|D" "outline-in"
		Item "Move Section Up|U" "outline-up"
		Item "Move Section Down|w" "outline-down"
		Separator
		Item "Select Section|S" "section-select"
		Separator
		SubMenu "Insert Cross-Reference to this Item|C" "context-toc-ref-to-par"
	End

#
# InsetERT context menu
#
	Menu "context-ert"
		# repeat 1 is added as a work-around to not indicate this action as toggable
		Item "Wrap by Preview|y" "command-sequence repeat 1;inset-toggle;char-forward;char-backward;char-forward-select;preview-insert;char-backward;char-backward;inset-toggle"
		OptItem "Edit Externally..." "inset-edit"
		OptItem "End Editing Externally" "inset-end-edit"
	End

#
# InsetHyperlink context menu
#
	Menu "context-hyperlink"
		Item "Settings...|S" "inset-settings"
		Separator
		Item "Open Target...|O" "inset-edit"
	End

#
# InsetCounter context menu
#
	Menu "context-counter"
		Item "Settings...|S" "inset-settings"
	End

#
# Toolbar context menu
#
	Menu "context-toolbars"
		Toolbars
		Separator
		Item "Lock Toolbars|L" "toolbar-movable *"
		Separator
		Item "Small-sized Icons" "icon-size small"
		Item "Normal-sized Icons" "icon-size normal"
		Item "Big-sized Icons" "icon-size big"
		Item "Huge-sized Icons" "icon-size huge"
		Item "Giant-sized Icons" "icon-size giant"
	End

#
# Status bar zoom context menu
#
	Menu "context-zoom"
		ZoomOptions
	End

#
# Status bar context menu
#
	Menu "context-statusbar"
		Item "Zoom Level|Z" "ui-toggle zoomlevel"
		Item "Zoom Slider|S" "ui-toggle zoomslider"
		Separator
		Item "Word Count|W" "ui-toggle statistics-w"
		Item "Character Count|C" "ui-toggle statistics-cb"
		Item "Character Count (No Blanks)|h" "ui-toggle statistics-c"
		Item "Start Statistics Relative to Current Count|R" "statistics-reference-clamp"
		OptItem "Reset to Absolute Statistics Count|A" "statistics-reference-clamp reset"
	End

End
