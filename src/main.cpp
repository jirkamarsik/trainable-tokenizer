#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
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

// FOR DEBUGGING PURPOSES
void print_path(fs::path const &path) {
	cout << path << endl;
}

int main(int argc, char const **argv) {
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
		cerr << "Encountered unknown option " << exc.get_option_name() << ". See trtok --help for proper usage." << endl;
		return 1;
	} catch (po::required_option const &exc) {
		cerr << "Missing mandatory parameter " << exc.get_option_name() << ". See trtok --help for proper usage." << endl;
		return 1;
	}
	
	if (o_help) {
		cout << "Usage: trtok <prepare|train|tokenize|evaluate> SCHEME [OPTION]... [FILE]..." << endl;
		cout << explicit_options;
		return 0;
	}

	char *e_trtok_path = getenv("TRTOK_PATH");
	if (e_trtok_path == NULL) {
		cerr << "Error: Environment variable TRTOK_PATH is not set.\n"
			"       Please set it to the installation directory of trtok.\n";
		return 1;
	}

	fs::path schemes_root = fs::path(e_trtok_path) / fs::path("schemes");
	if (!fs::is_directory(schemes_root)) {
		cerr << "Error: $TRTOK_PATH/schemes is not a directory.\n"
			"       Make sure the environment variable TRTOK_PATH is set to the installation\n"
			"       directory of trtok and that the tokenization schemes are stored in\n"
			"       its subdirectory 'schemes'.\n";
		return 1;
	}

	fs::path scheme_rel_path = fs::path(s_scheme);
	fs::path scheme_path = schemes_root / scheme_rel_path;
	if (!fs::is_directory(scheme_path)) {
		cerr << "Error: Scheme directory " << s_scheme << " not found in the schemes directory.\n";
		return 1;
	}

	if (find(scheme_rel_path.begin(), scheme_rel_path.end(), "..") != scheme_rel_path.end()) {
		cerr << "Error: The double-dot isn't allowed in scheme paths.\n";
		return 1;
	}


	// READING THE CONFIG FILES
	//
	// Variables still relevant: 
	//	schemes_root / scheme_rel_path == scheme_path 
	//	o_*, s_* options and settings
	//	e_* environment variables


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
		last_filename = i->filename().string();
	}

	fs::path build_path = fs::path(e_trtok_path) / fs::path("build") / scheme_rel_path;
	fs::create_directories(build_path);


	// COMPILING AND LOADING THE ROUGH TOKENIZER
	try {
		compile_rough_lexer(split_files, join_files, begin_files, end_files, build_path);
	} catch (config_exception const &exc) {
		cerr << exc.what() << endl;
		return 1;
	}

	int return_code = lt_dlinit();
	if (return_code != 0) {
		std::cerr << "Error: lt_dlinit returned " << return_code << "." << std::endl;
		return 1;
	}
	
	fs::path roughtok_wrapper_path = build_path / fs::path("roughtok");
	lt_dlhandle libroughtok = lt_dlopen(roughtok_wrapper_path.c_str());
	if (libroughtok == NULL) {
		std::cerr << "Error: Cannot load the compiled rough tokenizer.\n"
			     "       Oh no! Something bad must have happened in the build system...\n"
			     "Why? : " << lt_dlerror() << std::endl;
		return 1;
	}

	void *factory_func_void_p = lt_dlsym(libroughtok, "make_quex_wrapper");
	if (factory_func_void_p == NULL) {
		std::cerr << "Error: No extern make_quex_wrapper in the wrapper!\n"
			     "Why? : " <<  lt_dlerror() << std::endl;
		return 1;
	}
	typedef IRoughLexerWrapper* (*factory_func_t)(void);
	factory_func_t factory_func_p = (factory_func_t)factory_func_void_p;
	IRoughLexerWrapper *rough_lexer_wrapper = factory_func_p();


	// READING AND PARSING THE PROPERTY DEFINITIONS
	boost::unordered_map<std::string, int> prop_name_to_id;
	int n_properties = 0;

	std::vector<pcrecpp::RE> regex_properties;
	for (std::vector<fs::path>::const_iterator i = rep_files.begin();
	     i != rep_files.end(); i++) {
		prop_name_to_id[i->stem().string()] = n_properties;
		std::string regex_string("");
		fs::ifstream regex_file(*i);
		while (regex_file) {
			std::string line;
			getline(regex_file, line);
			if ((line.length() == 0) || (line[0] == '#'))
				continue;
			if (regex_string == "")
				regex_string = line;
			else {
				std::cerr << *i <<
				": Error: More than 1 non-blank non-comment line in regex property definition file."
				<< std::endl;
				return 1;
			}
		}
		pcrecpp::RE regex(regex_string, PCRE_UTF8 | PCRE_UCP);
		if (regex.error() != "") {
			std::cerr << *i <<
			": Error: The following error occured when compiling the regular expression:\n"
			<< regex.error() << std::endl;
			return 1;
		}
		regex_properties.push_back(regex);
		n_properties++;
	}

	std::multimap<std::string, int> word_to_enum_props;
	for (std::vector<fs::path>::const_iterator i = enump_files.begin();
	     i != enump_files.end(); i++) {
		prop_name_to_id[i->stem().string()] = n_properties;
		fs::ifstream enum_file(*i);
		while (enum_file) {
			std::string line;
			getline(enum_file, line);
			if (line.length() == 0)
				continue;
			word_to_enum_props.insert(std::make_pair(line, n_properties));
		}
		n_properties++;
	}
	

	// Testing code
	pipes::pipe my_input_pipe(pipes::pipe::limited_capacity);
	pipes::opipestream my_input_pipe_to(my_input_pipe);
	pipes::ipipestream my_input_pipe_from(my_input_pipe);

	tbb::concurrent_bounded_queue<cutout_t> cutout_queue;
	TextCleaner cleaner(&my_input_pipe_to, s_encoding, o_hide_xml, o_expand_entities, o_keep_entities_expanded, &cutout_queue);
	cleaner.setup(&std::cin);

	RoughTokenizer rough_tok(rough_lexer_wrapper);
	rough_tok.setup(&my_input_pipe_from, "UTF-8");

	FeatureExtractor feature_extractor(n_properties, regex_properties, word_to_enum_props);

	pipes::pipe my_output_pipe(pipes::pipe::limited_capacity);
	pipes::opipestream my_output_pipe_to(my_output_pipe);
	pipes::ipipestream my_output_pipe_from(my_output_pipe);
	
	OutputFormatter formatter(&my_output_pipe_to, o_detokenize, o_preserve_segments, o_preserve_paragraphs, &cutout_queue);

	Encoder encoder(&my_output_pipe_from, s_encoding);
	encoder.setup(&std::cout);

	tbb::pipeline my_pipeline;
	my_pipeline.add_filter(rough_tok);
	my_pipeline.add_filter(feature_extractor);
	my_pipeline.add_filter(formatter);


	tbb::tick_count t0 = tbb::tick_count::now();

	boost::thread input_thread(&TextCleaner::do_work, boost::ref(cleaner));
	boost::thread output_thread(&Encoder::do_work, boost::ref(encoder));
	my_pipeline.run(WORK_UNIT_COUNT);
	input_thread.join();
	output_thread.join();

	tbb::tick_count t1 = tbb::tick_count::now();
	std::cout << (t1 - t0).seconds() << "s" << std::endl;

	lt_dlexit();
}
