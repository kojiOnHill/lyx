/**
 * \file dummy_impl.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jean-Marc Lasgouttes
 *
 * Full author contact details are available in file CREDITS.
 */

/**
 * This file contains dummy implementation of some methods that are
 * needed by classes used by tex2lyx. This allows to reduce the number
 * of classes we have to link against.
*/

// {[(

#include <config.h>

#include "tex2lyx.h"
#include "LaTeXColors.h"
#include "LaTeXFeatures.h"
#include "LyXRC.h"
#include "xml.h"

#include "support/Messages.h"

#include <iostream>

using namespace std;

namespace lyx {

//
// Dummy Alert support (needed by TextClass)
//


namespace frontend {
namespace Alert {
	void warning(docstring const & title, docstring const & message, bool)
	{
		warning_message(to_utf8(title) + "\n" + to_utf8(message));
	}
} // namespace Alert
} // namespace frontend


//
// Required global variables
//

bool verbose = false;
LyXRC lyxrc;


// Make linker happy

LaTeXColors & theLaTeXColors()
{
	LaTeXColors * lc = new LaTeXColors;
	return * lc;
}


//
// Dummy translation support (needed at many places)
//


Messages messages_;
Messages const & getMessages(string const &)
{
	return messages_;
}


Messages const & getGuiMessages()
{
	return messages_;
}


//
// Dummy features support (needed by ModuleList)
//


bool LaTeXFeatures::isAvailable(string const &)
{
	return true;
}


string alignmentToCSS(LyXAlignment)
{
	return string();
}


//
// Keep the linker happy on Windows
//

void lyx_exit(int)
{abort();}

namespace xml {
docstring StartTag::writeTag() const { return docstring(); }
docstring StartTag::writeEndTag() const { return docstring(); }
}

} // namespace lyx
