#ifndef TEXT_CLEANER_INCLUDE_GUARD
#define TEXT_CLEANER_INCLUDE_GUARD

#include <string>
#include <istream>
#include <tbb/concurrent_queue.h>

#include "trtok_clean_entities_EntityCleaner"
#include "trtok_clean_xml_XmlCleaner"
#include "cutout_t.hpp"

namespace trtok {

/* The TextCleaner class will be responsible for decoding the input text,
 * stripping off the XML markup and expanding the entities. The class sends
 * the unicode text encoded in UTF-8 to an ostream specified during construction
 * and posts the changes done to the input to a cutout queue so that the XML
 * and entities may be reconstructed on output. */
template <typename charT> class TextCleaner
{
};


/* Due to the nature of XML (contains whitespace) and Quex, one of two
 * preprocessors is used depending on whether the user wants to hide XML
 * or not. Since the preprocessors are defined using different types
 * in different namespaces, the code is repeated twice for both of them
 * through macros. */
#define TEXTCLEANER_DOWORK(cleaner_namespace, cleaner_class, token_prefix, text_getter, charT) cleaner_namespace::cleaner_class lex(m_input_stream, m_input_encoding);\
			cleaner_namespace::Token *token_p = 0x0;\
\
			do {\
				lex.receive(&token_p);\
\
				if (token_p->type_id() == token_prefix##TEXT) {\
					*m_output_stream << token_p->text_getter;\
				}\
				else if (token_p->type_id() == token_prefix##ENTITY) {\
					if (m_expand_entities) {\
						cutout_t cutout;\
						cutout.kind = ENTITY;\
						cutout.position = token_p->number;\
						cutout.text = token_p->pretty_char_text();\
						if (m_cutout_queue_p != 0x0)\
							m_cutout_queue_p->push(cutout);\
						*m_output_stream << expand_entity<charT>(token_p->text); }\
					else\
						*m_output_stream << token_p->test_getter;\
				}\
				else if (token_p->type_id() == token_prefix##XML) {\
					cutout_t cutout;\
					cutout.kind = XML;\
					cutout.position = token_p->number;\
					cutout.text = token_p->pretty_char_text();\
					if (m_cutout_queue_p != 0x0)\
						m_cutout_queue_p->push(cutout);\
				}\
			} while (token_p->type_id() != token_prefix##TERMINATION)


/* When processing annotated reference data, we want the text straight in UTF-8,
 * but when reading the raw data, we prefer USC-4, which can save time which would
 * be spent by the rough tokenizer decoding the UTF-8 to UCS-4. Since UCS-4
 * characters are 4 byte long, we must use a different stream type and also
 * a different way of retrieving Quex's results. */
#define TEXTCLEANER_BODY(ostream_type) private:\
	std::istream *m_input_stream;\
	std::basic_ostream<uint32_t> *m_output_stream;\
	char const *m_input_encoding;\
	bool m_expand_entities, m_hide_xml;\
	tbb::concurrent_queue<cutout_t> *m_cutout_queue_p;\
public:\
	TextCleaner(std::basic_ostream<uint32_t> *output_stream, char const *input_encoding,\
		    bool expand_entities, bool hide_xml,\
		    tbb::concurrent_queue<cutout_t> *cutout_queue_p = 0x0):\
		m_output_stream(output_stream), m_input_encoding(input_encoding),\
		m_expand_entities(expand_entities), m_hide_xml(hide_xml),\
		m_cutout_queue_p(cutout_queue_p) {}\
\
	void setup(std::istream *input_stream)\
	{\
		m_input_stream = input_stream;\
	}

//TODO: to be implemented
template <typename charT> charT const *expand_entity(std::string const &entity) {
	return "X";
}

template <> class TextCleaner<char> {
TEXTCLEANER_BODY(std::basic_ostream<char>)

	void do_work() {
		if (!m_hide_xml) {
			TEXTCLEANER_DOWORK(clean_entities, EntityCleaner, QUEX_PREPROC_NOXML_, pretty_char_text(), char);
		}
		else {
			TEXTCLEANER_DOWORK(clean_xml, XmlCleaner, QUEX_PREPROC_WITHXML_, pretty_char_text(), char);
		}
	}
}

template <> class TextCleaner<uint32_t> {
TEXTCLEANER_BODY(std::basic_ostream<uint32_t>)

public:
	void do_work() {
		if (!m_hide_xml) {
			TEXTCLEANER_DOWORK(clean_entities, EntityCleaner, QUEX_PREPROC_NOXML_, test, uint32_t);
		}
		else {
			TEXTCLEANER_DOWORK(clean_xml, XmlCleaner, QUEX_PREPROC_WITHXML_, test, uint32_t);
		}
	}
}

}
#endif
