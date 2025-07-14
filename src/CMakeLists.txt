# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.
#
# Copyright (c) 2006-2011 Peter Kümmel, <syntheticpp@gmx.net>
#

project(${_lyx})

include_directories(${TOP_SRC_DIR}/src)

handle_warning_options("SRC" "CXX" -Wno-nested-anon-types -Wno-deprecated-declarations -Wno-reserved-macro-identifier)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

add_subdirectory(frontends)
add_subdirectory(graphics)
add_subdirectory(insets)
add_subdirectory(mathed)
add_subdirectory(support)
add_subdirectory(tex2lyx)
if (APPLE)
  add_subdirectory(convert)
endif()
if (UNIX)
	add_subdirectory(client)
endif()

file(GLOB lyx_sources ${TOP_SRC_DIR}/src/${LYX_CPP_FILES})
file(GLOB moc_files ${TOP_SRC_DIR}/src/${LYX_MOC_FILES})
list(REMOVE_ITEM  lyx_sources ${moc_files} .)
file(GLOB lyx_headers ${TOP_SRC_DIR}/src/${LYX_HPP_FILES})
set(lyxwrap_sources ${TOP_SRC_DIR}/src/lyxwrap.cpp)

list(REMOVE_ITEM lyx_sources
	${TOP_SRC_DIR}/src/lyxwrap.cpp
	${TOP_SRC_DIR}/src/Variables.cpp
	${TOP_SRC_DIR}/src/Section.cpp
	${TOP_SRC_DIR}/src/lyxcore.cpp
	${TOP_SRC_DIR}/src/lyxinsets.cpp
	${TOP_SRC_DIR}/src/lyxmathed.cpp)

# -Wno-error needed, because gcc seems to ignore -Wno-missing-noreturn
handle_source_option("CXX" "${TOP_SRC_DIR}/src/LyX.cpp" -Wno-invalid-noreturn -Wno-missing-noreturn -Wno-error)
handle_source_option("CXX" ${TOP_SRC_DIR}/src/TextMetrics.cpp -Wno-implicit-int-float-conversion)
handle_source_option("CXX" ${TOP_SRC_DIR}/src/CursorSlice.cpp -Wno-null-dereference)

foreach(_base BiblioInfo Buffer BufferEncodings BufferList BufferParams BufferView
	Changes Compare Converter Counters Cursor CursorSlice CutAndPaste
	DocIterator Font FontInfo FontList Format InsetIterator
	InsetList Language LaTeX LaTeXFeatures Layout LayoutFile LayoutModuleList LyX LyXRC
	MetricsInfo PDFOptions ParIterator Paragraph ParagraphMetrics ParagraphParameters
	Row RowPainter Statistics TexRow Text TextClass TextMetrics TocBackend TocBuilder Trans
	Undo VSpace factory lyxfind output_docbook output_latex output_plaintext output_xhtml
	xml) 
  handle_source_option("CXX" ${TOP_SRC_DIR}/src/${_base}.cpp -Wno-shadow)
endforeach()
if (${LYX_DEBUG_SANITIZE} MATCHES "ADDRESS")
  foreach(_base BiblioInfo Encoding LaTeX LaTeXFeatures LayoutFile Text VCBackend lyxfind Preamble)
    handle_source_option("CXX" ${TOP_SRC_DIR}/src/${_base}.cpp -Wno-maybe-uninitialized)
  endforeach()
endif()
if (${LYX_DEBUG_SANITIZE} MATCHES "UNSPECIFIED")
  handle_source_option("CXX" ${TOP_SRC_DIR}/src/TexRow.cpp -Wno-array-bounds -Wno-stringop-overread)
endif()
handle_source_option("CXX" ${TOP_SRC_DIR}/src/Statistics.cpp -Wno-null-dereference)
foreach(_base Buffer BufferList BufferParams BufferView ConverterCache CutAndPaste
	LaTeX Text TextClass factory output_docbook output_latex output_plaintext output_xhtml)
  #handle_source_option("CXX" ${TOP_SRC_DIR}/src/${_base}.cpp -Wno-deprecated-copy-with-user-provided-dtor)
endforeach()
foreach(_base Paragraph xml)
  #handle_source_option("CXX" ${TOP_SRC_DIR}/src/${_base}.cpp -Wno-deprecated-copy-with-dtor)
