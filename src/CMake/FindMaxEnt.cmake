# - Tries to find Le Zhang's Maximum Entropy Toolkit for Python and C++
# Sets the following variables:
#	MaxEnt_FOUND
# 	MaxEnt_INCLUDE_DIRS
#	MaxEnt_LIBRARIES
#	MaxEnt_DEFINITIONS

# A package full of convenience functions for writing Find modules
include (LibFindMacros)

# Try to use pkg-config's data to help find the include dir and lib file
libfind_pkg_check_modules (MaxEnt_PKGCONF maxent QUIET)

# Pass on any other options like macro definitions found by pkg-config
set (MaxEnt_DEFINITIONS MaxEnt_PKGCONF_CFLAGS_OTHER)

# Find include dir and library file, possibly with help of pkg-config
find_path (MaxEnt_INCLUDE_DIR NAMES maxentmodel.hpp PATH_SUFFIXES maxent
           HINTS ${MaxEnt_PKGCONF_INCLUDE_DIRS} ${MaxEnt_PKGCONF_INCLUDEDIR})
find_library (MaxEnt_LIBRARY NAMES maxent HINTS ${MaxEnt_PKGCONF_LIBRARY_DIRS})

# Set the MaxEnt_PROCESS_ variables and call libfind_process to wrap it all up
# and report
set (MaxEnt_PROCESS_INCLUDES MaxEnt_INCLUDE_DIR)
set (MaxEnt_PROCESS_LIBS MaxEnt_LIBRARY)
libfind_process (MaxEnt maxentmodel.hpp)
