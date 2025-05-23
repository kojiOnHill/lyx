# APA x textclass definition file.
# Author: Jacob Bishop <bishop.jacob@gmail.com> based on the apa 
# layout written by Randy Gobbel.

Format 108
Columns                 1
Sides                   1
PageStyle               Headers


ClassOptions
	Other "man"
	FontSize              10|11|12
End


Preamble
EndPreamble


Style Standard
	Category              MainText
	LatexName             dummy
	LatexType             Paragraph
	Margin                Static
	ParIndent             MM
	ParSkip               0.4
	ParSep                0.4
	TopSep                1.0
	BottomSep             1.0
	Align                 Block
	AlignPossible         Block, Left, Right, Center
	LabelType             No_Label
	DocBookTag            para
End


Style Title
	Category              FrontMatter
	LatexName             title
	LatexType             Command
	InTitle               1
	Font
	  Size                Huge
	EndFont
	Align                 Center
	HTMLTitle             true
	DocBookTag            title
	DocBookTagType        paragraph
	DocBookInInfo         maybe
End


Style ShortTitle
	Category              FrontMatter
	LatexName             shorttitle
	LatexType             Command
	InTitle               1
	Align                 Left
	AlignPossible         Left, Right, Center
	Margin                Dynamic
	Labeltype             Static
	LabelString           "Short title:"
	Labelsep              M
	Font
	  Shape               SmallCaps
	  Size                Small
	EndFont
	LabelFont
	  Series              Bold
	  Size                Small
	  Shape               Up
	EndFont
	DocBookTag            titleabbrev
	DocBookTagType        paragraph
	DocBookInInfo         maybe
End


Style Author
	Category              FrontMatter
	LatexName             author
	LatexType             Command
	InTitle               1
	Align                 Center
	AlignPossible         Left, Right, Center
	TopSep                0.5
	BottomSep             0.5
	Font
	  Shape               Italic
	  Size                Small
	EndFont
	DocBookTag            personname
	DocBookTagType        paragraph
	DocBookWrapperTag     author
	DocBookWrapperTagType inline
	DocBookInInfo         always
End


Style TwoAuthors
	Category              FrontMatter
	LatexName             twoauthors
	LatexType             Command
	InTitle               1
End


Style ThreeAuthors
	Category              FrontMatter
	LatexName             threeauthors
	LatexType             Command
	InTitle               1
End


Style FourAuthors
	Category              FrontMatter
	LatexName             fourauthors
	LatexType             Command
	InTitle               1
End

Style FiveAuthors
	Category              FrontMatter
	LatexName             fiveauthors
	LatexType             Command
	InTitle               1
End

Style SixAuthors
	Category              FrontMatter
	LatexName             sixauthors
	LatexType             Command
	InTitle               1
End

Style LeftHeader
	Category              FrontMatter
	LatexName             leftheader
	LatexType             Command
	InTitle               1
	Align                 Left
	AlignPossible         Left, Right, Center
	Margin                Dynamic
	Labeltype             Static
	LabelString           "Left header:"
	Labelsep              M
	Font
	  Size                Small
	  Shape               SmallCaps
	EndFont
	LabelFont
	  Series              Bold
	  Size                Small
	EndFont
	DocBookTag            authorinitials
	DocBookAttr           role='left-header'
	DocBookTagType        paragraph
	DocBookInInfo         maybe
End


Style Affiliation
	Category              FrontMatter
	LatexName             affiliation
	LatexType             Command
	InTitle               1
	Margin                Dynamic
	ParSkip               0.4
	BottomSep             1.0
	TopSep                1.5
	ParSep                1.5
	Align                 Left
	AlignPossible         Block, Left, Right, Center
	LabelType             Static
	LabelString           "Affiliation:"
	Labelsep              M
	LabelFont
	  Series              Bold
	  Size                Small
	EndFont
	DocBookTag                        affiliation
	DocBookWrapperTag                 author
	DocBookWrapperMergeWithPrevious   true
	DocBookItemTag                    orgname
	DocBookItemTagType                paragraph
	DocBookInInfo                     always
End

Style TwoAffiliations
	Category              FrontMatter
	LatexName             twoaffiliations
	LatexType             Command
	InTitle               1
End

Style ThreeAffiliations
	Category              FrontMatter
	LatexName             threeaffiliations
	LatexType             Command
	InTitle               1
End

Style FourAffiliations
	Category              FrontMatter
	LatexName             fouraffiliations
	LatexType             Command
	InTitle               1
End

Style FiveAffiliations
	Category              FrontMatter
	LatexName             fiveaffiliations
	LatexType             Command
	InTitle               1
End

Style SixAffiliations
	Category              FrontMatter
	LatexName             sixaffiliations
	LatexType             Command
	InTitle               1
End