endforeach()
foreach(_base LayoutFile Paragraph xml)
  #handle_source_option("CXX" ${TOP_SRC_DIR}/src/${_base}.cpp -Wno-delete-non-abstract-non-virtual-dtor)
endforeach()
list(REMOVE_ITEM lyx_headers
	${TOP_SRC_DIR}/src/Variables.h
	${TOP_SRC_DIR}/src/Section.h)

if(NOT APPLE)
	list(REMOVE_ITEM lyx_sources ${TOP_SRC_DIR}/src/AppleSpellChecker.cpp)
	list(REMOVE_ITEM lyx_headers ${TOP_SRC_DIR}/src/AppleSpellChecker.h)
endif()

if(NOT ASPELL_FOUND)
	list(REMOVE_ITEM lyx_sources ${TOP_SRC_DIR}/src/AspellChecker.cpp)
	list(REMOVE_ITEM lyx_headers ${TOP_SRC_DIR}/src/AspellChecker.h)
else()
	set(dont_merge ${dont_merge} ${TOP_SRC_DIR}/src/AspellChecker.cpp)
endif()

if(NOT ENCHANT_FOUND)
	list(REMOVE_ITEM lyx_sources ${TOP_SRC_DIR}/src/EnchantChecker.cpp)
	list(REMOVE_ITEM lyx_headers ${TOP_SRC_DIR}/src/EnchantChecker.h)
else()
	set(dont_merge ${dont_merge} ${TOP_SRC_DIR}/src/EnchantChecker.cpp)
endif()

if(NOT HUNSPELL_FOUND)
	list(REMOVE_ITEM lyx_sources ${TOP_SRC_DIR}/src/HunspellChecker.cpp)
	list(REMOVE_ITEM lyx_headers ${TOP_SRC_DIR}/src/HunspellChecker.h)
else()
	set(dont_merge ${dont_merge} ${TOP_SRC_DIR}/src/HunspellChecker.cpp)
endif()


lyx_automoc(${TOP_SRC_DIR}/src/Compare.cpp)
if(WIN32)
	lyx_automoc(${TOP_SRC_DIR}/src/Server.cpp)
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR}
	${ZLIB_INCLUDE_DIR} ${ICONV_INCLUDE_DIR} ${QT_INCLUDES})

lyx_add_msvc_pch(lyx)

if (LYX_MERGE_FILES)
	if(dont_merge)
		list(REMOVE_ITEM lyx_sources ${dont_merge})
	endif()
	lyx_const_touched_files(_allinone lyx_sources)
	set(lyx_sources ${_allinone_files} ${dont_merge})
endif()

if (LYX_VLD)
	configure_file(${vld_path}/vld.ini ${CMAKE_CURRENT_BINARY_DIR}/vld.ini COPYONLY)
	set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/memory_leak_report.txt PROPERTIES GENERATED TRUE)
	set(vld_files ${CMAKE_CURRENT_BINARY_DIR}/vld.ini ${CMAKE_CURRENT_BINARY_DIR}/memory_leak_report.txt)
	lyx_add_info_files(VLD ${vld_files})
endif()

if(WIN32 AND NOT MINGW)
  set(FILE_RC ${TOP_SRC_DIR}/src/lyxwinres.rc)
  file(COPY ${TOP_SRC_DIR}/development/Win32/packaging/icons/lyx.ico DESTINATION ${CMAKE_BINARY_DIR})
  file(COPY ${TOP_SRC_DIR}/development/Win32/packaging/icons/lyx_doc.ico DESTINATION ${CMAKE_BINARY_DIR})
  message(STATUS "Using icon defined in resource file: ${FILE_RC}")
endif()

