# Standard textclass definition file. Taken from initial LyX source code
# Author : Matthias Ettrich <ettrich@informatik.uni-tuebingen.de>
# Transposed by Pascal André <andre@via.ecp.fr>
# Heavily modifed and enhanced by several developers.

# This include files contains various standard environments for lists.


Format 108

Input stdlyxlist.inc

Style Itemize
	Category              List
	Margin                Static
	LatexType             Item_Environment
	LatexName             itemize
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
	HTMLTag               ul
	HTMLItem              li
	HTMLLabel             NONE
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
	LatexName             enumerate
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
	LabelCounter          "enum"
	HTMLTag               ol
	HTMLItem              li
	HTMLLabel             NONE
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

Style Description
	Category              List
	Margin                First_Dynamic
	LatexType             Item_environment
	LatexName             description
	NextNoIndent          1
	LeftMargin            MM
	LabelSep              xxx
	ParSkip               0.4
	ItemSep               0.2
	TopSep                0.7
	BottomSep             0.7
	ParSep                0.3
	Align                 Block
	AlignPossible         Block, Left
	LabelType             Manual
	LabelFont
	  Series              Bold
	EndFont
	HTMLTag               dl
	HTMLItem              dd
	HTMLLabel             dt
	HTMLLabelFirst        1
	HTMLStyle
		dl.description dt { font-weight: bold; }
	EndHTMLStyle
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
	DocBookTag            variablelist
	DocBookItemWrapperTag varlistentry
	DocBookItemTag        listitem
	DocBookItemInnerTag   para
	DocBookItemLabelTag   term
	DocBookItemLabelTagType   paragraph
End


Style List
  Obsoletedby           Labeling
End
