cmake_minimum_required (VERSION 2.8)
project (TrainableTokenizer)


set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMake/")
set (LIBS)

set (Boost_USE_STATIC_LIBS ON)
set (Boost_USE_MULTITHREADED ON)
set (Boost_USE_STATIC_RUNTIME OFF)
find_package (Boost REQUIRED program_options filesystem system thread)
include_directories (${Boost_INCLUDE_DIRS})
link_directories (${Boost_LIBRARY_DIRS})
set (LIBS ${LIBS} ${Boost_LIBRARIES})

find_package (MaxEnt REQUIRED)
include_directories (${MaxEnt_INCLUDE_DIRS})
link_directories (${MaxEnt_LIBRARY_DIRS})
set (LIBS ${LIBS} ${MaxEnt_LIBRARIES})

find_package (TBB REQUIRED)
include_directories (${TBB_INCLUDE_DIRS})
link_directories (${TBB_LIBRARY_DIRS})
set (LIBS ${LIBS} ${TBB_LIBRARIES})

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

set (CHUNK_SIZE 100 CACHE STRING "Number of rough tokens constituting a work unit in the pipeline.")
set (ACCUMULATOR_CAPACITY 256 CACHE STRING "Maximum size of Quex's accumulator in the TextCleaner stage.")
set (QUEX_TOKEN_ID_OFFSET 10000)


file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/quex_files)
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/configured_headers)
file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/code)


configure_file (${CMAKE_CURRENT_SOURCE_DIR}/configuration.hpp.cmake
		${CMAKE_CURRENT_BINARY_DIR}/configured_headers/configuration.hpp)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/rough_tok/rough_tok_wrapper.cpp.cmake
		${CMAKE_CURRENT_BINARY_DIR}/code/rough_tok_wrapper.cpp)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/rough_tok/rough_tok_wrapper.hpp
		${CMAKE_CURRENT_BINARY_DIR}/code/rough_tok_wrapper.hpp COPYONLY)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/CMake/FindICU.cmake
		${CMAKE_CURRENT_BINARY_DIR}/code/FindICU.cmake COPYONLY)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/CMake/FindLIBICONV.cmake
		${CMAKE_CURRENT_BINARY_DIR}/code/FindLIBICONV.cmake COPYONLY)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/rough_tok/CMakeLists.txt.cmake
		${CMAKE_CURRENT_BINARY_DIR}/code/CMakeLists.txt @ONLY)


find_program (QUEX quex quex.bat quex-exe.py REQUIRED HINTS ENV QUEX_PATH DOC "Path to Quex's executable.")
if (USE_ICONV)
  set (QUEX_CONVERTER "--iconv")
elseif (USE_ICU)
  set (QUEX_CONVERTER "--icu")
endif (USE_ICONV)

set (QUEX_ENTITY quex_files/trtok_clean_entities_EntityCleaner)
add_custom_command (OUTPUT ${QUEX_ENTITY} ${QUEX_ENTITY}.cpp ${QUEX_ENTITY}-token
			   ${QUEX_ENTITY}-configuration ${QUEX_ENTITY}-token_ids
		    COMMAND quex --mode-files		${CMAKE_CURRENT_SOURCE_DIR}/clean_entities.qx
				 --engine	        trtok::clean_entities::EntityCleaner
				 --buffer-element-size	4
				 --token-prefix		QUEX_PREPROC_NOXML_
				 --odir			${CMAKE_CURRENT_BINARY_DIR}/quex_files
				 ${QUEX_CONVERTER}
		    DEPENDS clean_entities.qx VERBATIM
		    COMMENT "Building EntityCleaner with quex")

set (QUEX_XML quex_files/trtok_clean_xml_XmlCleaner)
add_custom_command (OUTPUT ${QUEX_XML} ${QUEX_XML}.cpp ${QUEX_XML}-token
			   ${QUEX_XML}-configuration ${QUEX_XML}-token_ids
		    COMMAND quex --mode-files		${CMAKE_CURRENT_SOURCE_DIR}/clean_xml.qx
			         --engine		trtok::clean_xml::XmlCleaner
				 --buffer-element-size	4
				 --token-prefix		QUEX_PREPROC_WITHXML_
				 --odir			${CMAKE_CURRENT_BINARY_DIR}/quex_files
				 ${QUEX_CONVERTER}
		    DEPENDS clean_xml.qx VERBATIM
		    COMMENT "Building XmlCleaner with quex")


include_directories (${CMAKE_CURRENT_SOURCE_DIR}
		     ${CMAKE_CURRENT_BINARY_DIR}/configured_headers
		     ${CMAKE_CURRENT_BINARY_DIR}/quex_files
		     $ENV{QUEX_PATH})

add_definitions (--std=c++0x)

set (SRCS main.cpp text_cleaner.cpp ${QUEX_ENTITY}.cpp ${QUEX_XML}.cpp
	  rough_tok_compile.cpp utils.cpp)
add_executable (trtok ${SRCS})

target_link_libraries (trtok ${LIBS})
