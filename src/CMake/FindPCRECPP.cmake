# - Tries to find the C++ interface to PCRE.
#
# Sets the following variables:
#	PCRECPP_FOUND
# 	PCRECPP_INCLUDE_DIRS
#	PCRECPP_LIBRARY_DIRS
#	PCRECPP_LIBRARIES
#	PCRECPP_DEFINITIONS


# PCRECPP can be usually seen with a helper script which gives us the info
# we need.
find_program (PCRE_CONFIG NAMES pcre-config
              DOC "Path to the helper pcre-config script.")
mark_as_advanced (PCRE_CONFIG)


# We have pcre-config and use it to query for how to compile with PCRE.
if (PCRE_CONFIG)

  execute_process (COMMAND ${PCRE_CONFIG} --cflags
                   OUTPUT_VARIABLE PCRE_CONFIG_PREPROCESSOR)
  execute_process (COMMAND ${PCRE_CONFIG} --libs-cpp
                   OUTPUT_VARIABLE PCRE_CONFIG_LINKER)

  # icu-config returns the parameters as preprocessor and linker flags,
  # which we have to disassemble to get at the library names and directories.
  if (WIN32)
    separate_arguments (PCRE_CONFIG_PREPROCESSOR WINDOWS_COMMAND
                        ${PCRE_CONFIG_PREPROCESSOR})
    separate_arguments (PCRE_CONFIG_LINKER WINDOWS_COMMAND
                        ${PCRE_CONFIG_LINKER})
  else (WIN32)
    separate_arguments (PCRE_CONFIG_PREPROCESSOR UNIX_COMMAND
                        ${PCRE_CONFIG_PREPROCESSOR})
    separate_arguments (PCRE_CONFIG_LINKER UNIX_COMMAND
                        ${PCRE_CONFIG_LINKER})
  endif (WIN32)

  foreach (COMPILER_ARG IN LISTS PCRE_CONFIG_PREPROCESSOR)
    if (COMPILER_ARG MATCHES "^-I.+")
      string (REGEX REPLACE "^-I" "" INCLUDE_DIR ${COMPILER_ARG})
      set (PCRECONF_INCLUDE_DIRS ${PCRECONF_INCLUDE_DIRS} ${INCLUDE_DIR})
    elseif (LINKER_ARG MATCHES "^-I.+")
      set (PCRECONF_CFLAGS_OTHER ${PCRECONF_CFLAGS_OTHER} ${COMPILER_ARG})
    endif (COMPILER_ARG MATCHES "^-I.+")
  endforeach (COMPILER_ARG)

  foreach (LINKER_ARG IN LISTS PCRE_CONFIG_LINKER)
    if (LINKER_ARG MATCHES "^-L.+")
      string (REGEX REPLACE "^-L" "" LIBRARY_DIR ${LINKER_ARG})
      set (PCRECONF_LIBRARY_DIRS ${PCRECONF_LIBRARY_DIRS} ${LIBRARY_DIR})
    endif (LINKER_ARG MATCHES "^-L.+")
  endforeach (LINKER_ARG)

endif (PCRE_CONFIG)

# A package full of convenience functions for writing Find modules
include (LibFindMacros)

# Try to use pkg-config's data to help find the include dir and lib file
libfind_pkg_check_modules (PCRECPP_PKGCONF pcre pcrecpp QUIET)

# Pass on any other options like macro definitions found by pkg-config
set (PCRECPP_DEFINITIONS PCRECONF_CFLAGS_OTHER PCRECPP_PKGCONF_CFLAGS_OTHER)

# Find include dir and library file, possibly with help of pkg-config
find_path (PCRECPP_INCLUDE_DIR NAMES pcre.h pcrecpp.h
           HINTS ${PCRECONF_INCLUDE_DIRS}
                 ${PCRECPP_PKGCONF_INCLUDE_DIRS}
                 ${PCRECPP_PKGCONF_INCLUDEDIR})
find_library (PCRE_LIBRARY NAMES pcre
              HINTS ${PCRECONF_LIBRARY_DIRS} ${PCRECPP_PKGCONF_LIBRARY_DIRS})
find_library (PCRECPP_LIBRARY NAMES pcrecpp
              HINTS ${PCRECONF_LIBRARY_DIRS} ${PCRECPP_PKGCONF_LIBRARY_DIRS})

# Set the PCRECPP_PROCESS_ variables and call libfind_process to wrap it
# all up and report
set (PCRECPP_PROCESS_INCLUDES PCRECPP_INCLUDE_DIR)
set (PCRECPP_PROCESS_LIBS PCRE_LIBRARY PCRECPP_LIBRARY)
libfind_process (PCRECPP pcrecpp.h)
