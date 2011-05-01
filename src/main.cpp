#include <cstdlib>
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
#include "tbb/tbb_exception.h"
#include "tbb/tick_count.h"
#include <ltdl.h>
#include <pcre.h>
#include <pcrecpp.h>

#include "pipes/pipe.hpp"

#include "configuration.hpp"
#include "config_exception.hpp"
#include "TextCleaner.hpp"
#include "cutout_t.hpp"
#include "rough_tok_compile.hpp"
#include "RoughTokenizer.hpp"
#include "token_t.hpp"
#include "FeatureExtractor.hpp"
#include "Classifier.hpp"
#include "trtok_read_features_FeaturesReader"
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

#define FEATURES_ERROR(message) {\
  std::cerr << features_file << ":" << features_token_p->line_number()\
<< ":" << features_token_p->column_number() << ": Error: "\
<< message << std::endl;\
  return 1;\
}

#define FEATURES_SYNTAX_ERROR(expected) {\
  std::cerr << features_file << ":" << features_token_p->line_number()\
<< ":" << features_token_p->column_number() << ": Error: "\
<< "Syntax error! Expected " << expected << "." << std::endl;\
  return 1;\
}

void report_time(char const *description, double seconds) {
  std::clog << description << ": " << seconds << "s" << std::endl;
}

