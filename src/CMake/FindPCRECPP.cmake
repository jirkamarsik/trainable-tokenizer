# - Tries to find the C++ interface to PCRE.
#
# Sets the following variables:
#	PCRECPP_FOUND
# 	PCRECPP_INCLUDE_DIRS
#	PCRECPP_LIBRARY_DIRS
#	PCRECPP_LIBRARIES
#	PCRECPP_DEFINITIONS


# PCRECPP can be usually seen with a helper script which gives us the info we need.
find_program (PCRE_CONFIG NAMES pcre-config DOC "Path to the helper pcre-config script.")
mark_as_advanced (PCRE_CONFIG)

if (PCRE_CONFIG)
  execute_process (COMMAND ${PCRE_CONFIG} --cflags OUTPUT_VARIABLE PCRECPP_CONFIG_PREPROCESSOR)
  execute_process (COMMAND ${PCRE_CONFIG} --libs-cpp OUTPUT_VARIABLE PCRECPP_CONFIG_LINKER)

  # icu-config returns the parameters as preprocessor and linker flags, which we have to
  # disassemble to get at the library names and directories.
  if (WIN32)
    separate_arguments (PCRE_CONFIG_PREPROCESSOR WINDOWS_COMMAND ${PCRE_CONFIG_PREPROCESSOR})
    separate_arguments (PCRE_CONFIG_LINKER WINDOWS_COMMAND ${PCRE_CONFIG_LINKER})
  else (WIN32)
    separate_arguments (PCRE_CONFIG_PREPROCESSOR UNIX_COMMAND ${PCRE_CONFIG_PREPROCESSOR})
    separate_arguments (PCRE_CONFIG_LINKER UNIX_COMMAND ${PCRE_CONFIG_LINKER})
  endif (WIN32)

  foreach (INCLUDE_DIR IN LISTS PCRE_CONFIG_PREPROCESSOR)
    string (REGEX REPLACE "^-I" "" INCLUDE_DIR ${INCLUDE_DIR})
    set (PCRECPP_INCLUDE_DIRS ${PCRECPP_INCLUDE_DIRS} ${INCLUDE_DIR})
  endforeach (INCLUDE_DIR)

  foreach (LINKER_ARG IN LISTS PCRE_CONFIG_LINKER)
    if (LINKER_ARG MATCHES "^-L.+")
      string (REGEX REPLACE "^-L" "" LIBRARY_DIR ${LINKER_ARG})
      set (PCRECPP_LIBRARY_DIRS ${PCRECPP_LIBRARY_DIRS} ${LIBRARY_DIR})
    elseif (LINKER_ARG MATCHES "^-l.+")
      string (REGEX REPLACE "^-l.+" "" LIBRARY ${LINKER_ARG})
      set (PCRECPP_LIBRARIES ${PCRECPP_LIBRARIES} ${LIBRARY})
    endif (LINKER_ARG MATCHES "^-L.+")
  endforeach (LINKER_ARG)

  message (STATUS "Found PCRECPP")
  set (PCRECPP_FOUND ON)
else (PCRE_CONFIG)
  # A package full of convenience functions for writing Find modules
  include (LibFindMacros)
  
  # Try to use pkg-config's data to help find the include dir and lib file
  libfind_pkg_search_modules (PCRECPP_PKGCONF perl perlcpp QUIET)

  # Pass on any other options like macro definitions found by pkg-config
  set (PCRECPP_DEFINITIONS PCRECPP_PKGCONF_CFLAGS_OTHER)
  
  # Find include dir and library file, possibly with help of pkg-config
  find_path (PCRECPP_INCLUDE_DIR NAMES pcre.h pcrecpp.h HINTS ${PCRECPP_PKGCONF_INCLUDE_DIRS} ${PCRECPP_PKGCONF_INCLUDEDIR})
  find_library (PCRECPP_LIBRARY NAMES pcre pcrecpp HINTS ${PCRECPP_PKGCONF_LIBRARY_DIRS})

  # Set the PCRECPP_PROCESS_ variables and call libfind_process to wrap it all up and report
  set (PCRECPP_PROCESS_INCLUDES PCRECPP_INCLUDE_DIR)
  set (PCRECPP_PROCESS_LIBS PCRECPP_LIBRARY)
  libfind_process (PCRECPP)
endif (PCRE_CONFIG)
