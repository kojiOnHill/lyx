# Original Author : David L. Johnson <dlj0@lehigh.edu>
# Probably broken by Jean-Marc Lasgouttes <Jean-Marc.Lasgouttes@inria.fr>
# modified and modularized by Emmanuel GUREGHIAN <gureghia@boston.bertin.fr>
# Tinkered with Sep. '07 by Paul Rubin <rubin@msu.edu>
# Modularized Jan 08 by Richard Kimberly Heck <rikiheck@lyx.org>
# Hacked June '09 by Paul Rubin <rubin@msu.edu> to use separate counters
# Chapter dependence added Oct '16 by Andrew Parsloe <aparsloe@clear.net.nz>

# The environments defined are :
# - Theorem
# - Corollary
# - Lemma
# - Proposition
# - Conjecture
# - Fact
# - Definition
# - Example
# - Problem
# - Exercise
# - Solution
# - Remark
# - Claim
# - Case (by inclusion)


Format 107

Input theorems-counters-bytype.inc

Counter theorem
	Within chapter
End
Counter corollary
	Within chapter
End
Counter lemma
	Within chapter
End
Counter proposition
	Within chapter
End
Counter conjecture
	Within chapter
End
Counter fact
	Within chapter
End
Counter definition
	Within chapter
End
Counter example
	Within chapter
End
Counter problem
	Within chapter
End
Counter exercise
	Within chapter
End
Counter solution
	Within chapter
End
Counter remark
	Within chapter
End
Counter claim
	Within chapter
End

OutlinerName thm "Definitions & Theorems"

Style Theorem
	Category              Reasoning
	Margin                First_Dynamic
	LatexType             Environment
	LatexName             thm
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
	LabelType             Static
	LabelCounter          theorem
	LabelString           "Theorem \thetheorem."
	Font
	  Shape               Italic
	  Size                Normal
	EndFont
	LabelFont
	  Shape               Up
	  Series              Bold
	EndFont
	Preamble
		\theoremstyle{plain}
    \ifx\thechapter\undefined
	    \newtheorem{thm}{\protect\theoremname}
	  \else
      \newtheorem{thm}{\protect\theoremname}[chapter]
    \fi
	EndPreamble
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


Style Corollary
	CopyStyle             Theorem
	LatexName             cor
	LabelString           "Corollary \thecorollary."
	Preamble
	  \theoremstyle{plain}
    \ifx\thechapter\undefined
	    \newtheorem{cor}{\protect\corollaryname}
	  \else
      \newtheorem{cor}{\protect\corollaryname}[chapter]
    \fi
	EndPreamble
	LabelCounter          corollary
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


Style Lemma
	CopyStyle             Theorem
	LatexName             lem
	LabelString           "Lemma \thelemma."
	Preamble
	  \theoremstyle{plain}
    \ifx\thechapter\undefined
      \newtheorem{lem}{\protect\lemmaname}
    \else
      \newtheorem{lem}{\protect\lemmaname}[chapter]
    \fi
	EndPreamble
	LangPreamble
	  \providecommand{\lemmaname}{_(Lemma)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\lemmaname}{_(Lemma)}}
	EndBabelPreamble
	LabelCounter          lemma
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='lemma'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Proposition
	CopyStyle             Theorem
	LatexName             prop
	LabelString           "Proposition \theproposition."
	Preamble
	  \theoremstyle{plain}
    \ifx\thechapter\undefined
      \newtheorem{prop}{\protect\propositionname}
    \else
      \newtheorem{prop}{\protect\propositionname}[chapter]
    \fi
	EndPreamble
	LangPreamble
	  \providecommand{\propositionname}{_(Proposition)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\propositionname}{_(Proposition)}}
	EndBabelPreamble
	LabelCounter          proposition
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='proposition'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Conjecture
	CopyStyle             Theorem
	LatexName             conjecture
	LabelString           "Conjecture \theconjecture."
	Preamble
	  \theoremstyle{plain}
    \ifx\thechapter\undefined
      \newtheorem{conjecture}{\protect\conjecturename}
    \else
      \newtheorem{conjecture}{\protect\conjecturename}[chapter]
    \fi
	EndPreamble
	LangPreamble
	  \providecommand{\conjecturename}{_(Conjecture)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\conjecturename}{_(Conjecture)}}
	EndBabelPreamble
	LabelCounter          conjecture
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='conjecture'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Fact
	CopyStyle             Theorem
	LatexName             fact
	LabelString           "Fact \thefact."
	Preamble
	  \theoremstyle{plain}
    \ifx\thechapter\undefined
      \newtheorem{fact}{\protect\factname}
    \else
      \newtheorem{fact}{\protect\factname}[chapter]
    \fi
	EndPreamble
	LangPreamble
	  \providecommand{\factname}{_(Fact)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\factname}{_(Fact)}}
	EndBabelPreamble
	LabelCounter          fact
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='fact'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Definition
	CopyStyle             Theorem
	LatexName             defn
	LabelString           "Definition \thedefinition."
	Font
	  Shape               Up
	EndFont
	LabelFont
	  Shape               Up
	  Series              Bold
	EndFont
	Preamble
	  \theoremstyle{definition}
    \ifx\thechapter\undefined
      \newtheorem{defn}{\protect\definitionname}
    \else
      \newtheorem{defn}{\protect\definitionname}[chapter]
    \fi
	EndPreamble
	LangPreamble
	  \providecommand{\definitionname}{_(Definition)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\definitionname}{_(Definition)}}
	EndBabelPreamble
	LabelCounter          definition
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='definition'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Example
	CopyStyle             Definition
	LatexName             example
	LabelString           "Example \theexample."
	Preamble
	  \theoremstyle{definition}
    \ifx\thechapter\undefined
      \newtheorem{example}{\protect\examplename}
    \else
      \newtheorem{example}{\protect\examplename}[chapter]
    \fi
	EndPreamble
	Requires              amsthm
	LangPreamble
	  \providecommand{\examplename}{_(Example)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\examplename}{_(Example)}}
	EndBabelPreamble
	LabelCounter          example
	DocBookWrapperTag     informalexample
	DocBookTag            para
	DocBookAttr           ""
