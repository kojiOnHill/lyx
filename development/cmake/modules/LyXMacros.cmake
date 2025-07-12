#
#  Copyright (c) 2006-2011 Peter K�mmel, <syntheticpp@gmx.net>
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#  1. Redistributions of source code must retain the copyright
#	  notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the copyright
#	  notice, this list of conditions and the following disclaimer in the
#	  documentation and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#	  derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
#  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
#  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
#  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
#  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
#  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

include (MacroAddFileDependencies)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS true)

macro(lyx_add_path _list _prefix)
	set(_tmp)
	foreach(_current ${${_list}})
		set(_tmp ${_tmp} ${_prefix}/${_current})
	endforeach(_current)
	set(${_list} ${_tmp})
endmacro(lyx_add_path _out _prefix)


#create the implementation files from the ui files and add them
#to the list of sources
#usage: LYX_ADD_UI_FILES(foo_SRCS ${ui_files})
macro(LYX_ADD_UI_FILES _sources _ui_files)
	set(uifiles})
	foreach (_current_FILE ${ARGN})
		get_filename_component(_tmp_FILE ${_current_FILE} ABSOLUTE)
		get_filename_component(_basename ${_tmp_FILE} NAME_WE)
		set(_header ${CMAKE_CURRENT_BINARY_DIR}/ui_${_basename}.h)

		# we need to run uic and replace some things in the generated file
		# this is done by executing the cmake script LyXuic.cmake
		# ######
		# Latest test showed on linux and windows show no bad consequeces,
		# so we removed the call to LyXuic.cmake
		qt_wrap_uifiles(${_header} ${_tmp_FILE} OPTIONS -tr lyx::qt_)
		list(APPEND uifiles ${_header})
	endforeach()
	set(${_ui_files} ${uifiles})
endmacro(LYX_ADD_UI_FILES)



macro(LYX_AUTOMOC)
	set(_matching_FILES)
	foreach (_current_FILE ${ARGN})

		get_filename_component(_abs_FILE ${_current_FILE} ABSOLUTE)
		# if "SKIP_AUTOMOC" is set to true, we will not handle this file here.
		# here. this is required to make bouic work correctly:
		# we need to add generated .cpp files to the sources (to compile them),
		# but we cannot let automoc handle them, as the .cpp files don't exist yet when
		# cmake is run for the very first time on them -> however the .cpp files might
		# exist at a later run. at that time we need to skip them, so that we don't add two
		# different rules for the same moc file
		get_source_file_property(_skip ${_abs_FILE} SKIP_AUTOMOC)

		if (EXISTS ${_abs_FILE} AND NOT _skip)

			file(READ ${_abs_FILE} _contents)

			get_filename_component(_abs_PATH ${_abs_FILE} PATH)

			string(REGEX MATCHALL "#include +[\"<]moc_[^ ]+\\.cpp[\">]" _match "${_contents}")
			if (_match)
				foreach (_current_MOC_INC ${_match})
					string(REGEX MATCH "moc_[^ <\"]+\\.cpp" _current_MOC "${_current_MOC_INC}")

					get_filename_component(_basename ${_current_MOC} NAME_WE)

					string(LENGTH ${_basename} _length)
					MATH(EXPR _mocless_length ${_length}-4)
					STRING(SUBSTRING  ${_basename} 4 ${_mocless_length} _mocless_name )

					set(_header ${_abs_PATH}/${_mocless_name}.h)

					#message(STATUS "moc : ${_header}")
					#set(_header ${CMAKE_CURRENT_SOURCE_DIR}/${_basename}.h)
					#set(_header ${_abs_PATH}/${_basename}.h)

					set(_moc  ${CMAKE_CURRENT_BINARY_DIR}/${_current_MOC})
					if (WIN32)
						set(_def -D_WIN32)
					else()
						set(_def)
					endif()
					#set(_moc ${_abs_PATH}/${_current_MOC})
					add_custom_command(OUTPUT ${_moc}
							  COMMAND ${QT_MOC_EXECUTABLE}
							  ARGS "-DQT_VERSION=${QTx_VERSION}" ${_def} ${_moc_INCS} ${_header} -o ${_moc}
							  MAIN_DEPENDENCY ${_header})
					macro_add_file_dependencies(${_abs_FILE} ${_moc})
					SET_SOURCE_FILES_PROPERTIES(${_moc} GENERATED)
				endforeach (_current_MOC_INC)
			else()
				#message(STATUS "moc not found : ${_abs_FILE} ")
			endif()
		endif()
	endforeach (_current_FILE)
