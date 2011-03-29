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
	RoughLexerWrapper(): lexer_p(0x0), token_p(0x0) {
		type_id_table[QUEX_ROUGH_TOKEN_PIECE - TOKEN_ID_OFFSET] = TOKEN_PIECE;
		type_id_table[QUEX_ROUGH_MAY_BREAK_SENTENCE - TOKEN_ID_OFFSET] = MAY_BREAK_SENTENCE;
		type_id_table[QUEX_ROUGH_MAY_SPLIT - TOKEN_ID_OFFSET] = MAY_SPLIT;
		type_id_table[QUEX_ROUGH_MAY_JOIN - TOKEN_ID_OFFSET] = MAY_JOIN;
		type_id_table[QUEX_ROUGH_WHITESPACE - TOKEN_ID_OFFSET] = WHITESPACE;
		type_id_table[QUEX_ROUGH_LINE_BREAK - TOKEN_ID_OFFSET] = LINE_BREAK;
		type_id_table[QUEX_ROUGH_PARAGRAPH_BREAK - TOKEN_ID_OFFSET] = PARAGRAPH_BREAK;
	}

	virtual void setup(std::istream *in, char const *encoding) {
		delete lexer_p;
		lexer_p = new quex::RoughLexer(in, encoding);
		token_p = 0x0;
	}

	virtual rough_token_t receive() {
		lexer_p->receive(&token_p);
		rough_token_t out_token;
		if (token_p->type_id() == QUEX_ROUGH_TERMINATION)
			out_token.type_id = TERMINATION;
		else
			out_token.type_id = type_id_table[token_p->type_id() - TOKEN_ID_OFFSET];
		out_token.text = token_p->pretty_char_text();
		return out_token;
	}
private:
	quex::RoughLexer *lexer_p;
	quex::Token *token_p;
	rough_token_id type_id_table[7];
};

/* A factory function which we will retrieve by way of dlopen() and family
 * and use it to construct an instance of the wrapper class. */
IRoughLexerWrapper* make_quex_wrapper() {
	return new RoughLexerWrapper();
}
