# - Tries to find libtool's dynamic loading library.
# Sets the following variables:
#	LTDL_FOUND
# 	LTDL_INCLUDE_DIRS
#	LTDL_LIBRARIES
#	LTDL_DEFINITIONS

# A package full of convenience functions for writing Find modules
include (LibFindMacros)

# Try to use pkg-config's data to help find the include dir and lib file
libfind_pkg_check_modules (LTDL_PKGCONF ltdl QUIET)

# Pass on any other options like macro definitions found by pkg-config
set (LTDL_DEFINITIONS LTDL_PKGCONF_CFLAGS_OTHER)

# Find include dir and library file, possibly with help of pkg-config
find_path (LTDL_INCLUDE_DIR NAMES ltdl.h HINTS ${LTDL_PKGCONF_INCLUDE_DIRS}
           ${LTDL_PKGCONF_INCLUDEDIR})
find_library (LTDL_LIBRARY NAMES ltdl HINTS ${LTDL_PKGCONF_LIBRARY_DIRS})

# Set the LTDL_PROCESS_ variables and call libfind_process to wrap it all up
# and report
set (LTDL_PROCESS_INCLUDES LTDL_INCLUDE_DIR)
set (LTDL_PROCESS_LIBS LTDL_LIBRARY)
libfind_process (LTDL)
