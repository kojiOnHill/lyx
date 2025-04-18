# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.
#
# Copyright (c) 2006, Peter K�mmel, <syntheticpp@gmx.net>
#

include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckTypeSize)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
include(MacroBoolTo01)
include(TestBigEndian)

test_big_endian(WORDS_BIGENDIAN)

#check_include_file_cxx(istream HAVE_ISTREAM)
#check_include_file_cxx(ostream HAVE_OSTREAM)
#check_include_file_cxx(sstream HAVE_SSTREAM)
#check_include_file_cxx(ios HAVE_IOS)
#check_include_file_cxx(locale HAVE_LOCALE)

# defines will be written to configIncludes.h
set(Include_Defines)
foreach(_h_file aspell.h aspell/aspell.h limits.h locale.h
	stdlib.h sys/stat.h sys/time.h sys/types.h sys/utime.h
	sys/socket.h unistd.h inttypes.h utime.h string.h argz.h xcb/xcb.h)
	string(REGEX REPLACE "[/\\.]" "_" _hf ${_h_file})
	string(TOUPPER ${_hf} _HF)
	check_include_files(${_h_file} HAVE_${_HF})
	set(Include_Defines "${Include_Defines}#cmakedefine HAVE_${_HF} 1\n")
endforeach()
check_include_file_cxx(regex HAVE_REGEX)
set(Include_Defines "${Include_Defines}#cmakedefine HAVE_REGEX 1\n")
configure_file(${LYX_CMAKE_DIR}/configIncludes.cmake ${TOP_BINARY_DIR}/configIncludes.h.cmake)
configure_file(${TOP_BINARY_DIR}/configIncludes.h.cmake ${TOP_BINARY_DIR}/configIncludes.h)

# defines will be written to configFunctions.h
set(Function_Defines)
foreach(_f alloca __argz_count __argz_next __argz_stringify
	chmod close _close dcgettext fcntl fork __fsetlocking
	getcwd getegid getgid getpid _getpid gettext getuid lstat lockf mempcpy mkdir _mkdir
	mkfifo open _open pclose _pclose popen _popen putenv readlink
	setenv setlocale strcasecmp stpcpy strdup strerror strtoul tsearch unsetenv wcslen)
  string(TOUPPER ${_f} _UF)
  check_function_exists(${_f} HAVE_${_UF})
  set(Function_Defines "${Function_Defines}#cmakedefine HAVE_${_UF} 1\n")
endforeach()
configure_file(${LYX_CMAKE_DIR}/configFunctions.cmake ${TOP_BINARY_DIR}/configFunctions.h.cmake)
configure_file(${TOP_BINARY_DIR}/configFunctions.h.cmake ${TOP_BINARY_DIR}/configFunctions.h)

check_symbol_exists(alloca "malloc.h" HAVE_SYMBOL_ALLOCA)
check_symbol_exists(asprintf "stdio.h" HAVE_ASPRINTF)
check_symbol_exists(wprintf "stdio.h" HAVE_WPRINTF)
check_symbol_exists(snprintf "stdio.h" HAVE_SNPRINTF)
check_symbol_exists(printf "stdio.h" HAVE_POSIX_PRINTF)
check_symbol_exists(pid_t "sys/types.h" HAVE_PID_T)
check_symbol_exists(intmax_t "inttypes.h" HAVE_INTTYPES_H_WITH_UINTMAX)
check_symbol_exists(uintmax_t "stdint.h" HAVE_STDINT_H_WITH_UINTMAX)
check_symbol_exists(LC_MESSAGES "locale.h" HAVE_LC_MESSAGES)
check_symbol_exists(PATH_MAX "limits.h" HAVE_DEF_PATH_MAX)

check_type_size(intmax_t HAVE_INTMAX_T)
macro_bool_to_01(HAVE_UINTMAX_T HAVE_STDINT_H_WITH_UINTMAX)

check_type_size("long double"  HAVE_LONG_DOUBLE)
check_type_size("long long"  HAVE_LONG_LONG_INT)
check_type_size(wchar_t HAVE_WCHAR_T)
check_type_size(wint_t  HAVE_WINT_T)