End


Style Problem
	CopyStyle             Definition
	LatexName             problem
	LabelString           "Problem \theproblem."
	Preamble
	  \theoremstyle{definition}
    \ifx\thechapter\undefined
      \newtheorem{problem}{\protect\problemname}
    \else
      \newtheorem{problem}{\protect\problemname}[chapter]
    \fi
	EndPreamble
	Requires              amsthm
	LangPreamble
	  \providecommand{\problemname}{_(Problem)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\problemname}{_(Problem)}}
	EndBabelPreamble
	LabelCounter          problem
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='problem'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Exercise
	CopyStyle             Definition
	LatexName             xca
	LabelString           "Exercise \theexercise."
	Preamble
	  \theoremstyle{definition}
    \ifx\thechapter\undefined
      \newtheorem{xca}{\protect\exercisename}
    \else
      \newtheorem{xca}{\protect\exercisename}[chapter]
    \fi
	EndPreamble
	Requires              amsthm
	LangPreamble
	  \providecommand{\exercisename}{_(Exercise)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\exercisename}{_(Exercise)}}
	EndBabelPreamble
	LabelCounter          exercise
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='exercise'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Solution
	CopyStyle             Definition
	LatexName             sol
	LabelString           "Solution \thesolution."
	Preamble
	  \theoremstyle{definition}
    \ifx\thechapter\undefined
      \newtheorem{sol}{\protect\solutionname}
    \else
      \newtheorem{sol}{\protect\solutionname}[chapter]
    \fi
	EndPreamble
	Requires              amsthm
	LangPreamble
		\providecommand{\solutionname}{_(Solution)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\solutionname}{_(Solution)}}
	EndBabelPreamble
	LabelCounter          solution
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='solution'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Remark
	CopyStyle             Theorem
	LatexName             rem
	LabelString           "Remark \theremark."
	Font
	  Shape               Up
	  Size                Normal
	EndFont
	LabelFont
	  Series              Medium
	  Shape               Italic
	EndFont
	Preamble
	  \theoremstyle{remark}
    \ifx\thechapter\undefined
      \newtheorem{rem}{\protect\remarkname}
    \else
      \newtheorem{rem}{\protect\remarkname}[chapter]
    \fi
	EndPreamble
	LangPreamble
	  \providecommand{\remarkname}{_(Remark)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\remarkname}{_(Remark)}}
	EndBabelPreamble
	LabelCounter          remark
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='remark'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Claim
	CopyStyle             Remark
	LatexName             claim
	LabelString           "Claim \theclaim."
	Preamble
	  \theoremstyle{remark}
    \ifx\thechapter\undefined
      \newtheorem{claim}{\protect\claimname}
    \else
      \newtheorem{claim}{\protect\claimname}[chapter]
    \fi
	EndPreamble
	Requires              amsthm
	LangPreamble
	  \providecommand{\claimname}{_(Claim)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\claimname}{_(Claim)}}
	EndBabelPreamble
	LabelCounter          claim
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='claim'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Input theorems-case.inc
Input theorems-refprefix.inc