lyx_find_info_files(LyXInfoFiles ${TOP_SRC_DIR}/*)
lyx_find_info_files(LyXCMakeFiles ${TOP_CMAKE_PATH}/*.txt)
lyx_find_info_files(LyXCMakeFiles ${TOP_CMAKE_PATH}/*.cmake)
lyx_find_info_files(LyXCMakeFiles ${TOP_CMAKE_PATH}/*.h)
lyx_find_info_files(LyXCMakeFiles ${TOP_CMAKE_PATH}/*.msvc)
lyx_find_info_files(LyXCMakeFiles ${TOP_MODULE_PATH}/*)
lyx_find_info_files(LyXCMakeFiles ${TOP_SCRIPT_PATH}/*)
lyx_find_info_files(LyXUiFiles ${TOP_SRC_DIR}/lib/ui/*)

add_executable(${_lyx}
	${WIN32_CONSOLE}
	${LYX_BUILD_BUNDLE}
	${lyx_sources}
	${lyx_headers}
	${vld_files}
	${FILE_RC}
	${lyx_info_files}
	${lyx_cmake_files}
	${OSX_BUNDLE_FILES}
)

add_dependencies(${_lyx} lyx_version)

set_target_properties(${_lyx} PROPERTIES
  QT_NO_UNICODE_DEFINES TRUE
	FOLDER "applications/LyX"
	INTERPROCEDURAL_OPTIMIZATION ${LYX_IPO_SUPPORTED})
lyx_add_gcc_pch(${_lyx})

target_link_libraries(${_lyx}
	mathed
	insets
	frontends
	frontend_qt
	graphics
	support
	${MYTHESLIB_LIBRARY}
	${ICONV_LIBRARY}
	${LYX_QTMAIN_LIBRARY}
	${vld_dll})

qt_use_modules(${_lyx} Core Gui ${QtCore5CompatModule})

if(QT_USES_X11)
  find_package(X11 REQUIRED)
  target_link_libraries(${_lyx} ${X11_X11_LIB}) # we need it because we use XSendEvent
  if(HAVE_QT5_X11_EXTRAS)
    if (XCB_LIBRARY MATCHES "NOTFOUND")
      message(FATAL_ERROR "Need xcb library to use with QT5_X11_EXTRAS")
    endif()
    target_link_libraries(${_lyx} ${LYX_QT5_X11_EXTRAS_LIBRARY} ${XCB_LIBRARY})
  endif()
endif()

lyx_target_link_libraries(${_lyx} HUNSPELL ASPELL ENCHANT Magic)

if(APPLE)
	target_link_libraries(${_lyx} "-bind_at_load")
	if(LYX_COCOA)
		target_link_libraries(${_lyx} "-framework Cocoa")
	else()
		target_link_libraries(${_lyx} "-framework AppKit")
	endif()
endif()

if(MINGW)
	target_link_libraries(${_lyx} ole32)
endif()

if(CYGWIN)
	target_link_libraries(${_lyx} gdi32 shlwapi ole32)
endif()

project_source_group("${GROUP_CODE}" lyx_sources lyx_headers)

install(TARGETS ${_lyx}
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION ${LYX_UTILITIES_INSTALL_PATH} COMPONENT Runtime)

if(LYX_BUNDLE)
	if(APPLE)
		# Now that lyx executable is defined, we can the files that should be included
		# directly in the bundle. Note that resources are not copied with this mechanism:
		# only the files necessary to run the executable are processed here (LyX.sdef, LyX.icns, etc.)
		list(LENGTH OSX_BUNDLE_FILES OSX_BUNDLE_LAST_INDEX)
		math(EXPR OSX_BUNDLE_LAST_INDEX "${OSX_BUNDLE_LAST_INDEX} - 1")
		foreach(_i RANGE ${OSX_BUNDLE_LAST_INDEX})
		    list(GET OSX_BUNDLE_FILES ${_i} _file)
		    list(GET OSX_BUNDLE_DIRS ${_i} _directory)
		    # Sets the location of the file into the macOS bundle
			set_source_files_properties("${_file}" PROPERTIES MACOSX_PACKAGE_LOCATION "${_directory}")
		endforeach()

		set_target_properties(${_lyx} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/../Info.plist")
	endif()
endif()

set(dowrapper ON) # create and install wrapper for external commands
foreach(_f ${lyxwrap_sources})
  if(NOT EXISTS "${_f}")
    set(dowrapper OFF)
  endif()
endforeach()
if(dowrapper AND USE_POSIX_PACKAGING)
  add_executable(lyxwrap${PROGRAM_SUFFIX} ${lyxwrap_sources})
  install(TARGETS lyxwrap${PROGRAM_SUFFIX} DESTINATION ${LYX_UTILITIES_INSTALL_PATH})
endif()

add_subdirectory(tests)