endmacro (LYX_AUTOMOC)


macro(lyx_const_touched_files _allinone_name _list)
	set(_file_list ${_allinone_name}_files)
	set(_file_const ${CMAKE_CURRENT_BINARY_DIR}/${_allinone_name}_const.C)
	set(_file_touched ${CMAKE_CURRENT_BINARY_DIR}/${_allinone_name}_touched.C)

	# don't touch exisiting or non-empty file,
	# so a cmake re-run doesn't touch all created files
	set(_rebuild_file_const 0)
	if (NOT EXISTS ${_file_const})
		set(_rebuild_file_const 1)
	else()
		FILE(READ ${_file_const} _file_content)
		if (NOT _file_content)
			set(_rebuild_file_const 1)
		endif()
	endif()

	set(_rebuild_file_touched 0)
	if (NOT EXISTS ${_file_touched})
		set(_rebuild_file_touched 1)
	else()
		FILE(READ ${_file_touched} _file_content)
		if (NOT _file_content)
			set(_rebuild_file_touched 1)
		endif()
	endif()

	if (LYX_MERGE_REBUILD)
		#message(STATUS "Merge files build: rebuilding generated files")
		set(_rebuild_file_const 1)
		set(_rebuild_file_touched 1)
	endif()

	if (_rebuild_file_const)
		file(WRITE  ${_file_const} "// autogenerated file \n//\n")
		file(APPEND ${_file_const} "//	 * clear or delete this file to build it again by cmake \n//\n\n")
	endif()

	if (_rebuild_file_touched)
		file(WRITE  ${_file_touched} "// autogenerated file \n//\n")
		file(APPEND ${_file_touched} "//	 * clear or delete this file to build it again by cmake \n//\n")
		file(APPEND ${_file_touched} "//	 * don't touch this file \n//\n\n")
		file(APPEND ${_file_touched} "#define DONT_INCLUDE_CONST_FILES\n")
		file(APPEND ${_file_touched} "#include \"${_file_const}\"\n\n\n")
	endif()

	#add merged files also to the project so they become editable
		if(${GROUP_CODE} MATCHES "flat")
		lyx_add_info_files_no_group(${${_list}})
	else()
		lyx_add_info_files(MergedFiles ${${_list}})
	endif()

	set(${_file_list} ${_file_const} ${_file_touched} ${lyx_${groupname}_info_files})

	foreach (_current_FILE ${${_list}})
		get_filename_component(_abs_FILE ${_current_FILE} ABSOLUTE)
		# don't include any generated files in the final-file
		# because then cmake will not know the dependencies
		get_source_file_property(_isGenerated ${_abs_FILE} GENERATED)
		if (_isGenerated)
			list(APPEND ${_file_list} ${_abs_FILE})
		else()
		  GET_FILENAME_COMPONENT(_file_name ${_abs_FILE} NAME_WE)
		  STRING(REGEX REPLACE "-" "_" _file_name "${_file_name}")
		  set(__macro_name ${_file_name}___ASSUME_CONST)

		  if (_rebuild_file_const)
			  file(APPEND ${_file_const}  "#define ${__macro_name}\n")
			  file(APPEND ${_file_const}  "#if defined(${__macro_name}) && !defined(DONT_INCLUDE_CONST_FILES)\n")
			  file(APPEND ${_file_const}  "#include \"${_abs_FILE}\"\n")
			  file(APPEND ${_file_const}  "#endif\n\n")
		  endif()

		  if (_rebuild_file_touched)
			  file(APPEND ${_file_touched}  "#ifndef ${__macro_name}\n")
			  file(APPEND ${_file_touched}  "#include \"${_abs_FILE}\"\n")
			  file(APPEND ${_file_touched}  "#endif\n\n")
		  endif()
		endif()
	endforeach (_current_FILE)
