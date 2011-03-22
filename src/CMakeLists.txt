cmake_minimum_required (VERSION 2.6.4)

project (TrainableTokenizer)

find_program (QUEX quex.bat quex PATHS ENV QUEX_PATH)

add_custom_command (OUTPUT trtok_clean_entities_EntityCleaner
			   trtok_clean_entities_EntityCleaner.cpp
			   trtok_clean_entities_EntityCleaner-configuration
			   trtok_clean_entities_EntityCleaner-token_ids
			   trtok_clean_entities_EntityCleaner-token
		    COMMAND quex --mode-files		${CMAKE_CURRENT_SOURCE_DIR}/clean_entities.qx
				 --engine	        trtok::clean_entities::EntityCleaner
				 --buffer-element-size	4
				 --token-prefix		QUEX_PREPROC_NOXML_
				 --iconv
		    DEPENDS clean_entities.qx VERBATIM
		    COMMENT "Building EntityCleaner...")

add_custom_command (OUTPUT trtok_clean_xml_XmlCleaner
			   trtok_clean_xml_XmlCleaner.cpp
			   trtok_clean_xml_XmlCleaner-configuration
			   trtok_clean_xml_XmlCleaner-token_ids
			   trtok_clean_xml_XmlCleaner-token
		    COMMAND quex --mode-files		${CMAKE_CURRENT_SOURCE_DIR}/clean_xml.qx
			         --engine		trtok::clean_xml::XmlCleaner
				 --buffer-element-size	4
				 --token-prefix		QUEX_PREPROC_WITHXML_
				 --iconv
		    DEPENDS clean_xml.qx VERBATIM
		    COMMENT "Building XmlCleaner...")

include_directories (${CMAKE_CURRENT_BINARY_DIR} $ENV{QUEX_PATH} .)

add_executable (trtok main.cpp text_cleaner.cpp trtok_clean_entities_EntityCleaner.cpp trtok_clean_xml_XmlCleaner.cpp)

target_link_libraries (trtok boost_program_options tbb)