int main(int argc, char const **argv) {

tbb::tick_count start_time = tbb::tick_count::now();

    // PARSING AND CHECKING THE ARGUMENTS

    /* We declare the following variables which will catch the values
     * of options coming from the command line. */
    string s_mode, s_scheme;
    string s_encoding;
    bool o_help;
    bool o_preserve_paragraphs, o_detokenize, o_preserve_segments;
    bool o_hide_xml, o_expand_entities, o_keep_entities_expanded;
    bool o_print_questions;

    /* We use the Boost Program Options library to handle option parsing.
     * We first start off by enumerating the names, types and descriptions
     * of our options. */
    po::options_description explicit_options;
    explicit_options.add_options()
        ("encoding,c", po::value<string>(&s_encoding)->default_value("UTF-8"),
"Specifies the input and output encoding of the tokenizer. UTF-8 is used if none is specified.")
        ("file-list,l", po::value< vector<string> >()->composing(),
"A list of input files to be processed. If mode is TRAIN and this setting is omitted, the file train.fl in scheme-path is used instead. For the EVALUATE mode, the file evaluate.fl is used as default. If the paths are relative, they are evaluated with respect to the location of the file list. More than 1 file list can be specified.")
        ("filename-regexp,r", po::value<string>()->default_value("(.*)\\.txt/\1.tok"),
"A regular expression/replacement string used to generate a set of pairs of input/output files. Output files are written to in TOKENIZE mode and are used as correct answers in TRAIN and EVALUATE modes. If no such expression is given in TRAIN and EVALUATE modes, the contents of files train.fnre and evaluate.fnre in the directory scheme-path are used instead. If the mode is TOKENIZE, the output of tokenization is printed to the standard output.")
        ("help,h", po::bool_switch(&o_help), "Prints this usage information.")
        ("preserve-paragraphs,p", po::bool_switch(&o_preserve_paragraphs), "Replaces paragraph breaks with a blank line.")
        ("detokenize,d", po::bool_switch(&o_detokenize), "Doesn't apply word splitting and joining decisions. The words found in the input are preserved, whitespace is condensed into single spaces or newlines in case of sentence break.")
        ("preserve-segments,s", po::bool_switch(&o_preserve_segments), "Assumes sentence boundaries are already specified by newlines.")
        ("hide-xml,x", po::bool_switch(&o_hide_xml), "Hides XML markup from the tokenizer and then reintroduces it to the output.")
        ("expand-entities,e", po::bool_switch(&o_expand_entities), "Treats entities as the characters they represent. The output will preserve these characters as entities.")
        ("keep-entities-expanded,k", po::bool_switch(&o_keep_entities_expanded), "Override the behavior of -e so expanded entities are kept expanded in the output instead of being replaced with the original entities.")
        ("print-questions,q", po::bool_switch(&o_print_questions), "Prints the questions presented to the maximum entropy classifier. In TOKENIZE mode, the classifier's answer is present as well; in TRAIN mode, it is the answer induced from the data. In EVALUATE mode, both the answer given by the classifier and the correct answer are output.")
        ;

    /* Positional arguments such as the mode and scheme need to be described
     * as well. They are defined in a different structure as we don't want
     * to list them as options when printing the usage.*/
    po::options_description positional_options;
    positional_options.add_options()
        ("mode", po::value<string>(&s_mode)->required(),"")
        ("scheme", po::value<string>(&s_scheme)->required(), "")
        ;

    // We inform the library of the order of these two positional arguments.
    po::positional_options_description pod;
    pod.add("mode", 1).add("scheme", 1);

    po::options_description all_options;
    all_options.add(explicit_options).add(positional_options);
    po::command_line_parser cmd_line(argc, argv);
    cmd_line.options(all_options).positional(pod);

    po::variables_map vm;

    try {
        po::store(cmd_line.run(), vm);
        po::notify(vm);
    } catch (po::unknown_option const &exc) {
        END_WITH_ERROR("trtok", "Encountered unknown option " << exc.get_option_name() << ". See trtok --help for proper usage.");
    } catch (po::required_option const &exc) {
        END_WITH_ERROR("trtok", "Missing mandatory parameter " << exc.get_option_name() << ". See trtok --help for proper usage.");
    } catch (po::too_many_positional_options_error const &exc) {
        END_WITH_ERROR("trtok", "Too many arguments. See 'trtok --help'.");
    }

tbb::tick_count options_parsed_time = tbb::tick_count::now();
    
    if (o_help) {
        cout << "Usage: trtok <prepare|train|tokenize|evaluate> SCHEME [OPTION]... [FILE]..." << endl;
        cout << explicit_options;
        return 0;
    }

    char *e_trtok_path = getenv("TRTOK_PATH");
    if (e_trtok_path == NULL) {
        END_WITH_ERROR("trtok", "Environment variable TRTOK_PATH is not set. Please set it to the installation directory of trtok.");
    }

    fs::path schemes_root = fs::path(e_trtok_path) / fs::path("schemes");
    if (!fs::is_directory(schemes_root)) {
        END_WITH_ERROR("trtok", "$TRTOK_PATH/schemes is not a directory. Make sure the environment variable TRTOK_PATH is set to the installation directory of trtok and that the tokenization schemes are stored in its subdirectory 'schemes'.");
    }

    fs::path scheme_rel_path = fs::path(s_scheme);
    fs::path scheme_path = schemes_root / scheme_rel_path;
    if (!fs::is_directory(scheme_path)) {
        END_WITH_ERROR("trtok", "Scheme directory " << s_scheme << " not found in the schemes directory.");
    }

    if (find(scheme_rel_path.begin(), scheme_rel_path.end(), "..") != scheme_rel_path.end()) {
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
    // a vocabulary of properties use in features).
    for (fs::directory_iterator j(scheme_path); j != fs::directory_iterator(); j++) {
        fs::path file_path = j->path();
        if (!fs::is_directory(file_path)) {
            relevant_files.push_back(file_path);
        }
    }
    for (fs::path::const_iterator i = scheme_rel_path.begin(); i != scheme_rel_path.end(); i++) {
        scheme_path /= *i;
        for (fs::directory_iterator j(scheme_path); j != fs::directory_iterator(); j++) {
            fs::path file_path = j->path();
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
    vector<fs::path> split_files, join_files, begin_files, end_files, enump_files, rep_files;
    vector<fs::path> fl_files, fnre_files;
    fs::path features_file;
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
     * The following "duplicates" are guaranteed to be less specific.*/
    string last_filename = "";
    for (vector<fs::path>::const_iterator i = relevant_files.begin(); i != relevant_files.end(); i++) {
        // Scheme inheritance: only the most specific instance of a file
        if (i->filename() == last_filename) {
            continue;
        }

        typedef boost::unordered_map< string, vector<fs::path>* >::iterator map_iter;
        map_iter lookup = file_vectors.find(i->extension().string());
        if (lookup != file_vectors.end()) {
            lookup->second->push_back(*i);
        }
        else if ((i->filename() == "features") || (i->filename() == "FEATURES")) {
            features_file = *i;
        }
        last_filename = i->filename().string();
    }

    fs::path build_path = fs::path(e_trtok_path) / fs::path("build") / scheme_rel_path;
    fs::create_directories(build_path);

tbb::tick_count config_files_found_time = tbb::tick_count::now();

    // COMPILING AND LOADING THE ROUGH TOKENIZER
    try {
        compile_rough_lexer(split_files, join_files, begin_files, end_files, build_path);
    } catch (config_exception const &exc) {
        std::cerr << exc.what() << endl;
        return 1;
    }
    
tbb::tick_count roughtok_compiled_time = tbb::tick_count::now();

    int return_code = lt_dlinit();
    if (return_code != 0) {
        END_WITH_ERROR("ltdl", "lt_dlinit returned " << return_code + ".");
    }
    
    fs::path roughtok_wrapper_path = build_path / fs::path("roughtok");
    lt_dlhandle libroughtok = lt_dlopen(roughtok_wrapper_path.c_str());
    if (libroughtok == NULL) {
        END_WITH_ERROR(roughtok_wrapper_path.native(), "Cannot load the compiled rough tokenizer. Oh no! Something bad must have happened in the build system... Why? : " << lt_dlerror());
    }

    void *factory_func_void_p = lt_dlsym(libroughtok, "make_quex_wrapper");
    if (factory_func_void_p == NULL) {
        END_WITH_ERROR(roughtok_wrapper_path.native(), "No extern make_quex_wrapper in the wrapper! Why? : " << lt_dlerror());
    }
    typedef IRoughLexerWrapper* (*factory_func_t)(void);
    factory_func_t factory_func_p = (factory_func_t)factory_func_void_p;
    IRoughLexerWrapper *rough_lexer_wrapper = factory_func_p();

tbb::tick_count roughtok_loaded_time = tbb::tick_count::now();

    // READING AND PARSING THE PROPERTY DEFINITIONS
    
    // A mapping from property names to indices.
    boost::unordered_map<std::string, int> prop_name_to_id;
    // and a mapping from property indices to names
    std::vector<std::string> prop_id_to_name;
    int n_properties = 0;
    int n_regular_properties = 0;

    std::vector<pcrecpp::RE> regex_properties;
    for (std::vector<fs::path>::const_iterator i = rep_files.begin();
         i != rep_files.end(); i++) {
        std::string regex_string("");
        fs::ifstream regex_file(*i);
        std::string line;
        while (getline(regex_file, line)) {
            if ((line.length() == 0) || (line[0] == '#'))
                continue;
            if (regex_string == "")
                regex_string = line;
            else {
                END_WITH_ERROR(i->native(),            "More than 1 non-blank non-comment line in regex property definition file.");
            }
        }
        pcrecpp::RE regex(regex_string, PCRE_UTF8 | PCRE_UCP);
        if (regex.error() != "") {
            END_WITH_ERROR(i->native(), "The following error occured when compiling the regular expression: " << regex.error());
        }
        regex_properties.push_back(regex);

        prop_name_to_id[i->stem().string()] = n_properties;
        prop_id_to_name.push_back(i->stem().string());
        n_properties++;
        n_regular_properties++;
    }

tbb::tick_count regex_properties_compiled_time = tbb::tick_count::now();

    std::multimap<std::string, int> word_to_enum_props;
    for (std::vector<fs::path>::const_iterator i = enump_files.begin();
         i != enump_files.end(); i++) {
        fs::ifstream enum_file(*i);
        std::string line;
        while (getline(enum_file, line)) {
            if (line.length() == 0)
                continue;
            word_to_enum_props.insert(std::make_pair(line, n_properties));
        }

        prop_name_to_id[i->stem().string()] = n_properties;
        prop_id_to_name.push_back(i->stem().string());
        n_properties++;
        n_regular_properties++;
    }

    prop_name_to_id["%length"] = n_properties;
    prop_id_to_name.push_back("%length");
    n_properties++;

    prop_name_to_id["%Word"] = n_properties;
    prop_id_to_name.push_back("%Word");
    n_properties++;

tbb::tick_count enum_properties_compiled_time = tbb::tick_count::now();

    // PARSING THE FEATURE SELECTION FILE

    if (features_file.empty())
      END_WITH_ERROR("trtok", "No features file found.");

    read_features::FeaturesReader features_lex(features_file.native(),"UTF-8");
    read_features::Token *features_token_p = 0x0;

    std::vector< std::pair< std::vector<int>,std::vector<int> > > features;
    std::vector< std::vector< std::pair<int,int> > > combined_features;
    try {
    typedef boost::unordered_map<std::string, int>::const_iterator
          lookup_iter;
    do {
      features_lex.receive(&features_token_p);
      if (features_token_p->type_id() == QUEX_FEATURES_NUMBER) {
        std::vector<int> offsets;
        do {
          int left = boost::lexical_cast<int>
            (features_token_p->pretty_char_text());
          features_lex.receive(&features_token_p);
          if (features_token_p->type_id() == QUEX_FEATURES_DOUBLEDOT) {
        // an interval
        features_lex.receive(&features_token_p);
        if (features_token_p->type_id() != QUEX_FEATURES_NUMBER)
          FEATURES_SYNTAX_ERROR("a number");
        int right = boost::lexical_cast<int>
              (features_token_p->pretty_char_text());
        if (right < left)
          FEATURES_ERROR("Interval upper bound lower than lower bound.");
        for (int offset = left; left <= right; left++) {
          offsets.push_back(offset);
        }
        features_lex.receive(&features_token_p);
          } else if ((features_token_p->type_id() == QUEX_FEATURES_COMMA)
             || (features_token_p->type_id() == QUEX_FEATURES_COLON)) {
        // a single offset
        offsets.push_back(left);
          } else FEATURES_SYNTAX_ERROR("a double-dot, a comma or a colon");
          if (features_token_p->type_id() == QUEX_FEATURES_COMMA) {
        features_lex.receive(&features_token_p);
          } else if (features_token_p->type_id() != QUEX_FEATURES_COLON)
        FEATURES_SYNTAX_ERROR("a comma or a colon");
        } while (features_token_p->type_id() == QUEX_FEATURES_NUMBER);
        features_lex.receive(&features_token_p);
        
        std::vector<int> properties;
        while ((features_token_p->type_id() == QUEX_FEATURES_IDENTIFIER)
        || (features_token_p->type_id() == QUEX_FEATURES_LPAREN)
        || (features_token_p->type_id() == QUEX_FEATURES_STAR)) {
          if (features_token_p->type_id() == QUEX_FEATURES_STAR) {
        for (int property = 0; property != n_regular_properties;
         property++) {
          properties.push_back(property);
        }
        features_lex.receive(&features_token_p);
        if (features_token_p->type_id() == QUEX_FEATURES_COMMA)
          features_lex.receive(&features_token_p);
        else if (features_token_p->type_id() != QUEX_FEATURES_SEMICOLON)
          FEATURES_SYNTAX_ERROR("a comma or a semicolon");
          } else if (features_token_p->type_id() == QUEX_FEATURES_IDENTIFIER) {
        lookup_iter lookup =
          prop_name_to_id.find(features_token_p->pretty_char_text());
        if (lookup == prop_name_to_id.end())
          FEATURES_ERROR("Property \""
            << features_token_p->pretty_char_text() << "\" not defined.");
        properties.push_back(lookup->second);
        features_lex.receive(&features_token_p);
        if (features_token_p->type_id() == QUEX_FEATURES_COMMA)
          features_lex.receive(&features_token_p);
        else if (features_token_p->type_id() != QUEX_FEATURES_SEMICOLON)
          FEATURES_SYNTAX_ERROR("a comma or a semicolon");
          } else if (features_token_p->type_id() == QUEX_FEATURES_LPAREN) {
        features_lex.receive(&features_token_p);
        std::vector<int> constituent_properties;
        while (features_token_p->type_id() == QUEX_FEATURES_IDENTIFIER) {
          lookup_iter lookup = 
            prop_name_to_id.find(features_token_p->pretty_char_text());
          if (lookup == prop_name_to_id.end())
            FEATURES_ERROR("Property \""
              << features_token_p->pretty_char_text() << "\" not defined.");
          constituent_properties.push_back(lookup->second);
          features_lex.receive(&features_token_p);
          if (features_token_p->type_id() == QUEX_FEATURES_COMBINE)
            features_lex.receive(&features_token_p);
          else if (features_token_p->type_id() != QUEX_FEATURES_RPAREN)
            FEATURES_SYNTAX_ERROR("a hat or a right parenthesis");
        }
        if (features_token_p->type_id() != QUEX_FEATURES_RPAREN)
          FEATURES_SYNTAX_ERROR("a hat or a right parenthesis");
        for (vector<int>::const_iterator offset = offsets.begin();
         offset != offsets.end(); offset++) {
          std::vector< std::pair <int,int> > combined_feature;
          for (vector<int>::const_iterator property =
           constituent_properties.begin();
           property != constituent_properties.end(); property++) {
            combined_feature.push_back
              (std::make_pair(*offset, *property));
          }
          combined_features.push_back(combined_feature);
        }
        features_lex.receive(&features_token_p);
        if (features_token_p->type_id() == QUEX_FEATURES_COMMA)
          features_lex.receive(&features_token_p);
        else if (features_token_p->type_id() != QUEX_FEATURES_SEMICOLON)
          FEATURES_SYNTAX_ERROR("a comma or a semicolon");
          }
        }
        features.push_back(std::make_pair(offsets, properties));
      } else if (features_token_p->type_id() == QUEX_FEATURES_LPAREN) {
        features_lex.receive(&features_token_p); 
        std::vector< std::pair <int,int> > combined_feature;
        while (features_token_p->type_id() == QUEX_FEATURES_NUMBER) {
          int offset = boost::lexical_cast<int>
            (features_token_p->pretty_char_text());
          features_lex.receive(&features_token_p);
          if (features_token_p->type_id() != QUEX_FEATURES_COLON)
        FEATURES_SYNTAX_ERROR("a colon");
          features_lex.receive(&features_token_p);
          if (features_token_p->type_id() != QUEX_FEATURES_IDENTIFIER)
        FEATURES_SYNTAX_ERROR("an identifier");
          lookup_iter lookup = 
        prop_name_to_id.find(features_token_p->pretty_char_text());
          if (lookup == prop_name_to_id.end())
        FEATURES_ERROR("Property \""
          << features_token_p->pretty_char_text() << "\" not defined.");
          combined_feature.push_back(std::make_pair(offset, lookup->second));
          features_lex.receive(&features_token_p);
          if (features_token_p->type_id() == QUEX_FEATURES_COMBINE)
        features_lex.receive(&features_token_p);
          else if (features_token_p->type_id() != QUEX_FEATURES_RPAREN)
        FEATURES_SYNTAX_ERROR("a hat or a right parenthesis");
        }
        combined_features.push_back(combined_feature);
        if (features_token_p->type_id() != QUEX_FEATURES_RPAREN)
          FEATURES_SYNTAX_ERROR("a hat or a right parenthesis");
      } else if (features_token_p->type_id() != QUEX_FEATURES_TERMINATION)
        FEATURES_SYNTAX_ERROR("a number or a left parenthesis");
    } while (features_token_p->type_id() != QUEX_FEATURES_TERMINATION);
    } catch (std::runtime_error) {
      std::cerr << features_file << ":" << features_lex.line_number()
        << ":" << features_lex.column_number() << ": Error: "
        << "Unexpected symbol." << std::endl;
      return 1;
    }

    // INTERPRETING THE FEATURES FILE

    int precontext = 0, postcontext = 0;
    for (std::vector< std::pair< std::vector<int>,std::vector<int> > >::
     const_iterator features_line = features.begin();
     features_line != features.end();
     features_line++) {
      for (std::vector<int>::const_iterator offset =
       features_line->first.begin();
       offset != features_line->first.end();
       offset++) {
        postcontext = max(postcontext, *offset);
        precontext = -max(precontext, -(*offset));
      }
    }

    for (std::vector< std::vector< std::pair<int,int> > >::const_iterator
     combined_feature = combined_features.begin();
     combined_feature != combined_features.end();
     combined_feature++) {
      for (std::vector< std::pair<int,int> >::const_iterator
       constituent_feature = combined_feature->begin();
       constituent_feature != combined_feature->end();
       constituent_feature++) {
        postcontext = max(postcontext, constituent_feature->first);
        precontext = -max(precontext, -constituent_feature->first);
      }
    }

    bool *features_map =
        new bool[precontext + 1 + postcontext, n_properties];
    for (int i = 0; i < precontext + 1 + postcontext; i++) {
      for (int j = 0; j < n_properties; j++) {
        features_map[i,j] = false;
      }
    }

    for (std::vector< std::pair< std::vector<int>,std::vector<int> > >::const_iterator features_line = features.begin();
     features_line != features.end();
     features_line++) {
      for (std::vector<int>::const_iterator offset =
       features_line->first.begin();
       offset != features_line->first.end();
       offset++) {
        for (std::vector<int>::const_iterator property =
         features_line->second.begin();
         property != features_line->second.end();
         property++)
          features_map[*offset, *property] = true;
      }
    }

tbb::tick_count features_file_parsed_time = tbb::tick_count::now();

    // Testing code
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

    FeatureExtractor feature_extractor(n_regular_properties, regex_properties,
                                       word_to_enum_props);

    std::ifstream* annot_stream = new std::ifstream("annot.txt");
    Classifier classifier(s_mode, prop_id_to_name, precontext, postcontext,
                          features_map, combined_features, o_print_questions,
                          annot_stream);

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


tbb::tick_count pipeline_initialized_time = tbb::tick_count::now();

    boost::thread input_thread(&TextCleaner::do_work, boost::ref(cleaner));
    boost::thread output_thread(&Encoder::do_work, boost::ref(encoder));
    my_pipeline.run(WORK_UNIT_COUNT);
    input_thread.join();
    output_thread.join();

tbb::tick_count pipeline_finished_time = tbb::tick_count::now();

    training_parameters_t training_parameters;
    classifier.train_model(training_parameters,
                           (build_path / "maxent.model").native());

tbb::tick_count all_done_time = tbb::tick_count::now();

    report_time("Total time",
        (all_done_time - start_time).seconds());
    report_time("Total init time",
        (pipeline_initialized_time - start_time).seconds());
    report_time("Total pipeline time",
        (pipeline_finished_time - pipeline_initialized_time).seconds());
    report_time("Total training time",
        (all_done_time - pipeline_finished_time).seconds());
    report_time("Parsing options",
        (options_parsed_time - start_time).seconds());
    report_time("Config file lookup",
        (config_files_found_time - options_parsed_time).seconds());
    report_time("Rough tokenizer compilation",
        (roughtok_compiled_time - config_files_found_time).seconds());
    report_time("Rough tokenizer loading",
        (roughtok_loaded_time - roughtok_compiled_time).seconds());
    report_time("Regular expression property compilation",
        (regex_properties_compiled_time - roughtok_loaded_time).seconds());
    report_time("Enumerated property compilation",
        (enum_properties_compiled_time -
         regex_properties_compiled_time).seconds());
    report_time("Parsing features file",
        (features_file_parsed_time - enum_properties_compiled_time).seconds());
    report_time("Pipeline construction",
        (pipeline_initialized_time - features_file_parsed_time).seconds());

    lt_dlexit();
}