endmacro(lyx_const_touched_files)

macro(LYX_OPTION_INIT)
	set(LYX_OPTIONS)
	set(LYX_OPTION_STRINGS)
endmacro()


macro(LYX_OPTION _name _description _default _sys)
	set(_msg OFF)
	if(${_sys} MATCHES "GCC")
		set(_system CMAKE_COMPILER_IS_GNUCXX)
	elseif(${_sys} MATCHES "MAC")
		set(_system APPLE)
	else()
		set(_system ${_sys})
	endif()
	if(${_system} MATCHES "ALL")
		option(LYX_${_name} ${_description} ${_default})
		set(_msg ON)
	else()
		if(${${_system}})
			option(LYX_${_name} ${_description} ${_default})
			set(_msg ON)
		endif()
	endif()
	list(APPEND LYX_OPTIONS LYX_${_name})
	set(LYX_${_name}_description ${_description})
	set(LYX_${_name}_show_message ${_msg})
endmacro()

macro(LYX_COMBO _name _description _default)
  set(_lyx_name "LYX_${_name}")
  set(${_lyx_name} ${_default} CACHE STRING "${_description}")
  set(_combo_list ${_default} ${ARGN})
  set_property(CACHE ${_lyx_name} PROPERTY STRINGS ${_combo_list})
  list(APPEND LYX_OPTIONS ${_lyx_name})
  set(${_lyx_name}_show_message ON)
  string(REGEX REPLACE ";" " " _use_list "${_combo_list}")
  set(${_lyx_name}_description "${_description} (${_use_list})")
  list(APPEND LYX_OPTION_STRINGS ${_lyx_name})
  # Now check the value
  list(FIND _combo_list ${${_lyx_name}} _idx)
  if (_idx LESS 0)
    message(FATAL_ERROR "${_lyx_name} set to \"${${_lyx_name}}\", but has to be only one of (${_use_list})")
  endif()
endmacro()

macro(LYX_STRING _name _description _default)
  set(_lyx_name "LYX_${_name}")
  list(APPEND LYX_OPTIONS ${_lyx_name})
  set(${_lyx_name}_show_message ON)
  set(${_lyx_name}_description "${_description}")
  list(APPEND LYX_OPTION_STRINGS ${_lyx_name})
  # Now check the value
  # Should not contain ' '
  set(tmp_lyx_name ${${_lyx_name}})
  if (NOT "${${_lyx_name}}" STREQUAL "")
    if (NOT "${tmp_lyx_name}" MATCHES "^\\..*$")
      set(tmp_lyx_name ".${tmp_lyx_name}")
    endif()
    if (NOT "${tmp_lyx_name}" MATCHES "^\\.[a-zA-Z_0-9\\.]+$")
      message(FATAL_ERROR "Invalid string for lyx suffix (${tmp_lyx_name})")
    endif()
  endif()
  set(${_lyx_name} "${tmp_lyx_name}" CACHE STRING "${_description}" FORCE)
endmacro()

