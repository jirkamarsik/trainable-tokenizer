#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <utility>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/unordered_map.hpp>
#include <boost/ref.hpp>
#include <boost/thread.hpp>
#include "tbb/pipeline.h"
#include "tbb/concurrent_queue.h"
#include <ltdl.h>
#include <pcrecpp.h>
#include <maxentmodel.hpp>

#include "pipes/pipe.hpp"

#include "configuration.hpp"
#include "config_exception.hpp"
#include "TextCleaner.hpp"
#include "cutout_t.hpp"
#include "roughtok_compile.hpp"
#include "RoughTokenizer.hpp"
#include "read_features_file.hpp"
#include "FeatureExtractor.hpp"
#include "Classifier.hpp"
#include "SimplePreparer.hpp"
#include "OutputFormatter.hpp"
#include "Encoder.hpp"

using namespace std;
using namespace trtok;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

#define END_WITH_ERROR(location, message) {\
  cerr << location << ": Error: " << message << endl;\
  return 1;\
}

// A 2D-array accessor for the feature selection mask
#define FEATURES_MASK(offset, property)\
  features_mask[(offset) * n_properties + (property)]


/* A comparison function used later.
 * Compares paths first by filename then by path length */
bool path_compare(fs::path const &a, fs::path const &b) {
    if (a.filename() < b.filename()) {
        return true;
    } else if (a.filename() > b.filename()) {
        return false;
    } else {
        if (a.string().length() > b.string().length()) {
            return true;
        } else {
            return false;
        }
    }
}

void include_listed_files(fs::path const &file_list_path,
                          vector<string> &input_files) {
    istream *file_list_stream_p = (file_list_path == "-") ? &std::cin
                                      : new fs::ifstream(file_list_path);
    fs::path file_list_dir = file_list_path.parent_path();
    string line;
    while (getline(*file_list_stream_p, line)) {
      if (line.size() > 0)
        // the listed files are relative to the directory of the file list
        // if not already absolute
        input_files.push_back(fs::absolute(line, file_list_dir).native());
    }
    if (file_list_path != "-") {
      fs::ifstream *file_list_file_stream_p = (fs::ifstream*)file_list_stream_p;
      file_list_file_stream_p->close();
      delete file_list_file_stream_p;
    }
}