if(HUNSPELL_FOUND)
  # check whether hunspell C++ (rather than C) ABI is provided
  set(HunspellTestFile "${CMAKE_BINARY_DIR}/hunspelltest.cpp")
  file(WRITE "${HunspellTestFile}"
  "
  #include <hunspell.hxx>

  int main()
  {
    Hunspell sp(\"foo\", \"bar\");
    int i = sp.stem(\"test\").size();
  return(0);
  }
  "
  )

# The trick with faking the link command (see the else block) does not work
# with XCode (350a9daf).
if(APPLE OR LYX_EXTERNAL_HUNSPELL)
  try_compile(HAVE_HUNSPELL_CXXABI
    "${CMAKE_BINARY_DIR}"
    "${HunspellTestFile}"
    CMAKE_FLAGS
      "-DINCLUDE_DIRECTORIES:STRING=${HUNSPELL_INCLUDE_DIR}"
    LINK_LIBRARIES ${HUNSPELL_LIBRARY}
    OUTPUT_VARIABLE  LOG2)
else()
  try_compile(HAVE_HUNSPELL_CXXABI
    "${CMAKE_BINARY_DIR}"
    "${HunspellTestFile}"
    CMAKE_FLAGS
      "-DINCLUDE_DIRECTORIES:STRING=${HUNSPELL_INCLUDE_DIR}"
      # At this point, ../lib/libhunspell.a has not been built so we
      # cannot complete the linking.
      "-DCMAKE_CXX_LINK_EXECUTABLE='${CMAKE_COMMAD} echo dummy (fake) link command since libhunspell.a not built yet.'"
    OUTPUT_VARIABLE  LOG2)
endif()

  message(STATUS "HAVE_HUNSPELL_CXXABI = ${HAVE_HUNSPELL_CXXABI}")
  #message(STATUS "LOG2 = ${LOG2}")
  if(LYX_EXTERNAL_HUNSPELL AND (LYX_STDLIB_DEBUG OR LYX_DEBUG_GLIBC OR LYX_DEBUG_GLIBC_PEDANTIC) AND HAVE_HUNSPELL_CXXABI)
    message(FATAL_ERROR "Compiling LyX with stdlib-debug and system hunspell libraries may lead to crashes. Consider using '-DLYX_STDLIB_DEBUG=OFF -DLYX_DEBUG_GLIBC=OFF -DLYX_DEBUG_GLIBC_PEDANTIC=OFF' or -DLYX_EXTERNAL_HUNSPELL=OFF.")
  endif()
endif()

#check_cxx_source_compiles(
#	"
#	#include <algorithm>
#	using std::count;
#	int countChar(char * b, char * e, char const c)
#	{
#		return count(b, e, c);
#	}
#	int main(){return 0;}
#	"
#HAVE_STD_COUNT)

#check_cxx_source_compiles(
#	"
#	#include <cctype>
#	using std::tolower;
#	int main(){return 0;}
#	"
#CXX_GLOBAL_CSTD)

check_cxx_source_compiles(
	"
	#include <iconv.h>
	// this declaration will fail when there already exists a non const char** version which returns size_t
	double iconv(iconv_t cd,  char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);
	int main() { return 0; }
	"
HAVE_ICONV_CONST)

check_cxx_source_compiles(
	"
	int i[ ( sizeof(wchar_t)==2 ? 1 : -1 ) ];
	int main(){return 0;}
	"
SIZEOF_WCHAR_T_IS_2)

check_cxx_source_compiles(
	"
	int i[ ( sizeof(wchar_t)==4 ? 1 : -1 ) ];
	int main(){return 0;}
	"
SIZEOF_WCHAR_T_IS_4)

check_cxx_source_compiles(
	"
	int i[ ( sizeof(long long)>sizeof(long) ? 1 : -1 ) ];
	int main(){return 0;}
	"
SIZEOF_LONG_LONG_GREATER_THAN_SIZEOF_LONG)

if(LYX_DISABLE_CALLSTACK_PRINTING)
  set(LYX_CALLSTACK_PRINTING OFF CACHE BOOL "Print callstack when crashing")
else()
  check_cxx_source_compiles(
	"
	#include <execinfo.h>
	#include <cxxabi.h>
	int main() {
		void* array[200];
		size_t size = backtrace(array, 200);
		backtrace_symbols(array, size);
		int status = 0;
		abi::__cxa_demangle(\"abcd\", 0, 0, &status);
	}
	"
  LYX_CALLSTACK_PRINTING)
endif()

# Check whether STL is libstdc++ with C++11 ABI
check_cxx_source_compiles(
	"
	#include <vector>
	int main() {
	#if ! defined(_GLIBCXX_USE_CXX11_ABI) || ! _GLIBCXX_USE_CXX11_ABI
		this is not libstdc++ using the C++11 ABI
	#endif
		return(0);
	}
	"
USE_GLIBCXX_CXX11_ABI)

check_cxx_source_compiles(
	"
	#ifndef __clang__
		this is not clang
	#endif
	int main() {
	  return(0);
	}
	"
lyx_cv_prog_clang)

if (ENCHANT_FOUND)
  set(CMAKE_REQUIRED_INCLUDES ${ENCHANT_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${ENCHANT_LIBRARY})
  # Check, whether enchant is version 2.x at least
  check_cxx_source_compiles(
    "
    #include <enchant++.h>
    enchant::Broker broker;
    int main() {
      return(0);
    }
    "
  HAVE_ENCHANT2)
  if (HAVE_ENCHANT2)
    message(STATUS "ENCHANT2 found")
  endif()
endif()

set(USE_GLIBCXX_CXX11_ABI)

set(HAVE_QT5_X11_EXTRAS)
set(HAVE_QT6_X11_EXTRAS)
if (LYX_USE_QT MATCHES "QT5|QT6")
  if (LYX_USE_QT MATCHES "QT5")
    set(QtVal Qt5)
  else()
    set(QtVal Qt6)
  endif()
  set(CMAKE_REQUIRED_INCLUDES ${${QtVal}Core_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_FLAGS)
  #message(STATUS "${QtVal}Core_INCLUDE_DIRS = ${${QtVal}Core_INCLUDE_DIRS}")
  check_include_file_cxx(QtGui/qtgui-config.h HAVE_QTGUI_CONFIG_H)
  if (HAVE_QTGUI_CONFIG_H)
    set(lyx_qt_config "QtGui/qtgui-config.h")
  else()
    set(lyx_qt_config "QtCore/qconfig.h")
  endif()
  if(WIN32)
    set(QT_USES_X11 OFF CACHE BOOL "Win32 compiled without X11")
    # The try_run for minngw would not work here anyway
  else()
    check_cxx_source_runs(
      "
      #include <${lyx_qt_config}>
      #include <string>
      using namespace std;
      string a(QT_QPA_DEFAULT_PLATFORM_NAME);
      int main(int argc, char **argv)
      {
	if (a.compare(\"xcb\") == 0)
	  return(0);
	else
	  return 1;
      }
      "
      QT_USES_X11)
  endif()

  if (${QtVal}X11Extras_FOUND)
    get_target_property(_x11extra_prop ${QtVal}::X11Extras IMPORTED_CONFIGURATIONS)
    get_target_property(_x11extra_link_libraries ${QtVal}::X11Extras IMPORTED_LOCATION_${_x11extra_prop})
    set(CMAKE_REQUIRED_LIBRARIES ${_x11extra_link_libraries})
    set(CMAKE_REQUIRED_INCLUDES ${${QtVal}X11Extras_INCLUDE_DIRS})
    set(CMAKE_REQUIRED_FLAGS "${${QtVal}X11Extras_EXECUTABLE_COMPILE_FLAGS} -fPIC -DQT_NO_VERSION_TAGGING")
    #message(STATUS "CMAKE_REQUIRED_LIBRARIES = ${_x11extra_link_libraries}")
    #message(STATUS "CMAKE_REQUIRED_INCLUDES = ${${QtVal}X11Extras_INCLUDE_DIRS}")
    #message(STATUS "CMAKE_REQUIRED_FLAGS = ${CMAKE_REQUIRED_FLAGS}")
    check_cxx_source_compiles(
            "
            #include <QtX11Extras/QX11Info>
            int main()
            {
              bool isX11 = QX11Info::isPlatformX11();
            }
            "
      QT_HAS_X11_EXTRAS)
    string(TOUPPER ${QtVal} QTVAL)
    set(HAVE_${QTVAL}_X11_EXTRAS ${QT_HAS_X11_EXTRAS})
    set(LYX_${QTVAL}_X11_EXTRAS_LIBRARY ${_x11extra_link_libraries})
  endif()
  if (${QtVal}WinExtras_FOUND)
    get_target_property(_winextra_prop ${QtVal}::WinExtras IMPORTED_CONFIGURATIONS)
    string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
    get_target_property(_winextra_link_libraries ${QtVal}::WinExtras IMPORTED_LOCATION_${BUILD_TYPE})
    set(CMAKE_REQUIRED_LIBRARIES ${_winextra_link_libraries})
    set(CMAKE_REQUIRED_INCLUDES ${${QtVal}WinExtras_INCLUDE_DIRS})
    set(CMAKE_REQUIRED_FLAGS ${${QtVal}WinExtras_EXECUTABLE_COMPILE_FLAGS})
  endif()
else()
  message(FATAL_ERROR "Check for QT_USES_X11: Not handled LYX_USE_QT (= ${LYX_USE_QT})")
endif()