macro(LYX_OPTION_LIST_ALL)
	if(UNIX)
		set(run_cmake ${CMAKE_BINARY_DIR}/run_cmake.sh)
		file(WRITE ${run_cmake} "#!/bin/bash \n")
		execute_process(COMMAND chmod 755 ${run_cmake})
		set(cont "\\\n")
	elseif(WIN32)
		set(run_cmake ${CMAKE_BINARY_DIR}/run_cmake.bat)
		file(WRITE ${run_cmake} "")
		set(cont "<nul ^\n")
	endif()
	file(APPEND ${run_cmake} "cmake ${CMAKE_SOURCE_DIR}  ${cont}")
	file(APPEND ${run_cmake} " -G\"${CMAKE_GENERATOR}\"  ${cont}")
	foreach(_option ${LYX_OPTIONS})
		if(${_option}_show_message OR ${ARGV0} STREQUAL "help")
                        get_property(_prop CACHE ${_option} PROPERTY STRINGS)
			list(FIND LYX_OPTION_STRINGS ${_option} _index)
			set(_type "BOOL")
			if (${_index} GREATER -1)
			  #message(STATUS "${_option} is of type string")
                          set(_isset ${${_option}})
			  set(_type "STRING")
			elseif(${_option})
				set(_isset ON)
			else()
				set(_isset OFF)
			endif()
			string(SUBSTRING "${_option}:${_type}                            " 0 35 _var)
			string(SUBSTRING "${_isset}           " 0 7 _val)
			message(STATUS "${_var}= ${_val}: ${${_option}_description}")
			file(APPEND ${run_cmake} " -D${_option}:${_type}=${${_option}}  ${cont}")
		endif()
	endforeach()
	file(APPEND ${run_cmake} "\n")
	message(STATUS)
	message(STATUS "CMake command with options is available in shell script")
	message(STATUS "    '${run_cmake}'")
endmacro()

macro(lyx_add_info_files group)
	foreach(_it ${ARGN})
		if(NOT IS_DIRECTORY ${_it})
			get_filename_component(name ${_it} NAME)
			if(NOT ${_it} MATCHES "^/\\\\..*$;~$")
				set_source_files_properties(${_it} PROPERTIES HEADER_FILE_ONLY TRUE)
				set(lyx_${group}_info_files ${lyx_${group}_info_files} ${_it})
			endif()
		endif()
	endforeach()
	set(check_group ${group}) #cmake bug?
	if(check_group)
		source_group(${group} FILES ${lyx_${group}_info_files})
	endif()
	set(lyx_info_files ${lyx_info_files} ${lyx_${group}_info_files})
endmacro()

macro(lyx_add_info_files_no_group)
	lyx_add_info_files( "" ${ARGN})
endmacro()

macro(lyx_find_info_files group files)
	file(GLOB _filelist ${files})
	lyx_add_info_files(${group} ${_filelist})
endmacro()

macro(settestlabel testname)
  get_property(_lab_list TEST ${testname} PROPERTY LABELS)
  if(_lab_list)
    list(APPEND _lab_list "${ARGN}")
  else()
    set(_lab_list "${ARGN}")
  endif()
  list(REMOVE_DUPLICATES _lab_list)
  set_tests_properties(${testname} PROPERTIES LABELS "${_lab_list}")
endmacro()

macro(sortlabellist listout)
  set(tmplist "")
  foreach(_lab ${ARGN})
    list(APPEND tmplist "${depth_${_lab}}${_lab}")
  endforeach()
  list(SORT tmplist)
  string(REGEX REPLACE ";[-0-9]+" ";" ${listout} ";${tmplist}")
endmacro()

macro(createlabel reslabel first)
  set(${reslabel} ${first})
  foreach(_lab ${ARGN})
    set(${reslabel} "${${reslabel}}:${_lab}")
  endforeach()
endmacro()

macro(setmarkedtestlabel testname)
  sortlabellist(mynewlablelist ${ARGN})
  createlabel(mynewlabel ${mynewlablelist})
  settestlabel(${testname} ${mynewlabel})
endmacro()

macro(lyx_target_link_libraries _target)
  foreach(_lib ${ARGN})
    string(TOUPPER ${_lib} _ulib)
    if(${_ulib}_FOUND)
      #message(STATUS "target_link_libraries(${_target} ${${_lib}_LIBRARY})")
      target_link_libraries(${_target} ${${_lib}_LIBRARY})
    endif()
  endforeach()
