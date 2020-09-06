#
# SPDX-FileCopyrightText: 2013 Valama development team
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

find_program(INTLTOOL_EXTRACT_EXECUTABLE intltool-extract)
find_program(INTLTOOL_MERGE_EXECUTABLE intltool-merge)
mark_as_advanced(INTLTOOL_EXTRACT_EXECUTABLE)
mark_as_advanced(INTLTOOL_MERGE_EXECUTABLE)

if(INTLTOOL_EXTRACT_EXECUTABLE)
  execute_process(
    COMMAND
      ${INTLTOOL_EXTRACT_EXECUTABLE} "--version"
    OUTPUT_VARIABLE
      intltool_version
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(intltool_version MATCHES "^intltool-extract \\(.*\\) [0-9]")
    string(REGEX REPLACE "^intltool-extract \\([^\\)]*\\) ([0-9\\.]+[^ \n]*).*" "\\1" INTLTOOL_VERSION_STRING "${intltool_version}")
  endif()
  unset(intltool_version)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Intltool
  REQUIRED_VARS
    INTLTOOL_EXTRACT_EXECUTABLE
    INTLTOOL_MERGE_EXECUTABLE
  VERSION_VAR
    INTLTOOL_VERSION_STRING
)

set(INTLTOOL_OPTIONS_DEFAULT
  "--quiet"
)
