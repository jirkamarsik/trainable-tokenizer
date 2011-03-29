#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <cstring>
#include <cstdint>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "config_exception.hpp"
#include "utils.hpp"

namespace fs = boost::filesystem;

namespace trtok {

/* Checks that the files in our list of files match those listed in the file. */
inline bool check_file_lists_match(std::vector<fs::path> const &file_list,
				   std::istream &file_list_stream) {
	std::string file_name;

	typedef std::vector<fs::path>::const_iterator iter;
	for (iter i = file_list.begin(); i != file_list.end(); i++) {
		if (!file_list_stream) {
			// We have more files than is written in the file
			return false;
		}
		getline(file_list_stream, file_name);
		if (fs::path(file_name) != *i) {
			// We have different file from what is in the file
			return false;
		}
	}

	// So far so good, the diff between the two sets of files is empty
	return true;
}

/* Reads and stores pairs of tab-delimited expressions from a list of files.  */
typedef std::pair<std::string, std::string> context_t;
void read_contexts(std::vector<fs::path> const &files, std::vector<context_t> &contexts) {
	for (std::vector<fs::path>::const_iterator i = files.begin(); i != files.end(); i++) {
		fs::ifstream file(*i);
		int line_number = 0;
		
		while (file) {
			std::string line;
			getline(file, line);
			line_number++;
			if ((line.length() == 0) || (line[0] == '#')) {
				continue;
			}

			// There is no easy way to split a string in C++
			// without referring to Boost or C strings.
			char *line_data = new char[line.length() + 1];
			line.copy(line_data, line.length());
			line_data[line.length()] = '\0';
			char *prefix = strtok(line_data, "\t");
			char *suffix = strtok(NULL, "\t");
			if (prefix == NULL) {
				// An empty line.
				;
			} else if (suffix == NULL) {
				// Only 1 regex.
				std::ostringstream oss;
				oss << i->string() << ":" << line_number << ": Error: Missing regular expression describing suffix.";
				throw config_exception(oss.str().c_str());
			}
			else if (strtok(NULL, "\t") != NULL) {
				// More than 1 regex.
				std::ostringstream oss;
				oss << i->string() << ":" << line_number << ": Error: More than 2 tab-delimited expressions on a line.";
				throw config_exception(oss.str().c_str());
			} else {
				contexts.push_back(std::make_pair(std::string(prefix), std::string(suffix)));
			}

			delete[] line_data;
		}

		file.close();
	}
}

/* Reads UTF-8 characters from the specified files and converts them to unicode code points.
 * If the files contain a blank line, the \r\n code points are stored as well. If these
 * two characters are somehow present elsewhere in the file, they are ignored to prevent
 * accidentally including them in the set. */
void read_character_set(std::vector<fs::path> const &charset_files, std::vector<uint32_t> &charset) {
	for (std::vector<fs::path>::const_iterator i = charset_files.begin();
	     i != charset_files.end(); i++) {
		fs::ifstream file(*i);
		bool last_line_was_blank = false;

		while (file) {
			std::string line;
			getline(file, line);

			if (last_line_was_blank) {
				// An empty line stands for the newline character.
				last_line_was_blank = false;
				charset.push_back(0x0A);
				charset.push_back(0x0D);
			}
			if (line.length() == 0) {
				// getline gives us a blank file at the end of every file
				last_line_was_blank = true;
			}

			std::basic_string<uint32_t> codepoints = utf8_to_unicode(line);
			std::remove_copy_if(codepoints.begin(), codepoints.end(),
					std::back_inserter(charset), is_newline);
		}

		file.close();
	}
}

/* A helper function for the following function. */
std::string mode_name(bool entity_mode, bool may_split, bool may_join, bool may_break_sentence) {
	std::ostringstream oss;
	if (!may_split && !may_join && !may_break_sentence) {
		if (entity_mode) {
			return "ENTITY_MODE_READ_ENTITY";
		}
		else {
			return "READ_ON";
		}
	}
	oss << (entity_mode ? "ENTITY_MODE_" : "")
	    << ("FIND")
	    << (may_split ? "_SPLITS" : "")
	    << (may_join ? "_JOINS" : "")
	    << (may_break_sentence ? "_BREAKS" : "");
	return oss.str();
}

/* Prints a part of the generated .qx file.*/
void print_mode(std::ostream &quex_file, bool entity_mode, bool may_split, bool may_join,
		bool may_break_sentence, int n_split_contexts, int n_join_contexts,
		std::string start_mode) {
	quex_file << "mode " << mode_name(entity_mode, may_split, may_join, may_break_sentence) << " {\n";
	
	if (may_split) {
		for (int i = 0; i < n_split_contexts; i++) {
			quex_file <<
"  {SPLIT_PREFIX_" << i+1 << "}/{SPLIT_SUFFIX_" << i+1 << "}/\t\t\t{ flush_accumulator();\n"
"\t\t\t\t\t\t\t  self_send(QUEX_ROUGH_MAY_SPLIT);\n"
"\t\t\t\t\t\t\t  self.undo();\n"
"\t\t\t\t\t\t\t  self << " << mode_name(entity_mode, false, may_join, may_break_sentence) << "; }\n";
		}
	}

	if (may_join) {
		for (int i = 0; i < n_join_contexts; i++) {
			quex_file <<
"  {JOIN_PREFIX_" << i+1 << "}/\\P{White_Space}+{JOIN_SUFFIX_" << i+1 << "}/\t{ flush_accumulator();\n"
"\t\t\t\t\t\t\t  self_send(QUEX_ROUGH_MAY_JOIN);\n"
"\t\t\t\t\t\t\t  self.undo();\n"
"\t\t\t\t\t\t\t  self << " << mode_name(entity_mode, may_split, false, may_break_sentence) << "; }\n";
		}
	}

	if (may_break_sentence) {
		quex_file <<
"  {SENTENCE_ENDING_MARKER}/.|\\n/\t\t\t{ flush_accumulator();\n"
"\t\t\t\t\t\t\t  self_send(QUEX_ROUGH_MAY_BREAK_SENTENCE);\n"
"\t\t\t\t\t\t\t  self.undo();\n"
"\t\t\t\t\t\t\t  self << " << mode_name(entity_mode, may_split, may_join, false) << "; }\n"
"  {SENTENCE_BEGINNING_MARKER}\t\t\t\t{ flush_accumulator();\n"
"\t\t\t\t\t\t\t  self_send(QUEX_ROUGH_MAY_BREAK_SENTENCE);\n"
"\t\t\t\t\t\t\t  self.undo();\n"
"\t\t\t\t\t\t\t  self << " << mode_name(entity_mode, may_split, may_join, false) << "; }\n";
	}

	if (entity_mode) {
		if (may_split || may_join || may_break_sentence) {
			quex_file <<
"  <<FAIL>>\t\t\t\t\t\t{ self.undo();\n"
"\t\t\t\t\t\t\t  self << ENTITY_MODE_READ_ENTITY; }\n";
		}
		else {
			quex_file <<
"  &{XML_NAME};\t\t\t\t\t\t{ self_send1(QUEX_ROUGH_TOKEN_PIECE, Lexeme);\n"
"\t\t\t\t\t\t\t  self << " << start_mode << "; }\n";
		}
	}
	else {
		quex_file <<
"  &/{XML_NAME};\t\t\t\t\t\t{ flush_accumulator();\n"
"\t\t\t\t\t\t\t  self.undo();\n"
"\t\t\t\t\t\t\t  self << ENTITY_MODE_" << start_mode << "; }\n"
"  [:inverse(\\P{White_Space}):]\t\t\t\t{ if (self.ws_newlines != -1)\n"
"\t\t\t\t\t\t\t  {\n"
"\t\t\t\t\t\t\t\tsend_whitespace();\n"
"\t\t\t\t\t\t\t  }\n"
"\t\t\t\t\t\t\t  self_accumulator_add_character(*LexemeBegin);\n"
"\t\t\t\t\t\t\t  self.accumulator_size++;\n"
"\t\t\t\t\t\t\t  self << " << start_mode << "; }\n"
"  \\n\t\t\t\t\t\t\t{ if (self.ws_newlines == -1)\n"
"\t\t\t\t\t\t\t  {\n"
"\t\t\t\t\t\t\t\tflush_accumulator();\n"
"\t\t\t\t\t\t\t\tself.ws_newlines = 1;\n"
"\t\t\t\t\t\t\t  }\n"
"\t\t\t\t\t\t\t  else\n"
"\t\t\t\t\t\t\t\tself.ws_newlines++;\n"
"\t\t\t\t\t\t\t  self << " << start_mode << "; }\n"
"  \\P{White_Space}\t\t\t\t\t{ if (self.ws_newlines == -1)\n"
"\t\t\t\t\t\t\t  {\n"
"\t\t\t\t\t\t\t\tflush_accumulator();\n"
"\t\t\t\t\t\t\t\tself.ws_newlines = 0;\n"
"\t\t\t\t\t\t\t  }\n"
"\t\t\t\t\t\t\t  self << " << start_mode << "; }\n"
"  <<EOF>>\t\t\t\t\t\t{ flush_accumulator();\n"
"\t\t\t\t\t\t\t  if (self.ws_newlines != -1)\n"
"\t\t\t\t\t\t\t  {\n"
"\t\t\t\t\t\t\t\tsend_whitespace();\n"
"\t\t\t\t\t\t\t  }\n"
"\t\t\t\t\t\t\t  self_send(QUEX_ROUGH_TERMINATION); }\n";
	}

	quex_file << "}\n";
}

bool compile_rough_lexer(std::vector<fs::path> const &split_files,
			 std::vector<fs::path> const &join_files,
			 std::vector<fs::path> const &begin_files,
			 std::vector<fs::path> const &end_files,
			 fs::path const &build_path)
{
	fs::current_path(build_path);
	
	bool files_changed = false;

	/* The set of files specifying the rough tokenizer's behaviour
	 * might have changed between invocations. If so, we have to
	 * rebuild the rough tokenizer. The set of files from which
	 * we built the tokenizer the last item is stored in a file. */
	fs::path file_list_path("rough_tok_files");
	if (fs::exists(file_list_path) && !fs::is_directory(file_list_path)) {
		fs::ifstream file_list_stream(file_list_path);
		bool file_set_changed = false;
		
		if (!check_file_lists_match(split_files, file_list_stream)) {
			file_set_changed = true;
		}
		if (!file_set_changed && !check_file_lists_match(join_files, file_list_stream)) {
			file_set_changed = true;
		}
		if (!file_set_changed && !check_file_lists_match(begin_files, file_list_stream)) {
			file_set_changed = true;
		}
		if (!file_set_changed && !check_file_lists_match(end_files, file_list_stream)) {
			file_set_changed = true;
		}
		if (!file_set_changed && file_list_stream) {
			// There are still more filenames in the file
			file_set_changed = true;
		}
		file_list_stream.close();

		if (file_set_changed) {
			files_changed = true;
		} else {
			/* Although the same set of files has been used to generate
			 * the last lexer, the files may have been modified since.
			 * The CMake generated build system may or may not perform
			 * this timestamp check and the system-call to cmake and the
			 * build command might be too costly for every trtok startup,
			 * so we check the filestamps by hand first. */
			std::time_t compiled_time = fs::last_write_time(file_list_path);

			typedef std::vector<fs::path>::const_iterator iter;
			iter i = split_files.begin();
			while (!files_changed && (i != end_files.end())) {
				if (i == split_files.end())
					i = join_files.begin();
				else if (i == join_files.end())
					i = begin_files.begin();
				else if (i == begin_files.end())
					i = end_files.begin();
				if (fs::last_write_time(*i) >= compiled_time) {
					files_changed = true;
				}
				i++;
			}
		}
	} else {
		// We have never built this rough tokenizer
		files_changed = true;
	}

		
	if (files_changed) {
		std::vector<context_t> split_contexts, join_contexts;
		std::vector<uint32_t> begin_chars, end_chars;

		read_contexts(split_files, split_contexts);
		read_contexts(join_files, join_contexts);

		read_character_set(begin_files, begin_chars);
		read_character_set(end_files, end_chars);

		//Debugging code
		///*
		std::cout << "Split contexts:" << std::endl;
		for (std::vector<context_t>::const_iterator i = split_contexts.begin();
		     i != split_contexts.end(); i++) {
			std::cout << i->first << "\t" << i->second << std::endl;
		}
		std::cout << std::endl;

		std::cout << "Join contexts:" << std::endl;
		for (std::vector<context_t>::const_iterator i = join_contexts.begin();
		     i != join_contexts.end(); i++) {
			std::cout << i->first << "\t" << i->second << std::endl;
		}
		std::cout << std::endl;

		std::cout << "Sentence begin chars:" << std::endl;
		for (std::vector<uint32_t>::const_iterator i = begin_chars.begin();
		     i != begin_chars.end(); i++) {
			std::cout << *i << std::endl;
		}
		std::cout << std::endl;

		std::cout << "Sentence end chars:" << std::endl;
		for (std::vector<uint32_t>::const_iterator i = end_chars.begin();
		     i != end_chars.end(); i++) {
			std::cout << *i << std::endl;
		}
		std::cout << std::endl;
		//*/
		

		// GENERATING THE QUEX FILE
		fs::path quex_file_path = fs::path("RoughLexer.qx");
		fs::ofstream quex_file(quex_file_path);

		bool have_splits = split_contexts.size();
		bool have_joins = join_contexts.size();
		bool have_sentence_breaks = (begin_chars.size() + end_chars.size());
		std::string start_mode = mode_name(false, have_splits, have_joins, have_sentence_breaks);
		quex_file << 

"start = " << start_mode << ";\n"
"\n"
"define {\n";

		for (std::vector<context_t>::size_type i = 0; i != split_contexts.size(); i++) {
			quex_file << "  SPLIT_PREFIX_" << i+1 << " " << split_contexts[i].first << "\n";
			quex_file << "  SPLIT_SUFFIX_" << i+1 << " " << split_contexts[i].second << "\n";
		}

		for (std::vector<context_t>::size_type i = 0; i != join_contexts.size(); i++) {
			quex_file << "  JOIN_PREFIX_" << i+1 << " " << join_contexts[i].first << "\n";
			quex_file << "  JOIN_SUFFIX_" << i+1 << " " << join_contexts[i].second << "\n";
		}

		quex_file << std::hex;
		quex_file << "  SENTENCE_BEGINNING_MARKER [";
		for (std::vector<uint32_t>::const_iterator i = begin_chars.begin(); i != begin_chars.end(); i++) {
			quex_file << "\\U" << *i;
		}
		quex_file << "]\n";

		quex_file << "  SENTENCE_ENDING_MARKER [";
		for (std::vector<uint32_t>::const_iterator i = end_chars.begin(); i != end_chars.end(); i++) {
			quex_file << "\\U" << *i;
		}
		quex_file << "]\n";
		quex_file << std::dec;

		quex_file << 
"  XML_NAME_START_CHAR \":\"|[A-Z]|\"_\"|[a-z]|[\\UC0-\\UD6]|[\\UD8-\\UF6]|[\\UF8-\\U2FF]|[\\U370-\\U37D]|[\\U37F-\\U1FFF]|[\\U200C-\\U200D]|[\\U2070-\\U218F]|[\\U2C00-\\U2FEF]|[\\U3001-\\UD7FF]|[\\UF900-\\UFDCF]|[\\UFDF0-\\UFFFD]|[\\U10000-\\UEFFFF]\n"
"  XML_NAME_CHAR {XML_NAME_START_CHAR}|\"-\"|\".\"|[0-9]|\\UB7|[\\U0300-\\U036F]|[\\U203F-\\U2040]\n"
"  XML_NAME {XML_NAME_START_CHAR}{XML_NAME_CHAR}*\n"
"}\n"
"\n"
"token {\n"
"  TOKEN_PIECE;\n"
"  MAY_BREAK_SENTENCE;\n"
"  MAY_SPLIT;\n"
"  MAY_JOIN;\n"
"  WHITESPACE;\n"
"  LINE_BREAK;\n"
"  PARAGRAPH_BREAK;\n"
"}\n"
"\n"
"header {\n"
"#define flush_accumulator() if (self.accumulator_size > 0)\\\n"
"			    {\\\n"
"			    	self_accumulator_flush(QUEX_ROUGH_TOKEN_PIECE);\\\n"
"				self.accumulator_size = 0;\\\n"
"			    }\n"
"\n"
"#define send_whitespace() if (self.ws_newlines == 0)\\\n"
"				self_send(QUEX_ROUGH_WHITESPACE);\\\n"
"			  else if (self.ws_newlines == 1)\\\n"
"				self_send(QUEX_ROUGH_LINE_BREAK);\\\n"
"			  else\\\n"
"				self_send(QUEX_ROUGH_PARAGRAPH_BREAK);\\\n"
"			  self.ws_newlines = -1;\n"
"}\n"
"\n"
"body {\n"
"int ws_newlines, accumulator_size;\n"
"}\n"
"\n"
"init {\n"
"  self.ws_newlines = -1;\n"
"  self.accumulator_size = 0;\n"
"}\n"
"\n";

		bool bools[2] = {false, true};
		for (int entity = 0; entity < 2; entity++)
			for (int split = 0; split < (have_splits ? 2 : 1); split++)
				for (int join = 0; join < (have_joins ? 2 : 1); join++)
					for (int sentence_break = 0; sentence_break < (have_sentence_breaks ? 2 : 1); sentence_break ++) {
						print_mode(quex_file, bools[entity], bools[split], bools[join], bools[sentence_break],
							   split_contexts.size(), join_contexts.size(), start_mode);
						quex_file << std::endl;
					}

		quex_file.close();
	}
}

}
