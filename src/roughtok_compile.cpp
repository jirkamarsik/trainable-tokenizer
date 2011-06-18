#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <iterator>
#include <cstdlib>
#include <ctime>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/cstdint.hpp>
typedef boost::uint32_t uint32_t;

#include "roughtok_compile.hpp"
#include "configuration.hpp"
#include "config_exception.hpp"
#include "utils.hpp"

using namespace std;
namespace fs = boost::filesystem;

#define CHECK_FOR_FILE(filename) {\
    if (!fs::exists(code_path / filename)) {\
      throw config_exception("Error: Missing " #filename " in "\
        "$TRTOK_PATH/code directory. Please restore the code directory to "\
        "its original state or check that the installation succeeded.");\
    }\
}

namespace trtok {

/* Checks that the files in our list of files match those listed in the file.*/
inline bool check_file_lists_match(vector<fs::path> const &file_list,
                                   istream &file_list_istream) {

    string file_name;

    typedef vector<fs::path>::const_iterator iter;
    for (iter i = file_list.begin(); i != file_list.end(); i++) {
        if (!file_list_istream) {
            // We have more files than is written in the file
            return false;
        }
        getline(file_list_istream, file_name);
        if (file_name != i->generic_string()) {
            // We have different file from what is in the file
            return false;
        }
    }

    // So far so good, the diff between the two sets of files is empty
    return true;
}


/* Reads and stores pairs of tab-delimited expressions from a list of files. */
typedef pair<string, string> context_t;
void read_contexts(vector<fs::path> const &files,
                   vector<context_t> &contexts) {

    for (vector<fs::path>::const_iterator file = files.begin();
         file != files.end(); file++) {
      fs::ifstream file_stream(*file);
      int line_number = 0;
      
      string line;
      while (getline(file_stream, line)) {

        line_number++;
        if ((line.length() == 0) || (line[0] == '#')) {
            continue;
        }

        // There is no easy way to split a string in C++
        // without referring to Boost or C strings.
        char *line_data = new char[line.length() + 1];
        line.copy(line_data, line.length());
        line_data[line.length()] = '\0';
        char *prefix = strtok(line_data, " \t");
        char *suffix = strtok(NULL, " \t");
        if (prefix == NULL) {
            // An empty line.
            ;
        } else if (suffix == NULL) {
            // Only 1 regex.
            string error_msg;
            error_msg = file->string() + ":"
                 + boost::lexical_cast<string>(line_number)
                 + ": Error: Missing regular expression describing suffix.";
            throw config_exception(error_msg.c_str());
        }
        else if (strtok(NULL, " \t") != NULL) {
            // More than 1 regex.
            string error_msg;
            error_msg = file->string() + ":"
                 + boost::lexical_cast<string>(line_number)
                 + ": Error: More than 2 tab-delimited expressions on a line.";
            throw config_exception(error_msg.c_str());
        } else {
            contexts.push_back(make_pair(string(prefix), string(suffix)));
        }

        delete[] line_data;
      }

      file_stream.close();
    }
}


/* Reads UTF-8 characters from the specified files and converts them to
 * unicode code points. If the files contain a blank line, the \r\n code
 * points are stored as well. If these two characters are somehow present
 * elsewhere in the file, they are ignored to prevent accidentally including
 * them in the set. */
void read_character_set(vector<fs::path> const &charset_files,
                        vector<uint32_t> &charset) {

    for (vector<fs::path>::const_iterator file = charset_files.begin();
         file != charset_files.end(); file++) {

        fs::ifstream file_stream(*file);

        string line;
        while (getline(file_stream, line)) {

            if (line.length() == 0) {
                // An empty line stands for the newline character.
                charset.push_back(0x0A);
                charset.push_back(0x0D);
            }

            basic_string<uint32_t> codepoints = utf8_to_unicode(line);
            remove_copy_if(codepoints.begin(), codepoints.end(),
                           back_inserter(charset), is_newline);
        }

        file_stream.close();
    }
}

/* A helper function for the following function. */
string mode_name(bool entity_mode,
                 bool may_split,
                 bool may_join,
                 bool may_break_sentence) {

    if (!may_split && !may_join && !may_break_sentence) {
        if (entity_mode) {
            return "ENTITY_MODE_READ_ENTITY";
        }
        else {
            return "READ_ON";
        }
    }
    return string(entity_mode ? "ENTITY_MODE_" : "") + ("FIND")
        + (may_split ? "_SPLITS" : "") + (may_join ? "_JOINS" : "")
        + (may_break_sentence ? "_BREAKS" : "");
}

/* Prints a part of the generated .qx file.*/
void print_mode(ostream &quex_file,
                bool entity_mode,
                bool may_split,
                bool may_join,
                bool may_break_sentence,
                bool have_sentence_begins,
                bool have_sentence_ends, 
                int n_split_contexts,
                int n_join_contexts,
                string start_mode) {

    quex_file << "mode "
    << mode_name(entity_mode, may_split, may_join, may_break_sentence)
    << " {\n\n";
    
    if (may_split) {
      for (int i = 0; i < n_split_contexts; i++) {
        quex_file <<
        "  {SPLIT_PREFIX_" << i+1 << "}/{SPLIT_SUFFIX_" << i+1 << "}/\n"
        "        {\n"
        "          flush_accumulator();\n"
        "          self_send(QUEX_ROUGH_MAY_SPLIT);\n"
        "          self.undo();\n"
        "          self << "
              << mode_name(entity_mode, false, may_join, may_break_sentence)
              << ";\n"
        "        }\n"
        "\n";
      }
    }

    if (may_join) {
      for (int i = 0; i < n_join_contexts; i++) {
        quex_file <<
        "  {JOIN_PREFIX_" << i+1 << "}/"
            "\\P{White_Space}+{JOIN_SUFFIX_" << i+1 << "}/\n"
        "        {\n"
        "          flush_accumulator();\n"
        "          self_send(QUEX_ROUGH_MAY_JOIN);\n"
        "          self.undo();\n"
        "          self << "
              << mode_name(entity_mode, may_split, false, may_break_sentence)
              << ";\n"
        "        }\n"
        "\n";
      }
    }

    if (may_break_sentence) {
      if (have_sentence_begins) {
        quex_file <<
        "  {SENTENCE_BEGINNING_MARKER}\n"
        "        {\n"
        "          flush_accumulator();\n"
        "          self_send(QUEX_ROUGH_MAY_BREAK_SENTENCE);\n"
        "          self.undo();\n"
        "          self << "
              << mode_name(entity_mode, may_split, may_join, false)
              << ";\n"
        "        }\n"
        "\n";
      }
      if (have_sentence_ends) {
        quex_file <<
        "  {SENTENCE_ENDING_MARKER}/.|\\n/\n"
        "        {\n"
        "          flush_accumulator();\n"
        "          self_send(QUEX_ROUGH_MAY_BREAK_SENTENCE);\n"
        "          self.undo();\n"
        "          self << "
              << mode_name(entity_mode, may_split, may_join, false)
              << ";\n"
        "        }\n"
        "\n";
      }
    }

    if (entity_mode) {
      if (may_split || may_join || may_break_sentence) {
        quex_file <<
        "  <<FAIL>>\n"
        "        {\n"
        "          self.undo();\n"
        "          self << ENTITY_MODE_READ_ENTITY;\n"
        "        }\n"
        "\n";
      }
      else {
          quex_file <<
          "  &{XML_NAME};\n"
          "        {\n"
          "          self_send1(QUEX_ROUGH_TOKEN_PIECE, Lexeme);\n"
          "          self << " << start_mode << ";\n"
          "        }\n"
          "\n";
      }
    }
    else {
        quex_file <<
        "  &/{XML_NAME};\n"
        "        {\n"
        "          flush_accumulator();\n"
        "          self.undo();\n"
        "          self << ENTITY_MODE_"
              << (start_mode == "READ_ON" ? "READ_ENTITY" : start_mode)
              << ";\n"
        "        }\n"
        "\n"
        "  [:inverse(\\P{White_Space}):]\n"
        "        {\n"
        "          if (self.ws_newlines != -1) {\n"
        "            send_whitespace();\n"
        "          }\n"
        "          self_accumulator_add_character(*Lexeme);\n"
        "          self.accumulator_size++;\n"
        "          self << " << start_mode << ";\n"
        "        }\n"
        "\n"
        "  \\n\n"
        "        {\n"
        "          if (self.ws_newlines == -1) {\n"
        "            flush_accumulator();\n"
        "            self.ws_newlines = 1;\n"
        "          } else {\n"
        "            self.ws_newlines++;\n"
        "          }\n"
        "          self << " << start_mode << ";\n"
        "        }\n"
        "\n"
        "  \\r/[^\\n]\n"
        "        {\n"
        "          if (self.ws_newlines == -1) {\n"
        "            flush_accumulator();\n"
        "            self.ws_newlines = 1;\n"
        "          } else {\n"
        "            self.ws_newlines++;\n"
        "          }\n"
        "          self << " << start_mode << ";\n"
        "        }\n"
        "\n"
        "  \\P{White_Space}\n"
        "        {\n"
        "          if (self.ws_newlines == -1) {\n"
        "            flush_accumulator();\n"
        "            self.ws_newlines = 0;\n"
        "          }\n"
        "          self << " << start_mode << ";\n"
        "        }\n"
        "\n"
        "  <<EOF>>\n"
        "        {\n"
        "          flush_accumulator();\n"
        "          if (self.ws_newlines != -1) {\n"
        "            send_whitespace();\n"
        "          }\n"
        "          self_send(QUEX_ROUGH_TERMINATION);\n"
        "        }\n"
        "\n";
    }

    quex_file << "}\n";
}


bool compile_rough_lexer(vector<fs::path> const &split_files,
                         vector<fs::path> const &join_files,
                         vector<fs::path> const &begin_files,
                         vector<fs::path> const &end_files,
                         fs::path const &build_path) {

    fs::path original_path = fs::current_path();
    fs::current_path(build_path);
    
    bool files_changed = true;

    /* The set of files specifying the rough tokenizer's behaviour
     * might have changed between invocations. If so, we have to
     * rebuild the rough tokenizer. The set of files from which
     * we built the tokenizer the last item is stored in a file. */
    fs::path file_list_path("roughtok.files");

    if (fs::exists(file_list_path) && !fs::is_directory(file_list_path)) {

      fs::ifstream file_list_istream(file_list_path);

      bool file_set_changed = false;
      
      if (!check_file_lists_match(split_files, file_list_istream)) {
          file_set_changed = true;
      }
      if (!file_set_changed
        && !check_file_lists_match(join_files, file_list_istream)) {
          file_set_changed = true;
      }
      if (!file_set_changed
        && !check_file_lists_match(begin_files, file_list_istream)) {
          file_set_changed = true;
      }
      if (!file_set_changed
        && !check_file_lists_match(end_files, file_list_istream)) {
          file_set_changed = true;
      }
      if (!file_set_changed) {
          // There could still be more filenames in the file
          string line;
          while (getline(file_list_istream, line)) {
              if (line.length() > 0) {
                  file_set_changed = true;
              }
          }
      }
      file_list_istream.close();

      if (!file_set_changed) {
          /* Although the same set of files has been used to generate
           * the last lexer, the files may have been modified since.
           * The CMake generated build system may or may not perform
           * this timestamp check and the system-call to cmake and the
           * build command might be too costly for every trtok startup,
           * so we check the filestamps by hand first. */
          bool newer_filestamps = false;
          time_t compiled_time = fs::last_write_time(file_list_path);

          vector<fs::path>::const_iterator i = split_files.begin();
          while (!newer_filestamps && (i != end_files.end())) {
              if (i == split_files.end())
                  i = join_files.begin();
              if (i == join_files.end())
                  i = begin_files.begin();
              if (i == begin_files.end())
                  i = end_files.begin();
              if (i == end_files.end()) {
                  break;
              }
              if (fs::last_write_time(*i) >= compiled_time) {
                  newer_filestamps = true;
              }
              i++;
          }

          if (!newer_filestamps) {
              files_changed = false;
          }
      }
    }

        
    if (files_changed || !fs::exists(build_path / "roughtok")) {

        vector<context_t> split_contexts, join_contexts;
        vector<uint32_t> begin_chars, end_chars;

        read_contexts(split_files, split_contexts);
        read_contexts(join_files, join_contexts);

        read_character_set(begin_files, begin_chars);
        read_character_set(end_files, end_chars);


        // GENERATING THE QUEX FILE
        fs::path quex_file_path = fs::path("RoughLexer.qx");
        fs::ofstream quex_file(quex_file_path);

        bool have_splits = split_contexts.size() > 0;
        bool have_joins = join_contexts.size() > 0;
        bool have_sentence_begins = begin_chars.size() > 0;
        bool have_sentence_ends = end_chars.size() > 0;
        bool have_sentence_breaks = have_sentence_begins || have_sentence_ends;

        string start_mode =
            mode_name(false, have_splits, have_joins, have_sentence_breaks);

        quex_file << 
        "start = " << start_mode << ";\n"
        "\n"
        "define {\n";

        for (vector<context_t>::size_type i = 0;
             i != split_contexts.size(); i++) {
          quex_file << "  SPLIT_PREFIX_" << i+1 << " "
                    << split_contexts[i].first << "\n";
          quex_file << "  SPLIT_SUFFIX_" << i+1 << " "
                    << split_contexts[i].second << "\n";
        }

        for (std::vector<context_t>::size_type i = 0;
             i != join_contexts.size(); i++) {
          quex_file << "  JOIN_PREFIX_" << i+1 << " "
                    << join_contexts[i].first << "\n";
          quex_file << "  JOIN_SUFFIX_" << i+1 << " "
                    << join_contexts[i].second << "\n";
        }

        if (have_sentence_begins) {
          quex_file << hex;
          quex_file << "  SENTENCE_BEGINNING_MARKER [";
          for (vector<uint32_t>::const_iterator cp = begin_chars.begin();
               cp != begin_chars.end(); cp++) {
            quex_file << "\\U" << *cp;
          }
          quex_file << "]\n";
          quex_file << dec;
        }

        if (have_sentence_ends) {
          quex_file << std::hex;
          quex_file << "  SENTENCE_ENDING_MARKER [";
          for (vector<uint32_t>::const_iterator cp = end_chars.begin();
               cp != end_chars.end(); cp++) {
            quex_file << "\\U" << *cp;
          }
          quex_file << "]\n";
          quex_file << std::dec;
        }

        quex_file << 
        "  XML_NAME_START_CHAR \":\"|[A-Z]|\"_\"|[a-z]|[\\UC0-\\UD6]"
            "|[\\UD8-\\UF6]|[\\UF8-\\U2FF]|[\\U370-\\U37D]|[\\U37F-\\U1FFF]"
            "|[\\U200C-\\U200D]|[\\U2070-\\U218F]|[\\U2C00-\\U2FEF]"
            "|[\\U3001-\\UD7FF]|[\\UF900-\\UFDCF]|[\\UFDF0-\\UFFFD]"
            "|[\\U10000-\\UEFFFF]\n"
        "  XML_NAME_CHAR {XML_NAME_START_CHAR}|\"-\"|\".\"|[0-9]|\\UB7"
            "|[\\U0300-\\U036F]|[\\U203F-\\U2040]\n"
        "  XML_NAME {XML_NAME_START_CHAR}{XML_NAME_CHAR}*\n"
        "}\n"
        "\n"
        "token {\n"
        "  TOKEN_PIECE;\n"
        "  MAY_BREAK_SENTENCE;\n"
        "  MAY_SPLIT;\n"
        "  MAY_JOIN;\n"
        "  WHITESPACE;\n"
        "}\n"
	"\n"
	"token_type {\n"
	"  distinct {\n"
	"    text   : std::basic_string<QUEX_TYPE_CHARACTER>;\n"
	"    n_newlines : int;\n"
	"  }\n"
	"  \n"
	"  take_text {\n"
	"    self.text.assign(Begin, End-Begin);\n"
	"  }\n"
	"}\n"
        "\n"
        "header {\n"
        "\n"
        "#define flush_accumulator() {\\\n"
        "    if (self.accumulator_size > 0) {\\\n"
        "      self_accumulator_flush(QUEX_ROUGH_TOKEN_PIECE);\\\n"
        "      self.accumulator_size = 0;\\\n"
        "    }\\\n"
        "}\n"
        "\n"
        "#define send_whitespace() {\\\n"
        "    self.token_p()->n_newlines = self.ws_newlines;\\\n"
        "    self_send(QUEX_ROUGH_WHITESPACE);\\\n"
        "    self.ws_newlines = -1;\\\n"
        "}\n"
        "\n"
        "}\n"
        "\n"
        "body {\n"
        "  int ws_newlines, accumulator_size;\n"
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
              for (int sentence_break = 0;
                   sentence_break < (have_sentence_breaks ? 2 : 1);
                   sentence_break ++) {
                print_mode(quex_file, bools[entity], bools[split], bools[join],
                           bools[sentence_break], have_sentence_begins,
                           have_sentence_ends, split_contexts.size(),
                           join_contexts.size(), start_mode);
                quex_file << endl;
              }


        quex_file.close();


        // BUILDING THE ROUGHLEXER

        fs::path trtok_path(getenv("TRTOK_PATH"));
        fs::path code_path = trtok_path / fs::path("code");

        CHECK_FOR_FILE("CMakeLists.txt");
        CHECK_FOR_FILE("roughtok_wrapper.cpp");
        CHECK_FOR_FILE("roughtok_wrapper.hpp");
        CHECK_FOR_FILE("no_init_exception.hpp");
        CHECK_FOR_FILE("FindLIBICONV.cmake");
        CHECK_FOR_FILE("FindICU.cmake");

        // Clean out the old generated files so they are not accedintally used.
        fs::path compiled_wrapper_path = build_path / fs::path("roughtok");
        fs::path build_command_file_path =
                                build_path / fs::path ("build_command");

        fs::remove(compiled_wrapper_path);
        fs::remove(file_list_path);
        fs::remove(build_command_file_path);
        fs::remove_all(build_path / "CMakeFiles");
        fs::remove(build_path / "cmake_install.cmake");
        // Clean any temp. builds but keep the cache

        fs::remove(build_path / "RoughLexer.cpp");
        fs::remove(build_path / "RoughLexer");
        fs::remove(build_path / "RoughLexer-token");
        fs::remove(build_path / "RoughLexer-token_ids");
        fs::remove(build_path / "RoughLexer-configuration");

        // Copy the CMake list file...
        fs::path cmake_list_path = code_path / fs::path("CMakeLists.txt");
        fs::copy_file(cmake_list_path, build_path / fs::path("CMakeLists.txt"),
                      fs::copy_option::overwrite_if_exists);

        // and call CMake.
        int return_code = system(CMAKE_COMMAND " .");
        if (return_code != EXIT_SUCCESS) {
            throw config_exception("Error: CMake exited with an error code "
                "when compiling the rough tokenizer.");
        }

        // CMake also writes for us a file on whose single line is a command
        // we need to invoke to build the project on the target system.
        fs::ifstream build_command_file(build_command_file_path);
        string build_command;
        getline(build_command_file, build_command);
        build_command_file.close();

        return_code = system(build_command.c_str());
        if (return_code != EXIT_SUCCESS) {
          throw config_exception("Error: The build system exited with "
              "an error code when compiling the rough tokenizer.");
        }

        // UPDATING roughtok.files
        if (fs::exists(compiled_wrapper_path)) {

            // Finally we write the list of files that defined this lexer
            // and "stamp" them with the current time. (but only if we
            // compiled successfully)
            fs::ofstream file_list_ostream(file_list_path);
    
            typedef vector<fs::path>::const_iterator iter;
            for (iter i = split_files.begin(); i != split_files.end(); i++) {
                file_list_ostream << i->generic_string() << std::endl;
            }
    
            for (iter i = join_files.begin(); i != join_files.end(); i++) {
                file_list_ostream << i->generic_string() << std::endl;
            }
    
            for (iter i = begin_files.begin(); i != begin_files.end(); i++) {
                file_list_ostream << i->generic_string() << std::endl;
            }
    
            for (iter i = end_files.begin(); i != end_files.end(); i++) {
                file_list_ostream << i->generic_string() << std::endl;
            }
    
            file_list_ostream.close();
        }
    }

    // cd to the original working directory
    fs::current_path(original_path);
}

}
