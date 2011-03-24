#include <iostream>
#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include <tbb/concurrent_queue.h>

#include "text_cleaner.hpp"
#include "cutout_t.hpp"

using namespace std;
using namespace trtok;
namespace po = boost::program_options;

int main(int argc, char const **argv) {
	string s_mode, s_scheme;
	string s_encoding;
	bool o_help;
	bool o_preserve_paragraphs, o_detokenize, o_preserve_segments;
	bool o_hide_xml, o_expand_entities, o_keep_entities_expanded;
	bool o_print_questions;

	po::options_description explicit_options;
	explicit_options.add_options()
		("encoding,c", po::value<string>(&s_encoding)->default_value("UTF-8"),
"Specifies the input and output encoding of the tokenizer. UTF-8 is used if none is specified.")
		("file-list,l", po::value< vector<string> >()->composing(),
"A list of input files to be processed. If mode is TRAIN and this setting is omitted, the file train.fl in scheme-path is used instead. For the EVALUATE mode, the file evaluate.fl is used as default. If the paths are relative, they are evaluated with respect to the location of the file list. More than 1 file list can be specified.")
		("filename-regexp,r", po::value<string>()->default_value("(.*)\\.txt/\1.tok"),
"A regular expression/replacement string used to generate a set of pairs of input/output files. Output files are written to in TOKENIZE mode and are used as correct answers in TRAIN and EVALUATE modes. If no such expression is given in TRAIN and EVALUATE modes, the contents of files train.fnre and evaluate.fnre in the directory scheme-path are used instead. If the mode is TOKENIZE, the output of tokenization is printed to the standard output.")
		("help,h", "Prints this usage information.")
		("preserve-paragraphs,p", po::bool_switch(&o_preserve_paragraphs), "Replaces paragraph breaks with a blank line.")
		("detokenize,d", po::bool_switch(&o_detokenize), "Doesn't apply word splitting and joining decisions. The words found in the input are preserved, whitespace is condensed into single spaces or newlines in case of sentence break.")
		("preserve-segments,s", po::bool_switch(&o_preserve_segments), "Assumes sentence boundaries are already specified by newlines.")
		("hide-xml,x", po::bool_switch(&o_hide_xml), "Hides XML markup from the tokenizer and then reintroduces it to the output.")
		("expand-entities,e", po::bool_switch(&o_expand_entities), "Treats entities as the characters they represent. The output will preserve these characters as entities.")
		("keep-entities-expanded,k", po::bool_switch(&o_keep_entities_expanded), "Override the behavior of -e so expanded entities are kept expanded in the output instead of being replaced with the original entities.")
		("print-questions,q", "Prints the questions presented to the maximum entropy classifier. In TOKENIZE mode, the classifier's answer is present as well; in TRAIN mode, it is the answer induced from the data. In EVALUATE mode, both the answer given by the classifier and the correct answer are output.")
		;

	po::options_description positional_options;
	positional_options.add_options()
		("mode", po::value<string>(&s_mode),"")
		("scheme", po::value<string>(&s_scheme), "")
		;
	
	po::positional_options_description pod;
	pod.add("mode", 1).add("scheme", 1);

	po::options_description all_options;
	all_options.add(explicit_options).add(positional_options);
	po::command_line_parser cmd_line(argc, argv);
	cmd_line.options(all_options).positional(pod);

	po::variables_map vm;
	bool error_in_args = false;

	try {
		po::store(cmd_line.run(), vm);
		po::notify(vm);
	} catch (po::unknown_option) {
		error_in_args = true;
	}

	if (vm.count("help") || error_in_args) {
		cout << "Usage: trtok <prepare|train|tokenize|evaluate> SCHEME [OPTION]... [FILE]..." << endl;
		cout << explicit_options;
		exit(error_in_args ? 1 : 0);
	}



	// Testing code
	tbb::concurrent_queue<cutout_t> cutout_queue;

	TextCleaner cleaner(&cout, s_encoding, o_hide_xml, o_expand_entities, o_keep_entities_expanded, &cutout_queue);
	cleaner.setup(&cin);
	cleaner.do_work();

	typedef tbb::concurrent_queue<cutout_t>::const_iterator iter;
	for (iter i = cutout_queue.unsafe_begin(); i != cutout_queue.unsafe_end(); i++) {
		cout << (i->kind == ENTITY ? "ENTITY" : "XML") << " " << i->position << " " << i->text << " " << i->text.length() << endl;
	}
}
