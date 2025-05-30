# REVTeX 4.x textclass definition file.
# Based on official article.layout, revtex4.layout and stdclass.inc definition files,
# and on the unofficial revtex4-1.layout by Manoj Rajagopalan, posted to the wiki.
#
# Author: Julien Rioux <jrioux@lyx.org> and Uwe Stöhr
#
# This layout is according to
# https://journals.aps.org/files/revtex/auguide4-1.pdf

# a note:
#   - we provide natbib-internal according to LyX bug #5182
# todo:
#     - add support for \onlinecite{}


Format 108

Input stdclass.inc
Input numrevtex.inc

Provides natbib-internal	1
Provides url			1
Provides tablefootnote          1
Provides footnote-alternative   1

SecNumDepth			4
TocDepth			3
DefaultStyle		Standard

Style Section
	Align			Center
	Font
	  Shape			Smallcaps
	EndFont
	TocLevel 1
End


Style Subsubsection
	Font
	  Shape			Italic
	EndFont
	TocLevel 3
End


Style Paragraph
	Font
	  Shape			Italic
	EndFont
	TocLevel 4
End


# This input needs to appear *after* modifications to sections
Input stdstarsections.inc


########### Front matter ############

Style Abstract
	InTitle			1
End


Style Affiliation
	Category		FrontMatter
	Margin			Dynamic
	LatexType		Command
	InTitle			1
	LatexName		affiliation
	ParSkip			0.4
	Align			Center
	AlignPossible	Block, Left, Right, Center
	LabelType		Static
	LabelSep		M
	TextFont
	  Family		Typewriter
	EndFont
	LabelString		"Affiliation:"
	LabelFont
	  Color			textlabel2
	  Series		Bold	
	EndFont
	DocBookTag                        address
	DocBookTagType                    paragraph
	DocBookWrapperTag                 author
	DocBookWrapperMergeWithPrevious   true
	DocBookInInfo                     always
End


Style Affiliation_(alternate)
	CopyStyle		Affiliation
	LatexName		altaffiliation
	LabelString		"Affiliation (alternate):"
	Argument 1
	  LabelString	"Alternate Affiliation Option"
	  Tooltip		"Optional argument to the altaffiliation command"
	EndArgument
	PassThru		1
	DocBookTag                        address
	DocBookAttr                       role='alternate'
	DocBookTagType                    paragraph
	DocBookWrapperTag                 author
	DocBookWrapperMergeWithPrevious   true
	DocBookInInfo                     always
End


Style Affiliation_(none)
	CopyStyle		Affiliation
	KeepEmpty		true
	LabelString		"No affiliation"
	LatexName		noaffiliation%
End


# Address is exactly the same as Affiliation. We just put
# this in so that tex2lyx will know what to do with it.
Style Address
	CopyStyle		Affiliation
	LatexName		address
	LabelString		"Address:"
End


Style Author_Email
	CopyStyle		Affiliation
	LatexName		email
	LabelString		"Electronic Address:"
	Argument 1
	  LabelString	"Electronic Address Option|s"
	  Tooltip		"Optional argument to the email command"
	EndArgument
	PassThru		1
	TextFont
	  Family		Roman
	EndFont
	DocBookTag            email
	DocBookTagType        paragraph
	DocBookWrapperTag     author
	DocBookWrapperTagType block
	DocBookWrapperMergeWithPrevious  true
	DocBookInInfo         always
End


Style Author_URL
	CopyStyle		Author_Email
	LatexName		homepage
	LabelString		"URL:"
	Argument 1
	  LabelString	"Author URL Option"
	  Tooltip		"Optional argument to the homepage command"
	EndArgument
	PassThru		1
	DocBookTag            uri
	DocBookTagType        paragraph
	DocBookWrapperTag     author
	DocBookWrapperTagType block
	DocBookWrapperMergeWithPrevious  true
	DocBookInInfo         always
End


Style Collaboration
	CopyStyle		Affiliation
	LatexName		collaboration
	LabelString		"Collaboration:"
End


Style Date
	LabelType		Static
	LabelString		"Date:"
End


Style Keywords
	Category		FrontMatter
	Margin			Dynamic
	LatexType		Command
	InTitle			1
	LatexName		keywords
	LabelType		Static
	LabelSep		M
	LabelString		"Keywords:"
	LabelFont
	  Color			textlabel2
	  Series		Bold
	EndFont
	DocBookTag          keyword
	DocBookTagType      paragraph
	DocBookWrapperTag   keywordset
	DocBookInInfo       always
End


