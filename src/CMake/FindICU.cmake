# - Tries to find IBM's ICU library.
#
# Sets the following variables:
#	ICU_FOUND
# 	ICU_INCLUDE_DIRS
#	ICU_LIBRARY_DIRS
#	ICU_LIBRARIES
#	ICU_DEFINITIONS


# ICU can be usually seen with a helper script which gives us the info we need.
find_program (ICU_CONFIG NAMES icu-config
              DOC "Path to the icu-config helper script.")
mark_as_advanced (ICU_CONFIG)

if (ICU_CONFIG)

    execute_process (COMMAND ${ICU_CONFIG} --cppflags-searchpath
                     OUTPUT_VARIABLE ICU_CONFIG_INCLUDE_DIRS)
    execute_process (COMMAND ${ICU_CONFIG} --ldflags-searchpath
                     OUTPUT_VARIABLE ICU_CONFIG_LIBRARY_DIRS)
    execute_process (COMMAND ${ICU_CONFIG} --ldflags-libsonly
                     OUTPUT_VARIABLE ICU_CONFIG_LIBRARIES)

    # icu-config returns the parameters as preprocessor and linker flags,
    # which we have to disassemble to get at the library names and directories.
    if (WIN32)
        separate_arguments (ICU_CONFIG_INCLUDE_DIRS WINDOWS_COMMAND
                            ${ICU_CONFIG_INCLUDE_DIRS})
        separate_arguments (ICU_CONFIG_LIBRARY_DIRS WINDOWS_COMMAND
                            ${ICU_CONFIG_LIBRARY_DIRS})
        separate_arguments (ICU_CONFIG_LIBRARIES WINDOWS_COMMAND
                            ${ICU_CONFIG_LIBRARIES})
    else (WIN32)
        separate_arguments (ICU_CONFIG_INCLUDE_DIRS UNIX_COMMAND
                            ${ICU_CONFIG_INCLUDE_DIRS})
        separate_arguments (ICU_CONFIG_LIBRARY_DIRS UNIX_COMMAND
                            ${ICU_CONFIG_LIBRARY_DIRS})
        separate_arguments (ICU_CONFIG_LIBRARIES UNIX_COMMAND
                            ${ICU_CONFIG_LIBRARIES})
    endif (WIN32)

    foreach (INCLUDE_DIR IN LISTS ICU_CONFIG_INCLUDE_DIRS)
      string (REGEX REPLACE "^-I" "" INCLUDE_DIR ${INCLUDE_DIR})
      set (ICU_INCLUDE_DIRS ${ICU_INCLUDE_DIRS} ${INCLUDE_DIR})
    endforeach (INCLUDE_DIR)

    foreach (LIBRARY_DIR IN LISTS ICU_CONFIG_LIBRARY_DIRS)
      string (REGEX REPLACE "^-L" "" LIBRARY_DIR ${LIBRARY_DIR})
      set (ICU_LIBRARY_DIRS ${ICU_LIBRARY_DIRS} ${LIBRARY_DIR})
    endforeach (LIBRARY_DIR)

    foreach (LIBRARY IN LISTS ICU_CONFIG_LIBRARIES)
      string (REGEX REPLACE "^-l" "" LIBRARY ${LIBRARY})
      set (ICU_LIBRARIES ${ICU_LIBRARIES} ${LIBRARY})
    endforeach (LIBRARY)

    message (STATUS "Found ICU")
    set (ICU_FOUND ON)

else (ICU_CONFIG)

    # A package full of convenience functions for writing Find modules
    include (LibFindMacros)
    
    # Try to use pkg-config's data to help find the include dir and lib file
    libfind_pkg_check_modules (ICU_PKGCONF icu icuuc cygicuuc cygicuuc32
                               QUIET)

    # Pass on any other options like macro definitions found by pkg-config
    set (ICU_DEFINITIONS ICU_PKGCONF_CFLAGS_OTHER)
    
    # Find include dir and library file, possibly with help of pkg-config
    find_path (ICU_INCLUDE_DIR NAMES unicode/utypes.h HINTS
               ${ICU_PKGCONF_INCLUDE_DIRS} ${ICU_PKGCONF_INCLUDEDIR})
    find_library (ICU_LIBRARY NAMES icuuc cygicuuc cygicuuc32 HINTS
                  ${ICU_PKGCONF_LIBRARY_DIRS})

    # Set the ICU_PROCESS_ variables and call libfind_process to wrap it all up
    # and report
    set (ICU_PROCESS_INCLUDES ICU_INCLUDE_DIR)
    set (ICU_PROCESS_LIBS ICU_LIBRARY)
    libfind_process (ICU)
endif (ICU_CONFIG)
