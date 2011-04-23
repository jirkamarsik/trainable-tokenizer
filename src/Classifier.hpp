#include <iostream>
#include <string>
#include "tbb/pipeline.h"
#include <boost/cstdint.hpp>
typedef boost::uint32_t uint32_t;

namespace trtok {

class Classifier: public tbb::filter {
public:
	Classifier(std::string mode, bool print_questions, std::istream *annot_stream_p = NULL):
		tbb::filter(tbb::filter::serial_in_order), m_mode(mode),
		m_print_questions(print_questions), m_annot_stream_p(annot_stream_p)
	{ reset(); }

	void reset() {
            first_chunk = true;
        }

	virtual void* operator()(void *input_p);
private:
        bool consume_whitespace();
        void report_alignment_warning(std::string occurence_type,
            std::string prefix, std::string suffix, std::string advice);
private:
        // Configuration
	std::string m_mode;
	bool m_print_questions;
	std::istream *m_annot_stream_p;

        // State
        uint32_t annot_char;
        bool first_chunk;
};

}