int main(int argc, char const **argv) {

    // PARSING AND CHECKING THE ARGUMENTS

    /* We declare the following variables which will catch the values
     * of options coming from the command line. The s_* variables represent
     * settings, o_* represent options and sv_* represent nonsingleton
     * settings as a vector of strings. */
    string s_mode, s_scheme;
    string s_encoding;
    string s_qa_file;

    vector<string> sv_input_files;
    vector<string> sv_file_lists, sv_heldout_file_lists;
    string s_filename_regexp;

    bool o_detokenize, o_honour_single_newline, o_honour_more_newlines,
         o_never_add_newline;
    bool o_remove_xml, o_remove_xml_perm;
    bool o_expand_entities, o_expand_entities_perm;
    bool o_verbose;

    /* We use the Boost Program Options library to handle option parsing.
     * We first start off by enumerating the names, types and descriptions
     * of our options. */
    po::options_description explicit_options;
    explicit_options.add_options()
      ("encoding,c", po::value<string>(&s_encoding)->default_value("UTF-8"),
        "Specifies the input and output encoding of the tokenizer. UTF-8 is "
        "used if none is specified.")
      ("file-list,l", po::value< vector<string> >(&sv_file_lists)->composing(),
        "A list of input files to be processed. If the paths are relative, "
        "they are evaluated with respect to the location of the file list. "
        "More than 1 file list can be specified.")
      ("heldout-file-list,h",
          po::value< vector<string> >(&sv_heldout_file_lists)
                           ->composing(),
        "A list of input files to serve as heldout data during training. "
        "Applicable only in 'train' mode.")
      ("filename-regexp,r",
          po::value<string>(&s_filename_regexp)
                           ->default_value("/\\.txt/.tok/"),
        "A regular expression/replacement string used to find partner files "
        "for the input files. These are the output files when in 'prepare' "
        "or 'tokenize' modes and annotated files when in 'train' or "
        "'evaluate' modes.")
      ("detokenize,d", po::bool_switch(&o_detokenize),
        "Preserve the tokenization (whitespace delimited) of the input.")
      ("honour-single-newline,s", po::bool_switch(&o_honour_single_newline),
       "All newlines in the input are interpreted as sentence boundaries.")
      ("honour-more-newlines,m", po::bool_switch(&o_honour_more_newlines),
       "Spans of whitespace containing more than 1 newline will be preserved.")
      ("never-add-newline,n", po::bool_switch(&o_never_add_newline),
       "Prevent trtok from segmenting the text any further.")
      ("remove-xml,x", po::bool_switch(&o_remove_xml),
        "Removes XML markup from the input for the duration of the "
        "tokenization. If -X (--remove-xml-perm) is not set, the XML is "
        "reinserted into the output.")
      ("remove-xml-perm,X", po::bool_switch(&o_remove_xml_perm),
        "Removes XML markup from the input without reinserting it into the "
        "output after tokenization.")
      ("expand-entities,e", po::bool_switch(&o_expand_entities),
        "Expands HTML entities and character references found in the input for "
        "the duration of the tokenization. If -E (--expand-entities-perm) is "
        "not set, the characters produced by entity expansion will be replaced "
        "by the original entities in the output.")
      ("expand-entities-perm,E", po::bool_switch(&o_expand_entities_perm),
        "Expands entities found in the input and keeps the characters produced "
        "by the expansion in the output in their literal form.")
      ("questions,q", po::value<string>(&s_qa_file)->default_value("-"),
        "Prints the contexts and outcomes from the maximum entropy classifier "
        "to the specified file. In 'tokenize' mode, the classifier's answer is "
        "present; in 'train' mode, it is the answer induced from the data. In "
        "'evaluate' mode, both the answer given by the classifier and the "
        "correct answer are output. If no file is given in 'evaluate' mode, "
        "the questions are output to the standard output instead.")
      ("verbose,v", po::bool_switch(&o_verbose),
        "If set, the Maxent toolkit will output its reports.")
    ;

    /* Positional arguments such as the mode and scheme need to be described
     * as well. They are defined in a different structure as we don't want
     * to list them as options when printing the usage.*/
    po::options_description positional_options;
    positional_options.add_options()
        ("mode", po::value<string>(&s_mode)->required(),"")
        ("scheme", po::value<string>(&s_scheme)->required(), "")
        ("input-file", po::value< vector<string> >(&sv_input_files)
                       ->composing(), "")
        ;

    // We inform the library of the order of these two positional arguments.
    po::positional_options_description pod;
    pod.add("mode", 1).add("scheme", 1).add("input-file", -1);

    po::options_description all_options;
    all_options.add(explicit_options).add(positional_options);
    po::command_line_parser cmd_line(argc, argv);
    cmd_line.options(all_options).positional(pod);

    po::variables_map vm;

    try {
        po::store(cmd_line.run(), vm);
        po::notify(vm);

        // -X implies -x and -E implies -e
        if (o_remove_xml_perm)
          o_remove_xml = true;
        if (o_expand_entities_perm)
          o_expand_entities = true;
    } catch (po::error const &exc) {
        cerr << "trtok:command line options: Error: " << exc.what() << endl;
        cerr << "Usage: trtok <prepare|train|tokenize|evaluate> "
                "SCHEME [OPTION]... [FILE]..." << endl;
        cerr << explicit_options;
        return 1;
    }


    classifier_mode_t mode;
    if (s_mode == "prepare") {
      mode = PREPARE_MODE;
    } else if (s_mode == "train") {
      mode = TRAIN_MODE;
    } else if (s_mode == "tokenize") {
      mode = TOKENIZE_MODE;
    } else if (s_mode == "evaluate") {
      mode = EVALUATE_MODE;
    } else {
      END_WITH_ERROR("trtok", "Mode " << s_mode << " not recognized. Supported "
          "modes include prepare, train, tokenize and evaluate. See trtok "
          "--help for more.");
    }

    maxent::verbose = o_verbose ? 1 : 0;

    // We need the path to the TrTok file structure which is stored in the
    // environment variable TRTOK_PATH
    char *e_trtok_path = getenv("TRTOK_PATH");
    if (e_trtok_path == NULL) {
        END_WITH_ERROR("trtok", "Environment variable TRTOK_PATH is not set. "
            "Please set it to the installation directory of trtok.");
    }

    // The tokenization schemes are configured in the schemes subdirectory.
    fs::path schemes_root = fs::path(e_trtok_path) / fs::path("schemes");
    if (!fs::is_directory(schemes_root)) {
        END_WITH_ERROR("trtok", "$TRTOK_PATH/schemes is not a directory. "
            "Make sure the environment variable TRTOK_PATH is set to the "
            "installation directory of trtok and that the tokenization "
            "schemes are stored in its subdirectory 'schemes'.");
    }

    fs::path scheme_rel_path = fs::path(s_scheme);
    fs::path scheme_path = schemes_root / scheme_rel_path;
    if (!fs::is_directory(scheme_path)) {
        END_WITH_ERROR("trtok", "Scheme directory \"" << s_scheme << "\" not "
            "found in the schemes directory.");
    }

    // We disallow ".." because we want the path to be a path-separator-
    // delimited list of parent schemes.
    if (find(scheme_rel_path.begin(), scheme_rel_path.end(), "..")
        != scheme_rel_path.end()) {
      END_WITH_ERROR("trtok", "The double-dot isn't allowed in scheme paths.");
    }


    // READING THE CONFIG FILES
    //
    // Variables still relevant: 
    //    schemes_root / scheme_rel_path == scheme_path 
    //    o_*, s_* options and settings
    //    e_* environment variables


    /* We will iterate over the elements of the relative path to the selected
     * scheme. By appending them in order to the scheme directory, we get
     * the paths to all the parent schemes of the selected schemes. We use
     * this to extract all filepaths within the "scheme lineage".*/
    scheme_path = schemes_root;
    vector<fs::path> relevant_files;
    
    // We also iterate over the contents of the 'schemes' directory letting
    // users place universal definitions inside them (good for building
    // a vocabulary of universal properties to use in the features file).
    for (fs::directory_iterator file(scheme_path);
         file != fs::directory_iterator(); file++) {
      fs::path file_path = file->path();
      if (!fs::is_directory(file_path)) {
        relevant_files.push_back(file_path);
      }
    }
    for (fs::path::const_iterator ancestor = scheme_rel_path.begin();
         ancestor != scheme_rel_path.end(); ancestor++) {
      scheme_path /= *ancestor;

      for (fs::directory_iterator file(scheme_path);
           file != fs::directory_iterator(); file++) {
        fs::path file_path = file->path();
        if (!fs::is_directory(file_path)) {
          relevant_files.push_back(file_path);
        }
      }
    }

    /* We sort the files so that all instances of a filename are clustered
     * together beginning with the most specific one. */
    sort(relevant_files.begin(), relevant_files.end(), path_compare);

    /* We then split the files into bins according to their extension and
     * look for special filenames (train.fnre etc...).
     * If case folding is implented in the future, it might be feasible to
     * make the extensions completely case-insensitive. */
    vector<fs::path> split_files, join_files, break_files,
                     listp_files, rep_files;
    fs::path features_file, maxentparams_file;
    fs::path default_file_list, default_heldout_file_list, default_fnre_file;
    boost::unordered_map< string, vector<fs::path>* > file_vectors;
    file_vectors[".split"] = &split_files;
    file_vectors[".join"] = &join_files;
    file_vectors[".break"] = &break_files;
    file_vectors[".listp"] = &listp_files;
    file_vectors[".rep"] = &rep_files;

    /* When we encounter two paths with the same filename, we take the former.
     * The following "duplicates" are guaranteed to be less specific, because
     * we sort by path length in a descending order. */
    string last_filename = "";
    for (vector<fs::path>::const_iterator file = relevant_files.begin();
         file != relevant_files.end(); file++) {
      // Scheme inheritance: only the most specific instance of a file
      if (file->filename() == last_filename) {
          continue;
      }

      boost::unordered_map< string, vector<fs::path>* >::iterator
        lookup = file_vectors.find(file->extension().string());
      if (lookup != file_vectors.end()) {
          lookup->second->push_back(*file);
      } else if ((file->extension() == ".fl") && (file->stem() == s_mode)) {
          default_file_list = *file;
      } else if ((file->extension() == ".fl") && (file->stem() == "heldout")) {
          default_heldout_file_list = *file;
      } else if ((file->extension() == ".fnre") && (file->stem() == s_mode)) {
          default_fnre_file = *file;
      } else if (file->filename() == "features") {
          features_file = *file;
      } else if (file->filename() == "maxent.params")  {
          maxentparams_file = *file;
      }
      last_filename = file->filename().string();
    }

    // All files generated for this scheme will be stored in the build
    // directory under the same relatve path as the scheme definition.
    fs::path build_path =
        fs::path(e_trtok_path) / fs::path("build") / scheme_rel_path;
    fs::create_directories(build_path);


    // COMPILING AND LOADING THE ROUGH TOKENIZER
    try {
      compile_rough_lexer(split_files, join_files, break_files,
                          build_path);
    } catch (config_exception const &exc) {
      cerr << exc.what() << endl;
      return 1;
    }
    
    int return_code = lt_dlinit();
    if (return_code != 0) {
      END_WITH_ERROR("ltdl", "lt_dlinit returned " << return_code + ".");
    }
    
    fs::path roughtok_wrapper_path = build_path / fs::path("roughtok");
    lt_dlhandle libroughtok = lt_dlopen(roughtok_wrapper_path.c_str());
    if (libroughtok == NULL) {
      END_WITH_ERROR(roughtok_wrapper_path.native(), "Cannot load the "
          "compiled rough tokenizer. Oh no! Something bad must have happened "
          "in the build system... Why? : " << lt_dlerror());
    }

    void *factory_func_void_p = lt_dlsym(libroughtok, "make_quex_wrapper");
    if (factory_func_void_p == NULL) {
      END_WITH_ERROR(roughtok_wrapper_path.native(), "No extern "
          "make_quex_wrapper in the wrapper! Why? : " << lt_dlerror());
    }
    typedef IRoughLexerWrapper* (*factory_func_t)(void);
    factory_func_t factory_func_p = (factory_func_t)factory_func_void_p;
    IRoughLexerWrapper *rough_lexer_wrapper = factory_func_p();


    // READING AND PARSING THE PROPERTY DEFINITIONS
    
    // A mapping from property names to indices.
    boost::unordered_map<string, int> prop_name_to_id;
    // and a mapping from property indices to names
    vector<string> prop_id_to_name;

    // The properties are comprised of the basic user-defined properties,
    // which are defined in .rep files using regular expressions or in .listp
    // files using lists of rough tokens, and of builtin properties
    // %Word and %length.
    int n_properties = 0;
    int n_basic_properties = 0;

    // We compile the regex properties with PCRE
    vector<pcrecpp::RE> regex_properties;
    for (vector<fs::path>::const_iterator file = rep_files.begin();
         file != rep_files.end(); file++) {
      // Read, ...
      string regex_string("");
      fs::ifstream regex_file(*file);
      string line;
      while (getline(regex_file, line)) {
        if ((line.length() == 0) || (line[0] == '#'))
          continue;
        else if (regex_string == "")
          regex_string = line;
        else {
          END_WITH_ERROR(file->native(), "More than 1 non-blank non-comment "
              "line in regex property definition file.");
          }
      }
      regex_file.close();

      // compile ...
      pcrecpp::RE regex(regex_string, pcrecpp::UTF8());
      if (regex.error() != "") {
        END_WITH_ERROR(file->native(), "The following error occured when "
            "compiling the regular expression: " << regex.error());
      }
      regex_properties.push_back(regex);

      // and register.
      if (prop_name_to_id.count(file->stem().string()) > 0) {
        END_WITH_ERROR(*file, "Property \"" << file->stem() << "\" defined "
            "twice (both as a regex property and a list property).");
      }
      prop_name_to_id[file->stem().string()] = n_properties;
      prop_id_to_name.push_back(file->stem().string());
      n_properties++;
      n_basic_properties++;
    }

    // The words belonging to list properties are inserted into
    // a BST along with the ids of the list properties they belong to.
    multimap<string, int> word_to_list_props;
    for (vector<fs::path>::const_iterator file = listp_files.begin();
         file != listp_files.end(); file++) {
      // Read and store words...
      fs::ifstream list_file(*file);
      string line;
      while (getline(list_file, line)) {
        if (line.length() == 0)
          continue;
        word_to_list_props.insert(make_pair(line, n_properties));
      }
      list_file.close();

      // and register the property.
      if (prop_name_to_id.count(file->stem().string()) > 0) {
        END_WITH_ERROR(*file, "Property \"" << file->stem() << "\" defined "
            "twice (both as a regex property and a list property).");
      }
      prop_name_to_id[file->stem().string()] = n_properties;
      prop_id_to_name.push_back(file->stem().string());
      n_properties++;
      n_basic_properties++;
    }

    // Finally we add the two "builtin" properties.
    if (prop_name_to_id.count("%length") > 0) {
      END_WITH_ERROR("*/%length.[rep|listp]",
          "'%length' is a reserved property name.");
    }
    prop_name_to_id["%length"] = n_properties;
    prop_id_to_name.push_back("%length");
    n_properties++;

    if (prop_name_to_id.count("%Word") > 0) {
      END_WITH_ERROR("*/%Word.[rep|listp]",
          "'%Word' is a reserved property name.");
    }
    prop_name_to_id["%Word"] = n_properties;
    prop_id_to_name.push_back("%Word");
    n_properties++;


    // PARSING FEATURES FILE
    
    if (features_file.empty())
      END_WITH_ERROR("trtok", "No features file found.");

    // Features mask is a 2D array of bools which tells for each offset
    // in the context window in which features we are interested
    bool *features_mask;
    // Combined features are lists of offset-property pairs which are
    // all queried for and joined into a single compound maxent feature.
    vector< vector< pair<int,int> > > combined_features;
    // The precontext and postcontext variables describe the size of the
    // context window in both directions from the examined token.
    int precontext, postcontext;

    int read_features_exit_code =
        read_features_file(features_file.native(), prop_name_to_id,
                           n_properties, n_basic_properties, features_mask,
                           combined_features, precontext, postcontext);
    if (read_features_exit_code != 0) {
      return read_features_exit_code;
    }

    // DETERMINING THE INPUT FILES

    vector<string> input_files;

    // The files explicitly stated on the command line are to be processed
    // everytime. If a file named '-' was given, the standard input/output
    // combo is used.
    for (vector<string>::const_iterator file = sv_input_files.begin();
         file != sv_input_files.end(); file++) {

      fs::path file_path(*file);

      if (!fs::exists(file_path) && (file_path != "-")) {
        END_WITH_ERROR(*file, "File not found.");
      }

      input_files.push_back(*file);
    }

    // If we were given any explicit file list, we include all the files
    // referred inside it.
    for (vector<string>::const_iterator file_list = sv_file_lists.begin();
         file_list != sv_file_lists.end(); file_list++) {

      fs::path file_list_path(*file_list);

      if (!fs::exists(file_list_path) && (file_list_path != "-")) {
        END_WITH_ERROR(*file_list, "File not found.");
      }

      include_listed_files(file_list_path, input_files);
    }

    // If no files or file lists were given explicitly, we check for
    // a .fl file with a default file list.
    if ((input_files.size() == 0) && !default_file_list.empty()) {
      include_listed_files(default_file_list, input_files);
    }

    // If no input files or file lists were given or set up as default
    // by the user, then we process standard input.
    if (input_files.size() == 0) {
      input_files.push_back("-");
    }


    // This marks what part of the input_files is composed of regular
    // input files (i.e. not-heldout files).
    int num_nonheldout_files = input_files.size();

    // If we are in 'train' mode, we also add the heldout files after
    // the regular input files.
    if (mode == TRAIN_MODE) {

      // We first check for any explicit heldout file lists...
      for (vector<string>::const_iterator file_list = sv_heldout_file_lists.begin();
           file_list != sv_heldout_file_lists.end(); file_list++) {

        fs::path file_list_path(*file_list);

        if (!fs::exists(file_list_path) && (file_list_path != "-")) {
          END_WITH_ERROR(*file_list, "File not found.");
        }

        include_listed_files(file_list_path, input_files);
      }

      // ... and if there were none, we add the default heldout.fl.
      if ((input_files.size() == num_nonheldout_files)
          && !default_heldout_file_list.empty()) {
        include_listed_files(default_heldout_file_list, input_files);
      }
    }



    // PARSING THE FILENAME REGEXP/REPLACEMENT STRING
    
    // If no filename-regexp was given, we check for the presence of a
    // .fnre file and use its contents instead of the default.
    if (vm["filename-regexp"].defaulted() && !default_fnre_file.empty()) {

      fs::ifstream fnre_file(default_fnre_file);

      string line;
      string retrieved = "";
      while (getline(fnre_file, line)) {
        if (line.size() > 0) {
          if (retrieved == "") {
            retrieved = line;
          } else {
            END_WITH_ERROR(default_fnre_file, "More than 1 nonempty line in "
            "file.");
          }
        }
      }
      if (retrieved == "") {
        END_WITH_ERROR(default_fnre_file, "File is empty.");
      }
      s_filename_regexp = retrieved;

      fnre_file.close();
    }

    // Decomposition of the regexp/replacement string combo.
    char delimiter = s_filename_regexp[0];

    size_t second_delimiter_pos = s_filename_regexp.find(delimiter, 1);
    if (second_delimiter_pos == string::npos) {
      END_WITH_ERROR(
          (vm["filename-regexp"].defaulted() ? default_fnre_file : "trtok"),
          "Delimiter in the filename regexp/replacement string found only at "
          "the start. Make sure the first character of the regex/replacement "
          "string is used to terminate the regex and the replacement string "
          "(as in sed, e.g. /change_this/into_this/).");
    }

    size_t third_delimiter_pos =
            s_filename_regexp.find(delimiter, second_delimiter_pos + 1);
    if (third_delimiter_pos != s_filename_regexp.size() - 1) {
      END_WITH_ERROR(
          (vm["filename-regexp"].defaulted() ? default_fnre_file : "trtok"),
          "Delimiter in the filename regexp/replacement string not found at "
          "the end. Make sure the first character of the regex/replacement "
          "string is used to terminate the regex and the replacement string "
          "(as in sed, e.g. /change_this/into_this/).");
    }

    // The product: fnre_regexp, fnre_replace
    pcrecpp::RE fnre_regexp(
                    s_filename_regexp.substr(1, second_delimiter_pos - 1),
                    pcrecpp::UTF8());
    string fnre_replace = s_filename_regexp.substr(second_delimiter_pos + 1,
                          third_delimiter_pos - (second_delimiter_pos + 1));

    if (fnre_regexp.error() != "") {
      END_WITH_ERROR(
          (vm["filename-regexp"].defaulted() ? default_fnre_file : "trtok"),
          "The following error occured when compiling the regular expression: "
          << fnre_regexp.error());
    }
    

    // PARSING OTHER TIDBITS

    // This is the file in which the trained maxent model for this tokenization
    // scheme is stored.
    fs::path model_path = build_path / "maxent.model";
    if (((mode == TOKENIZE_MODE) || (mode == EVALUATE_MODE))
        && !fs::exists(model_path)) {
      END_WITH_ERROR(model_path, "Maxent model not found. Please train "
          "the maxent model before using it for tokenization.");
    }

    // If a questions/answers file was requested, we create a stream to one.
    // If we are in the EVALUATE_MODE, we always want to output questions
    // and answers.
    ostream *qa_stream_p = NULL;
    if (!vm["questions"].defaulted() || (mode == EVALUATE_MODE)) {
      if (s_qa_file == "-") {
        qa_stream_p = &cout;
      } else {
        qa_stream_p = new ofstream(s_qa_file.c_str());
      }
    }


    // PARSING THE TRAINING PARAMETERS

    training_parameters_t training_parameters;
    bool save_model_as_binary = false;

    if ((mode == TRAIN_MODE) && !maxentparams_file.empty()) {

      fs::ifstream maxentparams_stream(maxentparams_file);

      po::options_description training_options;
      training_options.add_options()
          ("event_cutoff",
              po::value<size_t>(&training_parameters.event_cutoff))
          ("n_iterations",
              po::value<size_t>(&training_parameters.n_iterations))
          ("method_name",
              po::value<string>(&training_parameters.method_name))
          ("smoothing_coefficient",
              po::value<double>(&training_parameters.smoothing_coefficient))
          ("convergence_tolerance",
              po::value<double>(&training_parameters.convergence_tolerance))
          ("save_as_binary",
              po::value<bool>(&save_model_as_binary))
          ;

      po::variables_map maxent_vm;
      try {
        po::store(po::parse_config_file(maxentparams_stream, training_options),
                  maxent_vm);
        po::notify(maxent_vm);
      } catch (po::error const &exc) {
          END_WITH_ERROR(maxentparams_file, exc.what() << endl <<
              "See the documentation for a proper way to set maxent training "
              "parameters.");
      }

      maxentparams_stream.close();
    }


    // CONSTRUCTING THE PIPELINE

    tbb::pipeline pipeline;

    tbb::concurrent_bounded_queue<cutout_t> *cutout_queue_p = NULL;

    TextCleaner *input_cleaner_p = NULL;
    pipes::pipe *input_pipe_p = NULL;
    pipes::opipestream *input_pipe_to_p = NULL;
    pipes::ipipestream *input_pipe_from_p = NULL;

    TextCleaner *annot_cleaner_p = NULL;
    pipes::pipe *annot_pipe_p = NULL;
    pipes::opipestream *annot_pipe_to_p = NULL;
    pipes::ipipestream *annot_pipe_from_p = NULL;

    RoughTokenizer *rough_tokenizer_p = NULL;
    FeatureExtractor *feature_extractor_p = NULL;
    Classifier *classifier_p = NULL;
    SimplePreparer *simple_preparer_p = NULL;
    OutputFormatter *output_formatter_p = NULL;

    Encoder *encoder_p = NULL;
    pipes::pipe *output_pipe_p = NULL;
    pipes::opipestream *output_pipe_to_p = NULL;
    pipes::ipipestream *output_pipe_from_p = NULL;

    if ((mode == TRAIN_MODE) || (mode == EVALUATE_MODE)) {
      
      input_pipe_p = new pipes::pipe(pipes::pipe::limited_capacity);
      input_pipe_to_p = new pipes::opipestream(*input_pipe_p);
      input_pipe_from_p = new pipes::ipipestream(*input_pipe_p);

      input_cleaner_p = new TextCleaner(input_pipe_to_p, s_encoding,
                                        o_remove_xml, o_remove_xml_perm,
                                        o_expand_entities,
                                        o_expand_entities_perm);

      rough_tokenizer_p = new RoughTokenizer(rough_lexer_wrapper);
      rough_tokenizer_p->setup(input_pipe_from_p, "UTF-8");
      pipeline.add_filter(*rough_tokenizer_p);

      annot_pipe_p = new pipes::pipe(pipes::pipe::limited_capacity);
      annot_pipe_to_p = new pipes::opipestream(*annot_pipe_p);
      annot_pipe_from_p = new pipes::ipipestream(*annot_pipe_p);

      annot_cleaner_p = new TextCleaner(annot_pipe_to_p, s_encoding,
                                        o_remove_xml, o_remove_xml_perm,
                                        o_expand_entities,
                                        o_expand_entities_perm);

      feature_extractor_p = new FeatureExtractor(n_basic_properties,
                                                 regex_properties,
                                                 word_to_list_props);
      pipeline.add_filter(*feature_extractor_p);

      classifier_p = new Classifier(mode, prop_id_to_name, precontext,
                                    postcontext, features_mask,
                                    combined_features, qa_stream_p,
                                    annot_pipe_from_p);
      if (mode == EVALUATE_MODE) {
        classifier_p->load_model(model_path.native());
      }
      pipeline.add_filter(*classifier_p);

    } else if ((mode == PREPARE_MODE) || (mode == TOKENIZE_MODE)) {

      cutout_queue_p = new tbb::concurrent_bounded_queue<cutout_t>;

      input_pipe_p = new pipes::pipe(pipes::pipe::limited_capacity);
      input_pipe_to_p = new pipes::opipestream(*input_pipe_p);
      input_pipe_from_p = new pipes::ipipestream(*input_pipe_p);

      input_cleaner_p = new TextCleaner(input_pipe_to_p, s_encoding,
                                        o_remove_xml, o_remove_xml_perm,
                                        o_expand_entities,
                                        o_expand_entities_perm,
                                        cutout_queue_p);

      rough_tokenizer_p = new RoughTokenizer(rough_lexer_wrapper);
      rough_tokenizer_p->setup(input_pipe_from_p, "UTF-8");
      pipeline.add_filter(*rough_tokenizer_p);

      // If we only want to cut up raw text so it is easier to annotate
      // and we are not interested in any features, we can cut out a lot
      // of work by replacing the FeatureExtractor and Classifier with a
      // SimplePreparer
      if ((mode == PREPARE_MODE) && (qa_stream_p == NULL)) {
        simple_preparer_p = new SimplePreparer();
        pipeline.add_filter(*simple_preparer_p);
      } else {
        feature_extractor_p = new FeatureExtractor(n_basic_properties,
                                                   regex_properties,
                                                   word_to_list_props);
        pipeline.add_filter(*feature_extractor_p);

        classifier_p = new Classifier(mode, prop_id_to_name, precontext,
                                      postcontext, features_mask,
                                      combined_features, qa_stream_p);
        classifier_p->load_model(model_path.native());
        pipeline.add_filter(*classifier_p);
      } //if ((mode == PREPARE_MODE) && (qa_stream_p == NULL))

      output_pipe_p = new pipes::pipe(pipes::pipe::limited_capacity);
      output_pipe_to_p = new pipes::opipestream(*output_pipe_p);
      output_pipe_from_p = new pipes::ipipestream(*output_pipe_p);
      
      output_formatter_p = new OutputFormatter(output_pipe_to_p, o_detokenize,
                                               o_honour_single_newline,
                                               o_honour_more_newlines,
                                               o_never_add_newline,
                                               cutout_queue_p);
      pipeline.add_filter(*output_formatter_p);

      encoder_p = new Encoder(output_pipe_from_p, s_encoding);

    } // if ((mode == PREPARE_MODE) || (mode == TOKENIZE_MODE))
    
    
    // RUNNING THE PIPELINE

    for (vector<string>::const_iterator input_file = input_files.begin();
         input_file != input_files.end(); input_file++) {
      
      clog << "trtok: Processing file " << *input_file << endl;

      try {
        // If we have processed all the regular input files and there are more
        // files to process, it must be the heldout data and so we notify the
        // Classifier.
        if (input_file - input_files.begin() == num_nonheldout_files) {
          classifier_p->switch_to_heldout_data();
        }

        fs::path input_file_path(*input_file);
        string other_file(*input_file);

        if (*input_file != "-") {
          // We are working with files
          if (!fs::exists(input_file_path)) {
            cerr << *input_file << ": Warning: File not found, skipping."
                 << endl;
            continue;
          }
          
          bool fnre_success = fnre_regexp.Replace(fnre_replace, &other_file);

          if (!fnre_success) {
            cerr << *input_file << ": Warning: Failed to apply regex to find "
                "partner file, skipping. Possible causes include the regular "
                "expression failing to match and the replacement string using "
                "illegal backreferences." << endl;
            continue;
          }
        }
        
        if ((mode == TRAIN_MODE) || (mode == EVALUATE_MODE)) {
          
          if (*input_file == "-") {
            END_WITH_ERROR("trtok", s_mode << " mode cannot act on "
                "the standard input alone.");
          }

          // Check for the annotated file,...
          fs::path annotated_file_path(other_file);
          if (!fs::exists(annotated_file_path)) {
            cerr << other_file << ": Warning: Annotated file does not exist, "
                "skipping." << endl;
            continue;
          }

          // open the files,...
          fs::ifstream input_stream(input_file_path);
          fs::ifstream annotated_stream(annotated_file_path);

          // restore the pipes,...
          /* The sender closes his pipestream to signal an EOF to the receiver.
             These pipestreams need to reopened for subsequent iterations.
             All pipestreams must however disconnect from the pipe before
             we can use it again. */
          if (!input_pipe_to_p->is_open()) {
            input_pipe_from_p->close();
            input_pipe_to_p->open(*input_pipe_p);
            input_pipe_from_p->open(*input_pipe_p);
          }
          if (!annot_pipe_to_p->is_open()) {
            annot_pipe_from_p->close();
            annot_pipe_to_p->open(*annot_pipe_p);
            annot_pipe_from_p->open(*annot_pipe_p);
          }

          // clean out and setup the pipeline,...
          input_cleaner_p->setup(&input_stream);
          rough_tokenizer_p->reset();
          annot_cleaner_p->setup(&annotated_stream);
          classifier_p->setup(input_file_path.native(),
                              annotated_file_path.native());

          // run it...
          boost::thread input_thread(&TextCleaner::do_work,
                                     boost::ref(*input_cleaner_p));
          boost::thread annot_thread(&TextCleaner::do_work,
                                     boost::ref(*annot_cleaner_p));
          pipeline.run(WORK_UNIT_COUNT);
          input_thread.join();
          annot_thread.join();
        
          // and close the files.
          input_stream.close();
          annotated_stream.close();

        } else if ((mode == PREPARE_MODE) || (mode == TOKENIZE_MODE)) {

          // Open the files,...
          fs::path output_file_path(other_file);
          if (!fs::is_directory(output_file_path.parent_path())) {
            fs::create_directories(output_file_path.parent_path());
          }

          istream *input_stream_p = (*input_file == "-") ? &cin
                                         : new fs::ifstream(input_file_path);
          ostream *output_stream_p = (*input_file == "-") ? &cout
                                          : new fs::ofstream(output_file_path);

          // restore the pipes,...
          if (!input_pipe_to_p->is_open()) {
            input_pipe_from_p->close();
            input_pipe_to_p->open(*input_pipe_p);
            input_pipe_from_p->open(*input_pipe_p);
          }
          if (!output_pipe_to_p->is_open()) {
            output_pipe_from_p->close();
            output_pipe_to_p->open(*output_pipe_p);
            output_pipe_from_p->open(*output_pipe_p);
          }

          // setup the pipeline,...
          input_cleaner_p->setup(input_stream_p);
          rough_tokenizer_p->reset();
          if (simple_preparer_p != NULL) {
            simple_preparer_p->reset();
          } else {
            feature_extractor_p->reset();
            classifier_p->setup(input_file_path.native());
          }
          output_formatter_p->reset();
          encoder_p->setup(output_stream_p);

          // run it...
          boost::thread input_thread(&TextCleaner::do_work,
                                     boost::ref(*input_cleaner_p));
          boost::thread output_thread(&Encoder::do_work,
                                      boost::ref(*encoder_p));
          pipeline.run(WORK_UNIT_COUNT);
          input_thread.join();
          output_thread.join();
          
          output_stream_p->flush();
          // and close the files.
          if (*input_file != "-") {
            fs::ifstream *input_file_stream_p = (fs::ifstream*)input_stream_p;
            fs::ofstream *output_file_stream_p = (fs::ofstream*)output_stream_p;
            input_file_stream_p->close();
            output_file_stream_p->close();
            delete input_file_stream_p;
            delete output_file_stream_p;
          }
        }
      } catch (exception) {
        cerr << *input_file << ": Error: An exception was thrown during "
          "the processing of the file.";
      }
    }

    // If our mission was to train a maxent model, then by now the Classifier
    // has accumulated all the required questions and answers, so we can hand
    // it over to the Maxent toolkit to do the rest.
    if (mode == TRAIN_MODE) {
      classifier_p->train_model(training_parameters, model_path.native(),
                                save_model_as_binary);
    }

    if (qa_stream_p != NULL) {
      qa_stream_p->flush();
      if (s_qa_file != "-") {
        std::ofstream *qa_file_stream_p = (std::ofstream*)qa_stream_p;
        qa_file_stream_p->close();
      }
    }

    lt_dlexit();
}