endmacro()

# Check if python module exists
# Idea stolen from http://public.kitware.com/pipermail/cmake/2011-January/041666.html
function(find_python_module module)
  string(TOUPPER ${module} module_upper)
  if(NOT LYX_PY_${module_upper})
    if(ARGC GREATER 1 AND ARGV1 STREQUAL "REQUIRED")
      set(LYX_PY_${module}_FIND_REQUIRED TRUE)
    endif()
    # A module's location is usually a directory, but for binary modules
    # it's a .so file.
    execute_process(COMMAND "${LYX_PYTHON_EXECUTABLE}" "-c"
      "import re, ${module}; print(re.compile('/__init__.py.*').sub('',${module}.__file__))"
      RESULT_VARIABLE _${module}_status
      OUTPUT_VARIABLE _${module}_location
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _${module}_status)
      set(LYX_PY_${module_upper} ${_${module}_location} CACHE STRING
        "Location of Python module ${module}")
    endif()
  endif()
  find_package_handle_standard_args(LYX_PY_${module} DEFAULT_MSG LYX_PY_${module_upper})
endfunction(find_python_module)

macro(setstripped _varname)
        if(${ARGC} GREATER 1)
                string(STRIP "${ARGV1}" _v)
                if (_v MATCHES "^\\[\(.+\)\\]$")
                  set(_v ${CMAKE_MATCH_1})
                endif()
                if(USE_POSIX_PACKAGING)
                        string(TOLOWER ${_v} ${_varname})
                else()
                        set(${_varname} ${_v})
                endif()
        else()
                set(${_varname})
        endif()
endmacro(setstripped)

# Determine the version and build-type
function(determineversionandbuildtype configfile package version dirs date buildtype)
  file(STRINGS "${configfile}" _config_lines)
  foreach(_c_l ${_config_lines} )
    if(_c_l MATCHES "^AC_INIT\\(\([^,]+\),\([^,]+\), *\\[\([^,]+\)\\] *,\(.*\)")
      # Not using CMAKE_MATCH_ directly is needed, because
      # its value is now changed inside macro setstripped
      set(_PB ${CMAKE_MATCH_1})
      set(_PV ${CMAKE_MATCH_2})
      set(_PBU ${CMAKE_MATCH_3})
      setstripped(PACKAGE_BASE ${_PB})
      setstripped(PACKAGE_VERSION ${_PV})
      setstripped(PACKAGE_BUGREPORT ${_PBU})
      set(${package} ${PACKAGE_BASE} ${PACKAGE_VERSION} ${PACKAGE_BUGREPORT} PARENT_SCOPE)
      if(PACKAGE_VERSION MATCHES "^\([0-9]+\)\\.\([0-9]+\)\(\\.\([0-9]+\)\(\\.\([0-9]+\)\)?\)?[-~]?\([A-Za-z]+[0-9]*\).*$")
        set(LYX_MAJOR_VERSION ${CMAKE_MATCH_1})
        set(LYX_MINOR_VERSION ${CMAKE_MATCH_2})
        set(LYX_RELEASE_LEVEL ${CMAKE_MATCH_4})
        set(LYX_RELEASE_PATCH ${CMAKE_MATCH_6})
        set(LYX_BUILD_TYPE_MATCH ${CMAKE_MATCH_7})
        string(TOLOWER "${LYX_BUILD_TYPE_MATCH}" LYX_BUILD_TYPE)
        set(LYX_DIR_VER "LYX_DIR_${CMAKE_MATCH_1}${CMAKE_MATCH_2}x")
        set(LYX_USERDIR_VER "LYX_USERDIR_${CMAKE_MATCH_1}${CMAKE_MATCH_2}x")
        if (NOT LYX_RELEASE_LEVEL)
          set(LYX_RELEASE_LEVEL 0)
        endif()
        if (NOT LYX_RELEASE_PATCH)
          set(LYX_RELEASE_PATCH 0)
        endif()
        set(LYX_VERSION "${LYX_MAJOR_VERSION}.${LYX_MINOR_VERSION}")
      endif()
    endif()
    if(_c_l MATCHES "^AC_SUBST\\( *LYX_DATE *, *\\[\\\"(.*)\\\"\\].*")
      set(LYX_DATE "${CMAKE_MATCH_1}")
    endif()
  endforeach(_c_l)
  set(${version} ${LYX_VERSION} ${LYX_MAJOR_VERSION} ${LYX_MINOR_VERSION} ${LYX_RELEASE_LEVEL} ${LYX_RELEASE_PATCH} PARENT_SCOPE)
  set(${dirs} ${LYX_DIR_VER} ${LYX_USERDIR_VER} PARENT_SCOPE)
  set(${date} ${LYX_DATE} PARENT_SCOPE)
  if(LYX_BUILD_TYPE MATCHES "^\(dev(el)?\)$")
    set(${buildtype} "development" PARENT_SCOPE)
  elseif(LYX_BUILD_TYPE MATCHES "^\(alpha|beta|rc|pre\)[0-9]*$")
    set(${buildtype} "prerelease" PARENT_SCOPE)
  elseif(LYX_BUILD_TYPE MATCHES "^$")
    set(${buildtype} "release" PARENT_SCOPE)
  else()
    set(${buildtype} "unknown" PARENT_SCOPE)
    message(FATAL_ERROR "\"${configfile}\": Unable to determine build-type from suffix \"${LYX_BUILD_TYPE}\" in AC_INIT macro")
  endif()
