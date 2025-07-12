# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.
#
# Copyright (c) 2006-2011 Peter Kümmel, <syntheticpp@gmx.net>
#

project(insets)

file(GLOB insets_sources ${TOP_SRC_DIR}/src/insets/${LYX_CPP_FILES})
file(GLOB insets_headers ${TOP_SRC_DIR}/src/insets/${LYX_HPP_FILES})

list(REMOVE_ITEM insets_sources ${TOP_SRC_DIR}/src/insets/InsetTheorem.cpp)
if (${LYX_DEBUG_SANITIZE} MATCHES "ADDRESS")
  foreach(_base ExternalTransforms InsetBibtex InsetERT InsetListings)
    handle_source_option("CXX" ${TOP_SRC_DIR}/src/insets/${_base}.cpp -Wno-maybe-uninitialized)
  endforeach()
  foreach(_base Inset InsetTabular)
    handle_source_option("CXX" ${TOP_SRC_DIR}/src/insets/${_base}.cpp -Wno-null-dereference)
  endforeach()
endif()
foreach(_base InsetExternal ExternalSupport Inset InsetArgument
	InsetBibitem InsetBibtex InsetBox InsetBranch
	InsetCaption InsetCaptionable InsetCitation InsetCollapsible InsetCommand InsetCommandParams InsetCounter
	InsetERT InsetFlex InsetFloat InsetFloatList InsetFoot InsetFootlike
	InsetGraphics InsetHyperlink InsetInfo InsetIPA InsetIPAMacro InsetInclude InsetIndex InsetIndexMacro
	InsetLabel InsetLayout InsetLine InsetListings InsetListingsParams InsetMarginal
	InsetNewline InsetNewpage InsetNomencl InsetNote
	InsetPhantom InsetPreview InsetQuotes InsetRef InsetScript InsetSeparator InsetSpace InsetSpecialChar
	InsetTOC InsetTabular InsetText InsetVSpace InsetWrap
	RenderButton RenderGraphic RenderPreview)
  handle_source_option("CXX" ${TOP_SRC_DIR}/src/insets/${_base}.cpp -Wno-shadow)
endforeach()
foreach(_base InsetArgument InsetBibitem InsetBox InsetBranch InsetCaption InsetCitation
	InsetCommand InsetCommandParams InsetFloat InsetFoot InsetIPA InsetIPAMacro InsetInclude InsetIndex
	InsetIndexMacro InsetLabel InsetListings InsetNomencl InsetNote InsetPhantom InsetScript
	InsetTOC InsetTabular InsetText InsetWrap)
  #handle_source_option("CXX" ${TOP_SRC_DIR}/src/insets/${_base}.cpp -Wno-deprecated-copy-with-user-provided-dtor)
endforeach()
foreach(_base Inset InsetTabular)
  handle_source_option("CXX" ${TOP_SRC_DIR}/src/insets/${_base}.cpp -Wno-null-dereference)
endforeach()

lyx_add_msvc_pch(insets)

include_directories(${TOP_SRC_DIR}/src/insets ${QT_INCLUDES})


if(NOT LYX_MERGE_FILES)
	add_library(insets ${library_type} ${insets_sources} ${insets_headers})
else()
	lyx_const_touched_files(_allinone  insets_sources)
	add_library(insets ${library_type} ${_allinone_files})
endif()
set_target_properties(insets PROPERTIES FOLDER "applications/LyX" QT_NO_UNICODE_DEFINES TRUE)

qt_use_modules(insets Gui)

lyx_add_gcc_pch(insets)

project_source_group("${GROUP_CODE}" insets_sources insets_headers)

