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
				   fs::fstream &file_list_stream) {
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
		fs::fstream file(*i);
		int line_number = 1;
		
		while (file) {
			std::string line;
			getline(file, line);
			if ((line.length() == 0) ||  (line[0] == '#')) {
				continue;
			}

			// There is no easy way to split a string in C++
			// without referring to Boost or C strings.
			char *line_data = new char[line.length() + 1];
			line.copy(line_data, line.length());
			line_data[line.length() + 1];
			char *prefix = strtok(line_data, "\t");
			char *suffix = strtok(NULL, "\t");
			if (prefix == NULL) {
				// An empty line.
				continue;
			} else if (suffix == NULL) {
				// Only 1 regex.
				std::ostringstream oss;
				oss << *i << ":" << line_number << ": Error: Missing regular expression describing suffix.";
				throw config_exception(oss.str().c_str());
			}
			else if (strtok(NULL, "\t") == NULL) {
				// More than 1 regex.
				std::ostringstream oss;
				oss << *i << ":" << line_number << ": Error: More than 2 tab-delimited expressions on a line.";
				throw config_exception(oss.str().c_str());
			} else {
				contexts.push_back(std::make_pair(std::string(prefix), std::string(suffix)));
			}

			delete[] line_data;
			line_number++;
		}

		file.close();
	}
}

void read_character_set(std::vector<fs::path> const &charset_files, std::vector<uint32_t> &charset) {
	for (std::vector<fs::path>::const_iterator i = charset_files.begin();
	     i != charset_files.end(); i++) {
		fs::fstream file(*i);

		while (file) {
			std::string line;
			getline(file, line);

			if (line.length() == 0) {
				// An empty line stands for the newline character.
				charset.push_back(10);
				continue;
			}

			std::basic_string<uint32_t> codepoints = utf8_to_unicode(line);
			std::copy(codepoints.begin(), codepoints.end(), std::back_inserter(charset));
		}

		file.close();
	}
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
		fs::fstream file_list_stream(file_list_path);
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
	}

		
	if (files_changed) {
		std::vector<context_t> split_contexts, join_contexts;
		std::vector<uint32_t> begin_chars, end_chars;

		read_contexts(split_files, split_contexts);
		read_contexts(join_files, join_contexts);

		read_character_set(begin_files, begin_chars);
		read_character_set(end_files, end_chars);
	}
}

}
