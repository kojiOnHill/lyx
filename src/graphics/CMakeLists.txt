# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.
#
# Copyright (c) 2006-2011 Peter Kümmel, <syntheticpp@gmx.net>
#

project(graphics)

file(GLOB graphics_sources ${TOP_SRC_DIR}/src/graphics/${LYX_CPP_FILES})
file(GLOB graphics_headers ${TOP_SRC_DIR}/src/graphics/${LYX_HPP_FILES})

if (${LYX_DEBUG_SANITIZE} MATCHES "ADDRESS")
  handle_source_option("CXX" ${TOP_SRC_DIR}/src/graphics/epstools.cpp -Wno-maybe-uninitialized)
endif()
foreach(_base GraphicsLoader PreviewImage PreviewLoader)
  handle_source_option("CXX" ${TOP_SRC_DIR}/src/graphics/${_base}.cpp -Wno-shadow)
endforeach()
foreach(_base GraphicsConverter PreviewLoader)
  #handle_source_option("CXX" ${TOP_SRC_DIR}/src/graphics/${_base}.cpp -Wno-deprecated-copy-with-user-provided-dtor)
endforeach()
foreach(_base GraphicsCacheItem GraphicsLoader)
  handle_source_option("CXX" ${TOP_SRC_DIR}/src/graphics/${_base}.cpp -Wno-delete-abstract-non-virtual-dtor)
endforeach()
foreach(_base GraphicsConverter PreviewLoader)
  handle_source_option("CXX" ${TOP_SRC_DIR}/src/graphics/${_base}.cpp -Wno-delete-non-abstract-non-virtual-dtor)
endforeach()
lyx_add_msvc_pch(graphics)

include_directories(${TOP_SRC_DIR}/src/graphics)
lyx_automoc(${TOP_SRC_DIR}/src/graphics/PreviewLoader.cpp)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${QT_INCLUDES})

if(NOT LYX_MERGE_FILES)
	add_library(graphics ${library_type} ${graphics_sources} ${graphics_headers})
else()
	lyx_const_touched_files(_allinone  graphics_sources)
	add_library(graphics ${library_type} ${_allinone_files})
endif()
set_target_properties(graphics PROPERTIES FOLDER "applications/LyX" QT_NO_UNICODE_DEFINES TRUE)
qt_use_modules(graphics Core)

lyx_add_gcc_pch(graphics)

project_source_group("${GROUP_CODE}" graphics_sources graphics_headers)

