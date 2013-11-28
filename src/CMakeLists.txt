cmake_minimum_required (VERSION 2.8)
project (TrainableTokenizer)


set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      "${CMAKE_CURRENT_SOURCE_DIR}/CMake/")
set (LIBS)

# The best hack I know of for looking for static libraries.
set (PREFER_STATIC_LIBRARIES "OFF" CACHE BOOL "Whether to prefer static libraries over dynamic ones.")
if (PREFER_STATIC_LIBRARIES)
  IF(WIN32)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  ELSE(WIN32)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  ENDIF(WIN32)
ENDIF (PREFER_STATIC_LIBRARIES)

# The FindBoost package is not future-proof, versions of newer packages
# have to be added explicitly, see FindBoost.cmake.
set (Boost_ADDITIONAL_VERSIONS "1.47" "1.47.0")
find_package (Boost 1.46 REQUIRED program_options filesystem system thread)
include_directories (${Boost_INCLUDE_DIRS})
link_directories (${Boost_LIBRARY_DIRS})
set (LIBS ${LIBS} ${Boost_LIBRARIES})

find_package (MaxEnt REQUIRED)
include_directories (${MaxEnt_INCLUDE_DIRS})
link_directories (${MaxEnt_LIBRARY_DIRS})
set (LIBS ${LIBS} ${MaxEnt_LIBRARIES})

# MaxEnt may be defining zlib symbols in its headers if it finds the library
# installed. With gold-style linkers we then have to explicitly link zlib.
find_package (ZLIB)
if (ZLIB_FOUND)
  include_directories (${ZLIB_INCLUDE_DIRS})
  set (LIBS ${LIBS} ${ZLIB_LIBRARIES})
endif (ZLIB_FOUND)

find_package (TBB REQUIRED)
include_directories (${TBB_INCLUDE_DIRS})
link_directories (${TBB_LIBRARY_DIRS})
if (TBB_DEBUG_FOUND)
  link_directories (${TBB_DEBUG_LIBRARY_DIRS})
  foreach (LIBRARY IN LISTS TBB_LIBRARIES)
    set (LIBS ${LIBS} optimized ${LIBRARY})
  endforeach (LIBRARY)
  foreach (LIBRARY IN LISTS TBB_DEBUG_LIBRARIES)
    set (LIBS ${LIBS} debug ${LIBRARY})
  endforeach (LIBRARY)
else (TBB_DEBUG_FOUND)
  set (LIBS ${LIBS} ${TBB_LIBRARIES})
endif (TBB_DEBUG_FOUND)

find_package (LIBICONV)
find_package (ICU)

if (LIBICONV_FOUND)
  set (USE_ICONV ON CACHE BOOL "Use libiconv for character decoding and encoding. Takes precedence over USE_ICU.")
  set (USE_ICU OFF CACHE BOOL "Use ICU for character decoding and encoding.")
elseif (ICU_FOUND)
  set (USE_ICONV OFF CACHE BOOL "Use libiconv for character decoding and encoding. Takes precedence over USE_ICU.")
  set (USE_ICU ON CACHE BOOL "Use ICU for character decoding and encoding.")
else (LIBICONV_FOUND)
  message (FATAL_ERROR "Neither libiconv nor ICU have been found.")
endif (LIBICONV_FOUND)

if (USE_ICONV)
  include_directories (${LIBICONV_INCLUDE_DIRS})
  link_directories (${LIBICONV_LIBRARY_DIRS})
  set (LIBS ${LIBS} ${LIBICONV_LIBRARIES})
  message (STATUS "Using LIBICONV")
elseif (USE_ICU)
  include_directories (${ICU_INCLUDE_DIRS})
  link_directories (${ICU_LIBRARY_DIRS})
  set (LIBS ${LIBS} ${ICU_LIBRARIES})
  message (STATUS "Using ICU")
endif (USE_ICONV)

find_package (PCRECPP REQUIRED)
include_directories (${PCRECPP_INCLUDE_DIRS})
link_directories (${PCRECPP_LIBRARY_DIRS})
set (LIBS ${LIBS} ${PCRECPP_LIBRARIES})

find_package (LTDL REQUIRED)
include_directories (${LTDL_INCLUDE_DIRS})
link_directories (${LTDL_LIBRARY_DIRS})
set (LIBS ${LIBS} ${LTDL_LIBRARIES})

