// -*- C++ -*-
/**
 * \file MathFactory.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef MATH_FACTORY_H
#define MATH_FACTORY_H

#include "InsetMath.h"
#include "MathParser.h"

#include <map>

namespace lyx {

class MathAtom;
class MathData;


MathAtom createInsetMath(docstring const &, Buffer * buf);
MathAtom createInsetMath(char const * const, Buffer * buf);

/** Fills md with the contents of str.
 *  str is created by the frontend dialog's and returned to the LyX core.
 *  The function returns true if successful.
 */
bool createInsetMath_fromDialogStr(docstring const &, MathData &);

typedef std::map<docstring, latexkeys> MathWordList;
MathWordList const & mathedWordList();

typedef std::map<docstring, UnicodeVariants> MathVariantList;
MathVariantList const & mathedVariantList();

typedef std::map<HullType, std::vector<docstring>> MathConflictList;
MathConflictList const & mathedConflictList();
std::vector<docstring> const & mathedConflictList(HullType hull);

std::vector<HullType> const & mathedConflictEnvs();

} // namespace lyx

#endif