Style PACS
	CopyStyle		Keywords
	LatexName		pacs
	LabelString		"PACS numbers:"
	DocBookTag            subjectset
	DocBookAttr           role='pacs'
	DocBookTagType        block
	DocBookItemWrapperTag      subject
	DocBookItemWrapperTagType  paragraph
	DocBookItemTag        subjectterm
	DocBookItemTagType    inline
	DocBookWrapperTag     NONE
	DocBookWrapperAttr    ""
	DocBookInInfo         always
End


Style Preprint
	Category		FrontMatter
	Margin			Right_Address_Box
	LatexType		Command
	InTitle			1
	LatexName		preprint
	ParSkip			0.4
	BottomSep		1.5
	ParSep			1.5
	Align			Left
	DocBookWrapperTag   cover
	DocBookWrapperAttr  role='preprint'
	DocBookTag          para
	DocBookInInfo       always
End


Style Thanks
	# TODO: This style has a misleading name, innit?
	CopyStyle		Affiliation
	LatexName		thanks
	LabelString		"Title footnote:"
	DocBookTag            subtitle
	DocBookAttr           role='footnote'
	DocBookWrapperTag     NONE
	DocBookInInfo         maybe
End


Style Title
	Argument 1
	  LabelString	"Short Title|S"
	  Tooltip		"Short title as it appears in the running headers"
	  InsertCotext	1
	EndArgument
	Font
	  Series		Bold
	  Size			Large
	EndFont
End


########### Back matter #############

Style Acknowledgments
	CopyStyle		Standard
	Category		BackMatter
	LatexType		Environment
	LatexName		acknowledgments
	LabelType		Centered
	LabelString		"acknowledgments"
	LabelBottomSep	0.5
	LabelFont
	  Series		Bold
	  Shape			Smallcaps
	  Size			Larger
	EndFont
	TopSep			0.7
	DocBookTag            para
	DocBookWrapperTag     acknowledgements
	DocBookSection        true
End


##### stuff specific to REVTeX 4.1

Style Ruled_Table
	CopyStyle		Standard
	Category		Specials
	LatexName		ruledtabular
	LatexType		environment
End


Style Turn_Page
	CopyStyle		Standard
	Category		Specials
	LatexName		turnpage
	LatexType		environment
End


Style Wide_Text
	CopyStyle		Standard
	Category		Specials
	LatexName		widetext
	LatexType		environment
End


Input stdfloatnames.inc

Float
	Type			algorithm
	GuiName			Algorithm
	Placement		H
	AllowedPlacement none
	AllowsSideways	0
	AllowsWide		0
	Extension		loa
	NumberWithin	none
	Style			ruled
	ListName		"List of Algorithms"
	IsPredefined	false
	UsesFloatPkg	true
	RefPrefix		alg
	PrettyFormat    "Algorithm ##"
End


Float
	Extension		lov
	GuiName			Video
	IsPredefined	true
	ListCommand		listofvideos
	ListName		"List of Videos"
	NumberWithin	none
	Placement		tbp
	AllowsSideways	0
	Style			plain
	Type			video
	UsesFloatPkg	false
	PrettyFormat    "Video ##"
End


OutlinerName video        "Videos"


InsetLayout Flex:Float_Link
	CopyStyle		Flex:URL
	LabelString		"Float link"
	LatexName		setfloatlink
	LatexType		command
	LyXType			custom
End


InsetLayout Flex:Lowercase
	LyxType			custom
	LatexType		command
	LatexName		lowercase
	LabelString		"lowercase text"
	Font
	  # Keep empty to preserve font settings
	  # from the surrounding environment
	EndFont
	LabelFont
	  Size			Small
	EndFont
	ResetsFont true
End


InsetLayout "Flex:Online cite"
	LyxType			custom
	LatexType		command
	LatexName		onlinecite
	LabelString		"online cite"
	Argument 1
	  LabelString	"Text behind"
	  Tooltip		"text behind the cite"
	EndArgument
	Font
	  # Keep empty to preserve font settings
	  # from the surrounding environment
	EndFont
	LabelFont
	  Size			Small
	EndFont
	ResetsFont true
End


InsetLayout Flex:Surname
	Decoration		conglomerate
	LabelString		Surname
	LatexName		surname
	LatexType		command
	LyXType			custom
	Font
	  # Keep empty to preserve font settings
	  # from the surrounding environment
	EndFont
	LabelFont
	  Color			textlabel2
	  Size			Small
	EndFont
	ResetsFont true
End


# RevTeX uses normal footnotes in tables
InsetLayout Foot:InFloatTable
	CopyStyle             Foot
End


NoStyle Chapter
NoStyle Chapter*
NoStyle LyX-Code
NoStyle Right_Address
NoStyle Subparagraph
NoStyle Subparagraph*
