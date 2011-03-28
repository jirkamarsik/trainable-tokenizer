#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace fs = boost::filesystem;

namespace trtok {

inline bool check_file_sets_match(std::vector<fs::path> const &file_set,
				  fs::fstream &file_list_stream) {
	std::string file_name;

	typedef std::vector<fs::path>::const_iterator iter;
	for (iter i = file_set.begin(); i != file_set.end(); i++) {
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

void compile_rough_lexer(std::vector<fs::path> const &split_files,
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
		
		if (!check_file_sets_match(split_files, file_list_stream)) {
			file_set_changed = true;
		{
		if (!file_set_changed && !check_file_sets_match(join_files, file_list_stream)) {
			file_set_changed = true;
		}
		if (!file_set_changed && !check_file_sets_match(begin_files, file_list_stream)) {
			file_set_changed = true;
		}
		if (!file_set_changed && !check_file_sets_match(end_files, file_list_stream)) {
			file_set_changed = true;
		}
		if (!file_set_changed && file_list_stream) {
			// There are still more filenames in the file
			file_set_changed = true;
		}

		if (file_set_changed) {
			files_changed = true;
		} else {
			/* Although the same set of files has been used to generate
			 * the last lexer, the files may have been modified since.
			 * The CMake generated build system may or may not perform
			 * this timestamp check and the system-call to cmake and the
			 * build command might be too costly for every trtok startup,
			 * so we check the filestamps by hand first. */
			compiled_time = fs::last_write_time(file_list_path);

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

		
		if (files_changed) {
			
		}
	}
}

}