endfunction(determineversionandbuildtype)

# determine known cmake cxx_std features but only if not greater than ${max_desired}
function(lyxgetknowncmakestd max_desired min_enabled result)
  set(tmp_list)
  set(CXX_STD_LIST)
  math(EXPR max_desired "${max_desired}+1")
  if (CMAKE_VERSION VERSION_LESS "3.9")
    list(APPEND tmp_list 14 17 20)
  else()
    foreach(_e ${CMAKE_CXX_COMPILE_FEATURES})
      if (_e MATCHES "^cxx_std_\(.*)")
        list(APPEND tmp_list ${CMAKE_MATCH_1})
      endif()
    endforeach()
  endif()
  list(REVERSE tmp_list)
  # Filter undesired from list
  foreach(i ${tmp_list})
    if (i LESS ${max_desired} AND i GREATER_EQUAL ${min_enabled})
      list(APPEND CXX_STD_LIST ${i})
    endif()
  endforeach()
  set(${result} ${CXX_STD_LIST} PARENT_SCOPE)
endfunction()

# Assign a cache variable to an option.
# E.g. "CC" + "-Wno-shadow" -> "CC_WSHADOW"
# or "CXX" + "-Wformat=2" -> "CXX_WFORMAT_2"
function(get_warning_var_name _compiler _warning _testwarning _varname)
  string(REGEX REPLACE "^\-Wno\-" "-W" _testwarning1 ${_warning})
  string(REGEX REPLACE "[\-=]" "_" _flag3 ${_testwarning1})
  # create check_flag_name for cache
  string(TOUPPER ${_flag3} _FLAG3)
  set(${_varname} "${_compiler}${_FLAG3}" PARENT_SCOPE)
  #message(STATUS "setting set(${_testwarning} ${_testwarning1})")
  set(${_testwarning} ${_testwarning1} PARENT_SCOPE)
endfunction(get_warning_var_name)

function(perform_check_warning _compiler VARNAME _testwarning _valid)
  mark_as_advanced(${VARNAME}) # Needed to avoid multiple checks on same warning-option
  if (${_compiler} MATCHES "CC")
    check_c_compiler_flag(${_testwarning} ${VARNAME})
  else()
    #message(STATUS "performing check_cxx_compiler_flag(${_testwarning} ${VARNAME})")
    check_cxx_compiler_flag(${_testwarning} ${VARNAME})
  endif()
  if (${${VARNAME}})
    set(${_valid} ON PARENT_SCOPE)
  else()
    set(${_valid} OFF PARENT_SCOPE)
  endif()
