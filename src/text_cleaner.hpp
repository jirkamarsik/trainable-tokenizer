#ifndef TEXT_CLEANER_INCLUDE_GUARD
#define TEXT_CLEANER_INCLUDE_GUARD

#include <string>
#include <istream>
#include <tbb/concurrent_queue.h>

#include "cutout_t.hpp"

namespace trtok {

/* The TextCleaner class will be responsible for decoding the input text,
 * stripping off the XML markup and expanding the entities. The class sends
 * the unicode text encoded in UTF-8 to an ostream specified during construction
 * and posts the changes done to the input to a cutout queue so that the XML
 * and entities may be reconstructed on output. */
class TextCleaner
{
public:
	TextCleaner(std::ostream *output_stream, char const *input_encoding,
		    bool expand_entities, bool hide_xml,
		    tbb::concurrent_queue<cutout_t> *cutout_queue_p = 0x0):
		m_output_stream(output_stream), m_input_encoding(input_encoding),
		m_expand_entities(expand_entities), m_hide_xml(hide_xml),
		m_cutout_queue_p(cutout_queue_p) {}

	void setup(std::istream *input_stream)
	{
		m_input_stream = input_stream;
	}

	void do_work();
private:
	std::istream *m_input_stream;
	std::ostream *m_output_stream;
	char const *m_input_encoding;
	bool m_expand_entities, m_hide_xml;
	tbb::concurrent_queue<cutout_t> *m_cutout_queue_p;

};
}

#endif
