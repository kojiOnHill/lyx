# Original Author : David L. Johnson <dlj0@lehigh.edu>
# Probably broken by Jean-Marc Lasgouttes <Jean-Marc.Lasgouttes@inria.fr>
# modified and modularized by Emmanuel GUREGHIAN <gureghia@boston.bertin.fr>
# Tinkered with Sep. '07 by Paul Rubin <rubin@msu.edu>
# Modularized Jan 08 by Richard Kimberly Heck <rikiheck@lyx.org>

# The starred environments defined are :
# - Theorem
# - Corollary
# - Lemma
# - Proposition
# - Conjecture
# - Definition
# - Example
# - Problem
# - Exercise
# - Solution
# - Remark
# - Claim
# - Proof

# TODO: for DocBook, there is no difference with standard (not starred) environments.
# How to encode the subtlety?

Format 108

OutlinerName thm "Definitions & Theorems"

Style Theorem*
	Category              Reasoning
	LatexType             Environment
	Margin                First_Dynamic
	LatexName             thm*
	LabelType             Static
	LabelString           "Theorem."
	NextNoIndent          1
	ResetArgs             1
	AddToToc              thm
	IsTocCaption          1
	Argument 1
		LabelString    "Additional Theorem Text"
		Tooltip        "Additional text appended to the theorem header"
		IsTocCaption      1
	EndArgument
	LabelSep              xx
	ParIndent             MMM
	ParSkip               0.4
	ItemSep               0.2
	TopSep                0.7
	BottomSep             0.7
	ParSep                0.3
	Align                 Block
	AlignPossible         Left
	Font
	  Shape               Italic
	  Size                Normal
	EndFont
	LabelFont
	  Shape               Up
	  Series              Bold
	EndFont
	TheoremName           thm*
	TheoremCounter        none
	TheoremLaTeXName      theoremname
	LangPreamble
	  \providecommand{\theoremname}{_(Theorem)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\theoremname}{_(Theorem)}}
	EndBabelPreamble
	Requires              amsthm
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='theorem'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Corollary*
	CopyStyle             Theorem*
	LatexName             cor*
	LabelString           "Corollary."
	TheoremName           cor*
	TheoremCounter        none
	TheoremLaTeXName      corollaryname
	LangPreamble
	  \providecommand{\corollaryname}{_(Corollary)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\corollaryname}{_(Corollary)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='corollary'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Lemma*
	CopyStyle             Theorem*
	LatexName             lem*
	LabelString           "Lemma."
	TheoremName           lem*
	TheoremCounter        none
	TheoremLaTeXName      lemmaname
	LangPreamble
	  \providecommand{\lemmaname}{_(Lemma)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\lemmaname}{_(Lemma)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='lemma'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Proposition*
	CopyStyle             Theorem*
	LatexName             prop*
	LabelString           "Proposition."
	TheoremName           prop*
	TheoremCounter        none
	TheoremLaTeXName      propositionname
	LangPreamble
	  \providecommand{\propositionname}{_(Proposition)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\propositionname}{_(Proposition)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='proposition'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Conjecture*
	CopyStyle             Theorem*
	LatexName             conjecture*
	LabelString           "Conjecture."
	TheoremName           conjecture*
	TheoremCounter        none
	TheoremLaTeXName      conjecturename
	LangPreamble
	  \providecommand{\conjecturename}{_(Conjecture)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\conjecturename}{_(Conjecture)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='conjecture'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Fact*
	CopyStyle             Theorem*
	LatexName             fact*
	LabelString           "Fact."
	TheoremName           fact*
	TheoremCounter        none
	TheoremLaTeXName      factname
	LangPreamble
	  \providecommand{\factname}{_(Fact)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\factname}{_(Fact)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='fact'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Definition*
	CopyStyle             Theorem*
	LatexName             defn*
#LabelType             Static
	LabelString           "Definition."
	Font
	  Shape               Up
	EndFont
	LabelFont
	  Shape               Up
	  Series              Bold
	EndFont
	TheoremName           defn*
	TheoremStyle          definition
	TheoremCounter        none
	TheoremLaTeXName      definitionname
	LangPreamble
	  \providecommand{\definitionname}{_(Definition)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\definitionname}{_(Definition)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='definition'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Example*
	CopyStyle             Definition*
	LatexName             example*
	LabelString           "Example."
	TheoremName           example*
	TheoremStyle          definition
	TheoremCounter        none
	TheoremLaTeXName      examplename
	LangPreamble
	  \providecommand{\examplename}{_(Example)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\examplename}{_(Example)}}
	EndBabelPreamble
	DocBookWrapperTag     example
	DocBookTag            para
	DocBookAttr           ""
End


Style Problem*
	CopyStyle             Definition*
	LatexName             problem*
	LabelString           "Problem."
	TheoremName           problem*
	TheoremStyle          definition
	TheoremCounter        none
	TheoremLaTeXName      problemname
	LangPreamble
	  \providecommand{\problemname}{_(Problem)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\problemname}{_(Problem)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='problem'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Exercise*
	CopyStyle             Definition*
	LatexName             xca*
	LabelString           "Exercise."
	TheoremName           xca*
	TheoremStyle          definition
	TheoremCounter        none
	TheoremLaTeXName      exercisename
	LangPreamble
	  \providecommand{\exercisename}{_(Exercise)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\exercisename}{_(Exercise)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='exercise'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Solution*
	CopyStyle             Definition*
	LatexName             sol*
	LabelString           "Solution."
	TheoremName           sol*
	TheoremStyle          definition
	TheoremCounter        none
	TheoremLaTeXName      solutionname
	LangPreamble
	  \providecommand{\solutionname}{_(Solution)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\solutionname}{_(Solution)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='solution'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Remark*
	CopyStyle             Theorem*
	LatexName             rem*
	LabelString           "Remark."
	Font
	  Shape               Up
	  Size                Normal
	EndFont
	LabelFont
	  Series              Medium
	  Shape               Italic
	EndFont
	TheoremName           rem*
	TheoremStyle          remark
	TheoremCounter        none
	TheoremLaTeXName      remarkname
	LangPreamble
	  \providecommand{\remarkname}{_(Remark)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\remarkname}{_(Remark)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='remark'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Claim*
	CopyStyle             Remark*
	LatexName             claim*
	LabelString           "Claim."
	TheoremName           claim*
	TheoremStyle          remark
	TheoremCounter        none
	TheoremLaTeXName      claimname
	LangPreamble
	  \providecommand{\claimname}{_(Claim)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\claimname}{_(Claim)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='claim'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End