set (CHUNK_SIZE 256 CACHE STRING
     "Number of rough tokens constituting a work unit in the pipeline.")
set (WORK_UNIT_COUNT 8 CACHE STRING
     "The number of token chunks flowing through the pipeline.")
set (ACCUMULATOR_CAPACITY 256 CACHE STRING
     "Maximum size of Quex's accumulator in the TextCleaner stage.")
set (ENCODER_BUFFER_SIZE 512 CACHE STRING
     "The size of the buffer used to hold characters for encoding on output.")
set (QUEX_TOKEN_ID_OFFSET 10000)

set (CMAKE_INSTALL_PREFIX "NOT-USED" CACHE STRING "Not used, see INSTALL_DIR")
set (INSTALL_DIR $ENV{HOME}/trtok CACHE STRING
     "The installation directory for trtok.")
mark_as_advanced (CMAKE_INSTALL_PREFIX)


file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/quex_files)
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/configured_headers)
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/code)


configure_file (${CMAKE_CURRENT_SOURCE_DIR}/configuration.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/configured_headers/configuration.hpp
               ESCAPE_QUOTES)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/roughtok/roughtok_wrapper.cpp.in
                ${CMAKE_CURRENT_BINARY_DIR}/code/roughtok_wrapper.cpp)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/roughtok/roughtok_wrapper.hpp
                ${CMAKE_CURRENT_BINARY_DIR}/code/roughtok_wrapper.hpp COPYONLY)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/roughtok/no_init_exception.hpp
                ${CMAKE_CURRENT_BINARY_DIR}/code/no_init_exception.hpp
                COPYONLY)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/CMake/FindICU.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/code/FindICU.cmake COPYONLY)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/CMake/FindLIBICONV.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/code/FindLIBICONV.cmake COPYONLY)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/CMake/LibFindMacros.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/code/LibFindMacros.cmake COPYONLY)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/roughtok/CMakeLists.txt.in
                ${CMAKE_CURRENT_BINARY_DIR}/code/CMakeLists.txt @ONLY)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/python/analyze.py
                ${CMAKE_CURRENT_BINARY_DIR}/analyze COPYONLY)


if (NOT IS_DIRECTORY $ENV{QUEX_PATH})
  message (FATAL_ERROR
    "The environment variable QUEX_PATH doesn't point to a valid directory.")
endif (NOT IS_DIRECTORY $ENV{QUEX_PATH})

if (WIN32)
  find_program (QUEX NAMES quex quex.bat quex-exe.py REQUIRED
                HINTS ENV QUEX_PATH DOC "Path to Quex's executable."
                NO_DEFAULT_PATH)
  if(NOT QUEX)
    find_program (QUEX NAMES quex quex.bat quex-exe.py REQUIRED
                  DOC "Path to Quex's executable.")
  endif(NOT QUEX)
else (WIN32)
  find_program (QUEX NAMES quex quex-exe.py quex.bat REQUIRED
                HINTS ENV QUEX_PATH DOC "Path to Quex's executable."
                NO_DEFAULT_PATH)
  if(NOT QUEX)
    find_program (QUEX NAMES quex quex-exe.py quex.bat REQUIRED
                  DOC "Path to Quex's executable.")
  endif(NOT QUEX)
endif (WIN32)

if (USE_ICONV)
  set (QUEX_CONVERTER "--iconv")
elseif (USE_ICU)
  set (QUEX_CONVERTER "--icu")
endif (USE_ICONV)

set (QUEX_ENTITY quex_files/trtok_clean_entities_EntityCleaner)
add_custom_command (OUTPUT ${QUEX_ENTITY} ${QUEX_ENTITY}.cpp
                           ${QUEX_ENTITY}-token ${QUEX_ENTITY}-configuration
                           ${QUEX_ENTITY}-token_ids
    COMMAND ${QUEX} --mode-files   ${CMAKE_CURRENT_SOURCE_DIR}/clean_entities.qx
                    --engine                trtok::clean_entities::EntityCleaner
                    --buffer-element-size                                      4
                    --token-id-prefix                        QUEX_PREPROC_NOXML_
                    --odir                ${CMAKE_CURRENT_BINARY_DIR}/quex_files
                    ${QUEX_CONVERTER}
    DEPENDS clean_entities.qx VERBATIM
    COMMENT "Building EntityCleaner with quex")

