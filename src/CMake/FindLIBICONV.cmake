# - Tries to find the GNU libiconv library.
#
# Sets the following variables:
#	LIBICONV_FOUND
# 	LIBICONV_INCLUDE_DIRS
#	LIBICONV_LIBRARIES
#	LIBICONV_DEFINITIONS


# libiconv is sometimes part 
include (CheckSymbolExists)
check_symbol_exists (iconv_open iconv.h LIBICONV_JUST_WORKS QUIET)

if (LIBICONV_JUST_WORKS)
    message (STATUS "Found LIBICONV")
    set (LIBICONV_FOUND ON)
else (LIBICONV_JUST_WORKS)

    # A package full of convenience functions for writing Find modules
    include (LibFindMacros)
    
    # Try to use pkg-config's data to help find the include dir and lib file
    libfind_pkg_search_modules (LIBICONV_PKGCONF iconv libiconv
                                libiconv-2 libiconv2 c)

    # Pass on any other options like macro definitions found by pkg-config
    set (LIBICONV_DEFINITIONS LIBICONV_PKGCONF_CFLAGS_OTHER)
    
    # Find include dir and library file, possibly with help of pkg-config
    find_path (LIBICONV_INCLUDE_DIR NAMES iconv.h HINTS
               ${LIBICONV_PKGCONF_INCLUDE_DIRS} ${LIBICONV_PKGCONF_INCLUDEDIR})
    find_library (LIBICONV_LIBRARY NAMES iconv libiconv libiconv-2 libiconv2 c
                  HINTS ${LIBICONV_PKGCONF_LIBRARY_DIRS})

    # Set the LIBICONV_PROCESS_ variables and call libfind_process to wrap it
    # all up and report
    set (LIBICONV_PROCESS_INCLUDES LIBICONV_INCLUDE_DIR)
    set (LIBICONV_PROCESS_LIBS LIBICONV_LIBRARY)
    libfind_process (LIBICONV)

endif (LIBICONV_JUST_WORKS)
