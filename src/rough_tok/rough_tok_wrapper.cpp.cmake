#include <istream>

#include "RoughLexer"

#include "rough_tok_wrapper.hpp"


// Quex adds a value to all user-defined token types. We want to translate
// Quex's type ids into our type ids which includes moving the range Quex
// uses into a zero-based one (by substracting the offset of token ids).
// This value will be set by CMake to the same value sent to Quex so it
// won't be sabotaged by changes in the implementation of Quex.
#define TOKEN_ID_OFFSET @QUEX_TOKEN_ID_OFFSET@

class RoughLexerWrapper : public IRoughLexerWrapper {
public:
	RoughLexerWrapper(): m_lexer_p(0x0), m_token_p(0x0) {
		type_id_table[QUEX_ROUGH_TOKEN_PIECE - TOKEN_ID_OFFSET] = TOKEN_PIECE;
		type_id_table[QUEX_ROUGH_MAY_BREAK_SENTENCE - TOKEN_ID_OFFSET] = MAY_BREAK_SENTENCE;
		type_id_table[QUEX_ROUGH_MAY_SPLIT - TOKEN_ID_OFFSET] = MAY_SPLIT;
		type_id_table[QUEX_ROUGH_MAY_JOIN - TOKEN_ID_OFFSET] = MAY_JOIN;
		type_id_table[QUEX_ROUGH_WHITESPACE - TOKEN_ID_OFFSET] = WHITESPACE;
		type_id_table[QUEX_ROUGH_LINE_BREAK - TOKEN_ID_OFFSET] = LINE_BREAK;
		type_id_table[QUEX_ROUGH_PARAGRAPH_BREAK - TOKEN_ID_OFFSET] = PARAGRAPH_BREAK;
	}

	virtual void setup(std::istream *in, char const *encoding) {
		m_in = in;
		m_encoding = encoding;
		if (m_lexer_p == 0x0)
			m_lexer_p = new quex::RoughLexer(m_in, m_encoding);
		else
			m_lexer_p->reset(m_in, m_encoding);
		m_token_p = 0x0;
	}

	virtual void reset() {
		m_lexer_p->reset(m_in, m_encoding);
	}

	virtual rough_token_t receive() {
		m_lexer_p->receive(&m_token_p);
		rough_token_t out_token;
		if (m_token_p->type_id() == QUEX_ROUGH_TERMINATION)
			out_token.type_id = TERMINATION;
		else
			out_token.type_id = type_id_table[m_token_p->type_id() - TOKEN_ID_OFFSET];
		if (out_token.type_id == TOKEN_PIECE)
			out_token.text = m_token_p->pretty_char_text();
		return out_token;
	}
private:
	quex::RoughLexer *m_lexer_p;
	quex::Token *m_token_p;
	std::istream *m_in;
	char const *m_encoding;
	rough_token_id type_id_table[7];
};

/* A factory function which we will retrieve by way of dlopen() and family
 * and use it to construct an instance of the wrapper class. */
extern "C" IRoughLexerWrapper* make_quex_wrapper() {
	return new RoughLexerWrapper();
}
