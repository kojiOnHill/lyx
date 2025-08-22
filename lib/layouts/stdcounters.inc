# Author : Lars Gullik Bjønnes <larsbj@lyx.org>

# This include file contains all the counters that are defined as standard
# in most LyX layouts.


Format 111
Counter part
	GuiName Part
	LabelString          "\Roman{part}"
	PrettyFormat         "Part|Parts ##"
End

Counter chapter
	GuiName Chapter
	PrettyFormat         "Chapter|Chapters ##"
End

Counter section
	GuiName Section
	Within               chapter
	PrettyFormat         "Section|Sections ##"
End

Counter subsection
	GuiName Subsection
	Within               section
	PrettyFormat         "Section|Sections ##"
End

Counter subsubsection
	GuiName Subsubsection
	Within               subsection
	PrettyFormat         "Section|Sections ##"
End

Counter paragraph
	GuiName Paragraph
	Within               subsubsection
	PrettyFormat         "Paragraph|Paragraphs ##"
End

Counter subparagraph
	GuiName Subparagraph
	Within               paragraph
	PrettyFormat         "Paragraph|Paragraphs ##"
End

Counter enumi
	GuiName "Numbered List (Level 1)"
	LabelString          "\arabic{enumi}."
	PrettyFormat         "Item \arabic{enumi}"
End

Counter enumii
	GuiName "Numbered List (Level 2)"
	Within               enumi
	LabelString          "(\alph{enumii})"
	PrettyFormat         "Item \arabic{enumi}\alph{enumii}"
End

Counter enumiii
	GuiName "Numbered List (Level 3)"
	Within               enumii
	LabelString          "\roman{enumiii}."
	PrettyFormat         "Item \arabic{enumi}(\alph{enumii})\roman{enumiii}"
End

Counter enumiv
	GuiName "Numbered List (Level 4)"
	Within               enumiii
	LabelString          "\Alph{enumiv}."
	PrettyFormat         "Item \arabic{enumi}(\alph{enumii})\roman{enumiii}\Alph{enumiv}"
End

Counter bibitem
	GuiName "Bibliography Item"
End

Counter listing
	GuiName Listing
	PrettyFormat "Listing|Listings ##"
End

Counter equation
	GuiName Equation
	PrettyFormat         "Equation|Equations (##)"
End

Counter footnote
	GuiName Footnote
	PrettyFormat         "Footnote \arabic{footnote}"
End

Counter thanks
	GuiName "Footnote (Title)"
	PrettyFormat         "Footnote \arabic{footnote}"
	LabelString	     "\fnsymbol{thanks}"
End
