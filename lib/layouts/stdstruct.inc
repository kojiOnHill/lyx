# Standard textclass definition file. Taken from initial LyX source code
# Author : Matthias Ettrich <ettrich@informatik.uni-tuebingen.de>
# Transposed by Pascal André <andre@via.ecp.fr>
# Heavily modifed and enhanced by several developers.
# Title definitions moved to stdtitle.inc by Hartmut Goebel <harmut@noris.net>

# This include files contains all layouts that handle the structure of
# a document, like abstract, bibliography and such.


Format 108

Style Abstract
	Margin                Static
	LatexType             Environment
	LatexName             abstract
	Category              FrontMatter
	NextNoIndent          1
	ToggleIndent          Always
	LeftMargin            MMM
	RightMargin           MMM
	ParIndent             MM
	ItemSep               0
	TopSep                0.7
	BottomSep             0.7
	ParSep                0
	Align                 Block
	LabelType             Centered
	LabelString           "Abstract"
	LabelBottomSep        0.2
	Font
	  Size                Small
	EndFont
	LabelFont
	  Series              Bold
	EndFont
	HTMLLabelFirst        1
	HTMLForceCSS          1
	HTMLStyle
		div.abstract {
			margin: 4ex;
		}
		div.abstract_item {
			font-size: small;
			padding-top: 1ex;
		}
		div.abstract_label {
			font-weight: bold;
		}
	EndHTMLStyle
	DocBookAbstract       true
	DocBookInInfo         always
	DocBookWrapperTag     abstract
	DocBookWrapperMergeWithPrevious    true
	DocBookTag            para
End


Style Bibliography
	Margin                First_Dynamic
	LatexType             Bib_Environment
	LatexName             thebibliography
	Category              BackMatter
	NextNoIndent          1
	ToggleIndent          Never
	LeftMargin            MM
	ParSkip               0.4
	ItemSep               0
	TopSep                0.7
	BottomSep             0.7
	ParSep                0.5
	Align                 Block
	AlignPossible         Block, Left
	LabelType             Bibliography
	LabelString           "References"
	LabelBottomSep        1.2
	LabelFont
	  Series              Bold
	  Size                Larger
	EndFont
	HTMLStyle
		div.bibliography {
			margin-left: 2em;
			text-indent: -2em;
		}
		span.bibitemlabel:before { content: "["; }
		span.bibitemlabel:after { content: "] "; }
	EndHTMLStyle
	TocLevel              1
	ISTocCaption          0
	Argument listpreamble:1
	  LabelString    "Bib preamble"
	  MenuString     "Bibliography Preamble"
	  Tooltip        "LaTeX code to be inserted before the first bibliography item"
	  PassThru       1
	  Font
	    Family typewriter
	    Color latex
	  EndFont
	EndArgument
	DocBookTag            bibliography
End
