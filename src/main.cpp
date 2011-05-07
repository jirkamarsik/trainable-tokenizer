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
#include <pcre.h>
#include <pcrecpp.h>
#include <maxentmodel.hpp>

#include "pipes/pipe.hpp"

#include "configuration.hpp"
#include "config_exception.hpp"
#include "TextCleaner.hpp"
#include "cutout_t.hpp"
#include "rough_tok_compile.hpp"
#include "RoughTokenizer.hpp"
#include "read_features_file.hpp"
#include "FeatureExtractor.hpp"
#include "Classifier.hpp"
#include "OutputFormatter.hpp"
#include "Encoder.hpp"

using namespace std;
using namespace trtok;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

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

#define END_WITH_ERROR(location, message) {\
  std::cerr << location << ": Error: " << message << std::endl;\
  return 1;\
}


// A 2D-array accessor for the feature selection mask
#define FEATURES_MASK(offset, property)\
  features_mask[(offset) * n_properties + property]


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
    vector<string> sv_file_lists;
    string s_filename_regexp;

    bool o_help;
    bool o_preserve_paragraphs, o_detokenize, o_preserve_segments;
    bool o_hide_xml, o_expand_entities, o_keep_entities_expanded;
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
        "A list of input files to be processed. If mode is TRAIN and this "
        "setting is omitted, the file train.fl in scheme-path is used "
        "instead. For the EVALUATE mode, the file evaluate.fl is used as "
        "default. If the paths are relative, they are evaluated with respect "
        "to the location of the file list. More than 1 file list can be "
        "specified.")
      ("filename-regexp,r",
          po::value<string>(&s_filename_regexp)
                           ->default_value("(.*)\\.txt/\1.tok"),
        "A regular expression/replacement string used to generate a set of "
        "pairs of input/output files. Output files are written to in TOKENIZE "
        "mode and are used as correct answers in TRAIN and EVALUATE modes. If "
        "no such expression is given in TRAIN and EVALUATE modes, the "
        "contents of files train.fnre and evaluate.fnre in the directory "
        "scheme-path are used instead. If the mode is TOKENIZE, the output of "
        "tokenization is printed to the standard output.")
      ("help,h", po::bool_switch(&o_help),
        "Prints this usage information.")
      ("preserve-paragraphs,p", po::bool_switch(&o_preserve_paragraphs),
        "Replaces paragraph breaks with a blank line.")
      ("detokenize,d", po::bool_switch(&o_detokenize),
        "Doesn't apply word splitting and joining decisions. The words found "
        "in the input are preserved, whitespace is condensed into single "
        "spaces or newlines in case of sentence break.")
      ("preserve-segments,s", po::bool_switch(&o_preserve_segments),
        "Assumes sentence boundaries are already specified by newlines.")
      ("hide-xml,x", po::bool_switch(&o_hide_xml),
        "Hides XML markup from the tokenizer and then reintroduces it to the "
        "output.")
      ("expand-entities,e", po::bool_switch(&o_expand_entities),
        "Treats entities as the characters they represent. The output will "
        "preserve these characters as entities.")
      ("keep-entities-expanded,k", po::bool_switch(&o_keep_entities_expanded),
        "Override the behavior of -e so expanded entities are kept expanded "
        "in the output instead of being replaced with the original entities.")
      ("questions,q", po::value<string>(&s_qa_file)->default_value("-"),
        "Prints the questions presented to the maximum entropy classifier to "
        "the specified file. In TOKENIZE mode, the classifier's answer is "
        "present as well; in TRAIN mode, it is the answer induced from the "
        "data. In EVALUATE mode, both the answer given by the classifier and "
        "the correct answer are output. If no file is given in EVALUATE mode, "
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
    } catch (po::unknown_option const &exc) {
        END_WITH_ERROR("trtok", "Encountered unknown option "
                                << exc.get_option_name() << ". "
                                "See trtok --help for proper usage.");
    } catch (po::required_option const &exc) {
        END_WITH_ERROR("trtok", "Missing mandatory parameter "
                                << exc.get_option_name() << ". "
                                "See trtok --help for proper usage.");
    } catch (po::too_many_positional_options_error const &exc) {
        END_WITH_ERROR("trtok", "Too many arguments. See 'trtok --help'.");
    }


    if (o_help) {
        cout << "Usage: trtok <prepare|train|tokenize|evaluate> "
                "SCHEME [OPTION]... [FILE]..." << endl;
        cout << explicit_options;
        return 0;
    }

    classifier_mode_t mode;
    if ((s_mode == "prepare") || (s_mode == "PREPARE")) {
      mode = PREPARE_MODE;
    } else if ((s_mode == "train") || (s_mode == "TRAIN")) {
      mode = TRAIN_MODE;
    } else if ((s_mode == "tokenize") || (s_mode == "TOKENIZE")) {
      mode = TOKENIZE_MODE;
    } else if ((s_mode == "evaluate") || (s_mode == "EVALUATE")) {
      mode = EVALUATE_MODE;
    } else {
      END_WITH_ERROR("trtok", "Mode " + s_mode + " not recognized. Supported "
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
        END_WITH_ERROR("trtok", "Scheme directory " << s_scheme << " not "
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
    vector<fs::path> split_files, join_files, begin_files, end_files,
                     enump_files, rep_files, fl_files, fnre_files;
    fs::path features_file, maxentparams_file;
    boost::unordered_map< string, vector<fs::path>* > file_vectors;
    file_vectors[".split"] = &split_files;
    file_vectors[".SPLIT"] = &split_files;
    file_vectors[".join"] = &join_files;
    file_vectors[".JOIN"] = &join_files;
    file_vectors[".begin"] = &begin_files;
    file_vectors[".BEGIN"] = &begin_files;
    file_vectors[".end"] = &end_files;
    file_vectors[".END"] = &end_files;
    file_vectors[".enump"] = &enump_files;
    file_vectors[".ENUMP"] = &enump_files;
    file_vectors[".rep"] = &rep_files;
    file_vectors[".REP"] = &rep_files;
    file_vectors[".fl"] = &fl_files;
    file_vectors[".FL"] = &fl_files;
    file_vectors[".fnre"] = &fnre_files;
    file_vectors[".FNRE"] = &fnre_files;

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
      }
      else if (((file->filename() == "features")
             || (file->filename() == "FEATURES"))
            && (features_file.empty())) {
          features_file = *file;
      }
      else if (((file->filename() == "maxent.params")
             || (file->filename() == "MAXENT.PARAMS"))
            && (maxentparams_file.empty())) {
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
      compile_rough_lexer(split_files, join_files, begin_files, end_files,
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
    // which are defined in .rep files using regular expressions or in .enump
    // files using enumerations of rough tokens, and of builtin properties
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

      // compile ...
      pcrecpp::RE regex(regex_string, PCRE_UTF8 | PCRE_UCP);
      if (regex.error() != "") {
        END_WITH_ERROR(file->native(), "The following error occured when "
            "compiling the regular expression: " << regex.error());
      }
      regex_properties.push_back(regex);

      // and register.
      prop_name_to_id[file->stem().string()] = n_properties;
      prop_id_to_name.push_back(file->stem().string());
      n_properties++;
      n_basic_properties++;
    }

    // The words beloning to enumerated properties are inserted into
    // a BST along with the ids of enumeation properties they belong to.
    std::multimap<string, int> word_to_enum_props;
    for (vector<fs::path>::const_iterator file = enump_files.begin();
         file != enump_files.end(); file++) {
      // Read and store words...
      fs::ifstream enum_file(*file);
      string line;
      while (getline(enum_file, line)) {
        if (line.length() == 0)
          continue;
        word_to_enum_props.insert(make_pair(line, n_properties));
      }

      // and register the property.
      prop_name_to_id[file->stem().string()] = n_properties;
      prop_id_to_name.push_back(file->stem().string());
      n_properties++;
      n_basic_properties++;
    }

    // Finally we add the two "builtin" properties.
    prop_name_to_id["%length"] = n_properties;
    prop_id_to_name.push_back("%length");
    n_properties++;

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

    
    // Testing code
    if (s_mode == "train") {
      pipes::pipe my_input_pipe(pipes::pipe::limited_capacity);
      pipes::opipestream my_input_pipe_to(my_input_pipe);
      pipes::ipipestream my_input_pipe_from(my_input_pipe);

      TextCleaner cleaner(&my_input_pipe_to, s_encoding, o_hide_xml,
                          o_expand_entities, o_keep_entities_expanded);
      cleaner.setup(&std::cin);

      RoughTokenizer rough_tok(rough_lexer_wrapper);
      rough_tok.setup(&my_input_pipe_from, "UTF-8");

      FeatureExtractor feature_extractor(n_basic_properties,
                                         regex_properties,
                                         word_to_enum_props);

      pipes::pipe my_annot_pipe(pipes::pipe::limited_capacity);
      pipes::opipestream my_annot_pipe_to(my_annot_pipe);
      pipes::ipipestream my_annot_pipe_from(my_annot_pipe);
      
      std::ifstream* annot_stream_p = new std::ifstream("annot.txt");

      TextCleaner annot_cleaner(&my_annot_pipe_to, s_encoding, o_hide_xml,
                                o_expand_entities, o_keep_entities_expanded);
      annot_cleaner.setup(annot_stream_p);

      Classifier classifier(mode, prop_id_to_name, precontext, postcontext,
                            features_mask, combined_features,
                            &my_annot_pipe_from, &std::cout);
      classifier.setup("test.txt");


      tbb::pipeline my_pipeline;
      my_pipeline.add_filter(rough_tok);
      my_pipeline.add_filter(feature_extractor);
      my_pipeline.add_filter(classifier);


      boost::thread input_thread(&TextCleaner::do_work,
                                 boost::ref(cleaner));
      boost::thread annot_thread(&TextCleaner::do_work,
                                 boost::ref(annot_cleaner));
      my_pipeline.run(WORK_UNIT_COUNT);
      input_thread.join();
      annot_thread.join();

      training_parameters_t training_parameters;
      classifier.train_model(training_parameters,
                             (build_path / "maxent.model").native());

    } else {

      pipes::pipe my_input_pipe(pipes::pipe::limited_capacity);
      pipes::opipestream my_input_pipe_to(my_input_pipe);
      pipes::ipipestream my_input_pipe_from(my_input_pipe);

      tbb::concurrent_bounded_queue<cutout_t> cutout_queue;
      TextCleaner cleaner(&my_input_pipe_to, s_encoding, o_hide_xml,
                          o_expand_entities, o_keep_entities_expanded,
                          &cutout_queue);
      cleaner.setup(&std::cin);

      RoughTokenizer rough_tok(rough_lexer_wrapper);
      rough_tok.setup(&my_input_pipe_from, "UTF-8");

      FeatureExtractor feature_extractor(n_basic_properties,
                                         regex_properties,
                                         word_to_enum_props);

      Classifier classifier(mode, prop_id_to_name, precontext, postcontext,
                            features_mask, combined_features);
      classifier.load_model((build_path / "maxent.model").native());
      classifier.setup("test.txt");

      pipes::pipe my_output_pipe(pipes::pipe::limited_capacity);
      pipes::opipestream my_output_pipe_to(my_output_pipe);
      pipes::ipipestream my_output_pipe_from(my_output_pipe);
      
      OutputFormatter formatter(&my_output_pipe_to, o_detokenize,
                                o_preserve_segments, o_preserve_paragraphs,
                                &cutout_queue);

      Encoder encoder(&my_output_pipe_from, s_encoding);
      encoder.setup(&std::cout);

      tbb::pipeline my_pipeline;
      my_pipeline.add_filter(rough_tok);
      my_pipeline.add_filter(feature_extractor);
      my_pipeline.add_filter(classifier);
      my_pipeline.add_filter(formatter);


      boost::thread input_thread(&TextCleaner::do_work, boost::ref(cleaner));
      boost::thread output_thread(&Encoder::do_work, boost::ref(encoder));
      my_pipeline.run(WORK_UNIT_COUNT);
      input_thread.join();
      output_thread.join();
    }

    lt_dlexit();
}
