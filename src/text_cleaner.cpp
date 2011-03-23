#include <string>

#include "text_cleaner.hpp"
#include "trtok_clean_entities_EntityCleaner"
#include "trtok_clean_xml_XmlCleaner"
#include "cutout_t.hpp"


/* Due to the nature of XML (contains whitespace) and Quex, one of two
 * preprocessors is used depending on whether the user wants to hide XML
 * or not. Since the preprocessors are defined using different types
 * in different namespaces, the code is repeated twice for both of them
 * through macros. */
#define TEXTCLEANER(cleaner_namespace, cleaner_class, token_prefix, unicode_to_utf8) cleaner_namespace::cleaner_class lex(m_input_stream, m_input_encoding);\
		lex.expand_entities = m_expand_entities;\
		cleaner_namespace::Token *token_p = 0x0;\
\
		do {\
			lex.receive(&token_p);\
\
			if (token_p->type_id() == token_prefix##TEXT) {\
				*m_output_stream << unicode_to_utf8(token_p->get_text());\
			}\
			else if (token_p->type_id() == token_prefix##ENTITY) {\
				if (m_expand_entities) {\
					cutout_t cutout;\
					cutout.kind = ENTITY;\
					cutout.position = token_p->number;\
					cutout.text = unicode_to_utf8(token_p->get_text());\
					if (m_cutout_queue_p != 0x0)\
						m_cutout_queue_p->push(cutout);\
					*m_output_stream << expand_entity(unicode_to_utf8(token_p->get_text())); }\
				else\
					*m_output_stream << unicode_to_utf8(token_p->get_text());\
			}\
			else if (token_p->type_id() == token_prefix##XML) {\
				cutout_t cutout;\
				cutout.kind = XML;\
				cutout.position = token_p->number;\
				cutout.text = unicode_to_utf8(token_p->get_text());\
				if (m_cutout_queue_p != 0x0)\
					m_cutout_queue_p->push(cutout);\
			}\
		} while (token_p->type_id() != token_prefix##TERMINATION)


namespace trtok {

//TODO: to be implemented
char const *expand_entity(std::string const &entity) {
	return "X";
}

void TextCleaner::do_work()
{
	if (!m_hide_xml)
	{
		TEXTCLEANER(clean_entities, EntityCleaner, QUEX_PREPROC_NOXML_, clean_entities::EntityCleaner_unicode_to_char);
	}
	else
	{
		TEXTCLEANER(clean_xml, XmlCleaner, QUEX_PREPROC_WITHXML_, clean_entities::EntityCleaner_unicode_to_char);
	}
}

}