Style Note
	Category              FrontMatter
	LatexName             note
	LatexType             Command
	InTitle               1
	Margin                Dynamic
	ParSkip               0.4
	BottomSep             1.5
	TopSep                1.5
	ParSep                1.5
	Align                 Left
	AlignPossible         Block, Left, Right, Center
	LabelType             Static
	LabelString           "Note:"
	Labelsep              M
	LabelFont
	  Series              Bold
	  Size                Small
	EndFont
End


Style Abstract
	Category              FrontMatter
	LatexName             abstract
	LatexType             Command
	InTitle               1
	Font
	  Size                Small
	EndFont
	Margin                Dynamic
	LeftMargin            MMN
	RightMargin           MMN
	Align                 Block
	AlignPossible         Block, Left, Right, Center
	Labeltype             Static
	LabelString           "Abstract:"
	Labelsep              M
	LabelFont
	  Series              Bold
	  Size                Small
	EndFont
	DocBookAbstract       true
	DocBookInInfo         always
	DocBookWrapperTag     abstract
	DocBookWrapperMergeWithPrevious    true
	DocBookTag            para
End

Style Keywords
	Category              FrontMatter
	LatexName             keywords
	LatexType             Command
	InTitle               1
	Font
	  Size                Small
	EndFont
	Margin                Dynamic
	LeftMargin            MMN
	RightMargin           MMN
	Align                 Block
	AlignPossible         Block, Left, Right, Center
	TopSep                0.8
	BottomSep             0.8
	Labeltype             Static
	LabelString           "Keywords:"
	Labelsep              M
	LabelFont
	  Size                Small
	  Shape               Italic
	EndFont
	DocBookTag          keyword
	DocBookTagType      paragraph
	DocBookWrapperTag   keywordset
	DocBookInInfo       always
End

Style AuthorNote
	Category              FrontMatter
	LatexName             authornote
	LatexType             Command
	InTitle               1
	Font
	  Size                Small
	EndFont
	Margin                Dynamic
	ParSkip               0.4
	BottomSep             1.5
	TopSep                1.5
	ParSep                1.5
	Align                 Left
	LeftMargin            MMN
	RightMargin           MMN
	Align                 Block
	AlignPossible         Block, Left, Right, Center
	Labeltype             Static
	LabelString           "Author Note:"
	Labelsep              M
	LabelFont
	  Series              Bold
	  Size                Small
	EndFont
	DocBookTag            para
	DocBookWrapperTag     acknowledgements
	DocBookSection        true
End


Style Journal
	Category              Preamble
	LatexName             journal
	LatexType             Command
	InPreamble            1
End


Style Copyright
	Category              Preamble
	LatexName             ccoppy
	LatexType             Command
	InPreamble            1
	Align                 Right
End


Style CopNum
	Category              Preamble
	LatexName             copnum
	LatexType             Command
	InPreamble            1
	Align                 Right
End

Style Volume
	Category              Preamble
	LatexName             volume
	LatexType             Command
	InPreamble            1
End

Style ThickLine
	LatexName             thickline
	LatexType             Command
	KeepEmpty             1
End



Input stdlayouts.inc


InsetLayout Caption:Centered
	LabelString          standard
	LaTeXType            command
	LatexName            centeredcaption
	NeedProtect          1
	MultiPar             false
	Argument 1
		LabelString   "Short Title|S"
		Tooltip       "The caption as it appears in the list of figures/tables"
		InsertCotext  1
	EndArgument
	HTMLStyle
		div.float-caption {
			text-align: center;
			border: 2px solid black;
			padding: 1ex;
			margin: 1ex;
		}
	EndHTMLStyle
	HTMLClass   "float-caption float-caption-centered"
End


Style FitFigure
	LatexName             fitfigure
	LatexType             Command
End


Style FitBitmap
	LatexName             fitbitmap
	LatexType             Command
End


Input stdsections.inc
Input stdstarsections.inc
Input stdinsets.inc

Input stdfloats.inc
Input stdcounters.inc

NoStyle Part
NoCounter part
NoStyle Part*
NoStyle Chapter
NoCounter chapter
NoStyle Chapter*
NoStyle Section*
NoStyle Subsection*
NoStyle Subsubsection*
NoStyle Paragraph*
NoStyle Subparagraph*


Style Section
	LabelType             No_Label
	Align                 Center
	Font
	  Series              Bold
	  Size                Huge
	EndFont
	TocLevel              1
End


Style Subsection
	LabelType             No_Label
	Font
	  Series              Bold
	  Size                Larger
	EndFont
	TocLevel              2
End


Style Subsubsection
	LabelType             No_Label
	Font
	  Series              Bold
	  Size                Large
	EndFont
	TocLevel              3
End


Style Paragraph
	LabelType             No_Label
	Font
	  Series              Bold
	  Size                Normal
	EndFont
	TocLevel              4
End


Style Subparagraph
	LabelType             No_Label
	Font
	  Series              Bold
	  Size                Normal
	  Shape               Italic
	EndFont
	TocLevel              5
End