set (QUEX_XML quex_files/trtok_clean_xml_XmlCleaner)
add_custom_command (OUTPUT ${QUEX_XML} ${QUEX_XML}.cpp ${QUEX_XML}-token
                           ${QUEX_XML}-configuration ${QUEX_XML}-token_ids
    COMMAND ${QUEX} --mode-files        ${CMAKE_CURRENT_SOURCE_DIR}/clean_xml.qx
                    --engine                        trtok::clean_xml::XmlCleaner
                    --buffer-element-size                                      4
                    --token-id-prefix                      QUEX_PREPROC_WITHXML_
                    --odir                ${CMAKE_CURRENT_BINARY_DIR}/quex_files
                    ${QUEX_CONVERTER}
    DEPENDS clean_xml.qx VERBATIM
    COMMENT "Building XmlCleaner with quex")

set (QUEX_FEATURES quex_files/trtok_read_features_FeaturesReader)
add_custom_command (OUTPUT ${QUEX_FEATURES} ${QUEX_FEATURES}.cpp
                           ${QUEX_FEATURES}-token
                           ${QUEX_FEATURES}-configuration
                           ${QUEX_FEATURES}-token_ids
    COMMAND ${QUEX} --mode-files    ${CMAKE_CURRENT_SOURCE_DIR}/read_features.qx
                    --engine                trtok::read_features::FeaturesReader
                    --buffer-element-size                                      4
                    --token-id-prefix                             QUEX_FEATURES_
                    --odir                ${CMAKE_CURRENT_BINARY_DIR}/quex_files
                    ${QUEX_CONVERTER}
    DEPENDS read_features.qx VERBATIM
    COMMENT "Building FeaturesReader with quex")


include_directories (${CMAKE_CURRENT_SOURCE_DIR}
                     ${CMAKE_CURRENT_BINARY_DIR}/configured_headers
                     ${CMAKE_CURRENT_BINARY_DIR}/quex_files
                     $ENV{QUEX_PATH})

add_definitions ("-DQUEX_OPTION_ASSERTS_DISABLED")

set (SRCS main.cpp TextCleaner.cpp ${QUEX_ENTITY}.cpp ${QUEX_XML}.cpp
    roughtok_compile.cpp RoughTokenizer.cpp OutputFormatter.cpp
    Encoder.cpp FeatureExtractor.cpp Classifier.cpp ${QUEX_FEATURES}.cpp
    read_features_file.cpp SimplePreparer.cpp)

add_executable (trtok ${SRCS})

# The gold linker now in use in modern GNU/Linux distributions such as
# Debian or Fedora no longer automatically links libraries for unresolved
# symbols in linked libraries. These platform-specific libraries must
# therefore be linked manually.

# However, not everybody has Fortran, so we should make its inclusion
# conditional (possibility to opt-out).
set (LINK_GFORTRAN_ON_UNIX "YES" CACHE BOOL
     "Should libgfortran be linked in a UNIX environment? Due to changes in GNU/Linux linker behavior, several libraries are given to the linker explicitly, including libgfortran. If you did not build your Maxent Toolkit with Fortran and do not have libgfortran on your system, you will want to set this option to NO.")
if (UNIX)
  set (LIBS ${LIBS} pthread rt)
  if (LINK_GFORTRAN_ON_UNIX)
    set (LIBS ${LIBS} gfortran)
  endif (LINK_GFORTRAN_ON_UNIX)
endif (UNIX)

target_link_libraries (trtok ${LIBS})


install (TARGETS trtok DESTINATION ${INSTALL_DIR})
install (PROGRAMS python/analyze.py DESTINATION ${INSTALL_DIR} RENAME analyze)
install (DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/code
         DESTINATION ${INSTALL_DIR})
install (DIRECTORY ../models/schemes DESTINATION ${INSTALL_DIR})
install (DIRECTORY ../models/build DESTINATION ${INSTALL_DIR})
install (CODE "message (\"Don't forget to set the environment variable TRTOK_PATH=${INSTALL_DIR}.\")")