endfunction(perform_check_warning)

# Add extra warning option to specific source file
# mostly used to disable option (like "-Wno-invalid-noreturn")
# given in parameters ${ARGN}
# e.g. handle_source_option("CXX" "${TOP_SRC_DIR}/src/LyX.cpp" -Wno-invalid-noreturn -Wno-missing-noreturn)
macro(handle_source_option _compiler _sourcefile)
  #message(STATUS "handle_source_option ${_compiler} ${_sourcefile} ${ARGN}")
  set(tmp_list "")
  get_source_file_property(_myflags ${_sourcefile} COMPILE_FLAGS)
  if (${_myflags} MATCHES "NOTFOUND")
    # this happens for the first time this _sourcefile has no COMPILE_FLAGS property yet
    set(_myflags "")
  else()
    string(REGEX REPLACE "^ +" "" _myflags ${_myflags})
    string(REGEX REPLACE " " ";" _myflags ${_myflags})
  endif()
  foreach(_lab ${_myflags} ${ARGN})
    get_warning_var_name(${_compiler} ${_lab} _testwarning NAME)
    perform_check_warning(${_compiler} ${NAME} ${_testwarning} _valid)
    if (${_valid})
      set(tmp_list "${tmp_list} ${_lab}")
    endif()
  endforeach()
  if (tmp_list)
    #message(STATUS "Adding ${tmp_list} to ${_sourcefile}")
    set_property(SOURCE ${_sourcefile} PROPERTY COMPILE_FLAGS ${tmp_list})
  endif()
endmacro(handle_source_option)

# Extract only valid compiler options from LYX_CXX_OPTIONS and LYX_CXX_FLAGS_EXTRA,
# But also omit options provided with any the extra parameters ${ARGN}
# Selected options will be valid for current source directory + all its subdirectories
#
# like: handle_warning_options("libiconv" "CC" -Wnested-anon-types -Werror)
macro(handle_warning_options _source _compiler)
  set(tmp_list ${LYX_CXX_OPTIONS} ${LYX_CXX_FLAGS_EXTRA})
  list(REMOVE_DUPLICATES tmp_list)
  #message(STATUS "_source = ${_source},_compiler = ${_compiler}, tmp_list = ${tmp_list}")
  #message(STATUS "ARGN = ${ARGN}")
  set(result_list)
  foreach(_lab ${ARGN})
    if (${_lab} MATCHES "^\-Wno\-.*")
      string(REGEX REPLACE "^\-Wno\-" "-W" _lab2 ${_lab})
      #message(STATUS "Removing option ${_lab2}")
      list(REMOVE_ITEM tmp_list ${_lab2})
      #message(STATUS "Appending option ${_lab}")
      list(APPEND tmp_list ${_lab})
    else()
      list(REMOVE_ITEM tmp_list ${_lab})
    endif()
  endforeach()
  foreach(_flag ${tmp_list})
    get_warning_var_name(${_compiler} ${_flag} _testwarning NAME)
    message(STATUS "Name = ${NAME}, _testwarning = ${_testwarning}")
    perform_check_warning(${_compiler} ${NAME} ${_testwarning} _valid)
    if (${_valid})
      #message(STATUS "ADDING option ${_flag}")
      add_compile_options(${_flag})
      list(APPEND result_list ${_flag})
    else()
      #message(STATUS "DISCARD option ${_flag}")
    endif()
  endforeach()
  message(STATUS "result_list = ${result_list}")
endmacro(handle_warning_options)

macro(check_includes _par)
  get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
  message(STATUS "source dir${_par} = ${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(dir ${dirs})
    message(STATUS "include dir${_par} = '${dir}'")
  endforeach()
endmacro(check_includes)