Style Itemize
	Category              List
	Margin                Static
	LatexType             Item_Environment
	LatexName             APAitemize
	NextNoIndent          1
	ToggleIndent          Never
	LeftMargin            MMN
	LabelSep              xx
	ItemSep               0.2
	TopSep                0.7
	BottomSep             0.7
	ParSep                0.3
	Align                 Block
	AlignPossible         Block, Left
	LabelType             Itemize
	LabelString           "*"
	Argument item:1
		LabelString   "Custom Item|s"
		Tooltip       "A customized item string"
	EndArgument
	Argument listpreamble:1
		LabelString    "List preamble"
		MenuString     "List Preamble"
		Tooltip        "LaTeX code to be inserted before the first item"
		PassThru       1
		Font
			Family typewriter
			Color latex
		EndFont
	EndArgument
	DocBookTag            itemizedlist
	DocBookItemTag        listitem
	DocBookItemInnerTag   para
End


Style Enumerate
	Category              List
	Margin                Static
	LatexType             Item_Environment
	LatexName             APAenumerate
	NextNoIndent          1
	ToggleIndent          Never
	LeftMargin            MMN
	LabelSep              xx
	ParSkip               0.0
	ItemSep               0.2
	TopSep                0.7
	BottomSep             0.7
	ParSep                0.3
	Align                 Block
	AlignPossible         Block, Left
	LabelType             Enumerate
	RefPrefix             enu
	Argument item:1
		LabelString   "Custom Item|s"
		Tooltip       "A customized item string"
	EndArgument
	Argument listpreamble:1
		LabelString    "List preamble"
		MenuString     "List Preamble"
		Tooltip        "LaTeX code to be inserted before the first item"
		PassThru       1
		Font
			Family typewriter
			Color latex
		EndFont
	EndArgument
	DocBookTag            orderedlist
	DocBookItemTag        listitem
	DocBookItemInnerTag   para
	HTMLStyle
		ol.enumi   { list-style-type: decimal; }
		ol.enumii  { list-style-type: lower-latin; }
		ol.enumiii { list-style-type: lower-roman; }
		ol.enumiv  { list-style-type: upper-latin; }
	EndHTMLStyle
End


Style Seriate
	Category              List
	Margin                Static
	LatexType             Item_Environment
	LatexName             seriate
	NextNoIndent          1
	LeftMargin            MMN
	LabelSep              x
	ParSkip               0.0
	ItemSep               0.9
	TopSep                0.5
	BottomSep             0.5
	ParSep                0.0
	Align                 Block
	AlignPossible         Block, Left
	LabelType             Itemize
	LabelCounter          enumii
	LabelString           "(\alph{enumii})"
	LabelStringAppendix   "(\alph{enumii})"
	RefPrefix             ser
	Argument listpreamble:1
		LabelString    "List preamble"
		MenuString     "List Preamble"
		Tooltip        "LaTeX code to be inserted before the first item"
		PassThru       1
		Font
			Family typewriter
			Color latex
		EndFont
	EndArgument
End

Style Course
	Category              FrontMatter
	InTitle               1
	LatexName             course
	LatexType             Command
	Font
	  Size                Small
	EndFont
	Margin                Dynamic
	LeftMargin            MMN
	RightMargin           MMN
	Align                 Block
	AlignPossible         Block, Left, Right, Center
	TopSep                0.8
	BottomSep             0.8
	Labeltype             Static
	LabelString           "Course: "
	Labelsep              M
	LabelFont
	  Size                Small
	  Shape               Italic
	EndFont
	DocBookTag            releaseinfo
	DocBookAttr           role='course'
	DocBookInInfo         always
End


# The addORCIDlink{}{} latex command can be used in the preamble,
# but also inside some other style like note, paragraph etc
Style addORCIDlink
	Category              FrontMatter
	InTitle               1
	Labeltype Static
	LabelString           "ORCID-link: "
	LatexName             addORCIDlink
	LatexType             Command
	Margin                Dynamic
	LeftMargin            MMN
	RightMargin           MMN
	Align Block
	Argument              1
	  LabelString         "Author-name"
	  Tooltip             ORCID
	  Mandatory           1
	  DocBookTag          phrase
	  DocBookAttr         role='author-name'
	EndArgument
	# TODO: for DocBook, this won't work outside the preamble.
	DocBookTag            personblurb
	DocBookAttr           role='orcid'
	DocBookTagType        paragraph
	DocBookInInfo         maybe
End


# To be used inside the document body
InsetLayout Flex:addORCIDlink
	LabelString           "ORCID-link: "
	LatexName             addORCIDlink
	Decoration            none
	LatexType             Command
	LyxType               custom
	Argument              1
	  LabelString         "Author-name"
	  Mandatory           1
	AutoInsert            1
	Decoration            none
	EndArgument
	Argument              2
	  LabelString         ORCID
	  AutoInsert          1
	  Mandatory           1
	  Decoration          none
	EndArgument
End

