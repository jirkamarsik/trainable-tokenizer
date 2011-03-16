#include <istream>

#include "RoughLexer"

#include "common.hpp"


// Quex adds a value to all user-defined token types. We want to translate
// Quex's type ids into our type ids which includes moving the range Quex
// uses into a zero-based one (by substracting the offset of token ids).
// This value will be set by CMake to the same value sent to Quex so it
// won't be sabotaged by changes in the implementation of Quex.
#define TOKEN_ID_OFFSET 10000

class RoughLexerWrapper : public IRoughLexerWrapper {
public:
	RoughLexerWrapper(std::istream* in, char const* encoding, bool byte_order_reversion):
	    lexer(in, encoding, byte_order_reversion), token_p(0x0) {
		type_id_table[QUEX_TKN_TOKEN_PIECE - TOKEN_ID_OFFSET] = TOKEN_PIECE;
		type_id_table[QUEX_TKN_MAY_BREAK_SENTENCE - TOKEN_ID_OFFSET] = MAY_BREAK_SENTENCE;
		type_id_table[QUEX_TKN_MAY_SPLIT - TOKEN_ID_OFFSET] = MAY_SPLIT;
		type_id_table[QUEX_TKN_MAY_JOIN - TOKEN_ID_OFFSET] = MAY_JOIN;
		type_id_table[QUEX_TKN_WHITESPACE - TOKEN_ID_OFFSET] = WHITESPACE;
		type_id_table[QUEX_TKN_LINE_BREAK - TOKEN_ID_OFFSET] = LINE_BREAK;
		type_id_table[QUEX_TKN_PARAGRAPH_BREAK - TOKEN_ID_OFFSET] = PARAGRAPH_BREAK;
	}

	virtual void reset(std::istream *in, char const *encoding, bool byte_order_reversion) {
		lexer = quex::RoughLexer(in, encoding, byte_order_reversion);
		token_p = 0x0;
	}

	virtual rough_token_t receive() {
		lexer.receive(&token_p);
		rough_token_t out_token;
		if (token_p->type_id() == QUEX_TKN_TERMINATION)
			out_token.type_id = TERMINATION;
		else
			out_token.type_id = type_id_table[token_p->type_id() - TOKEN_ID_OFFSET];
		out_token.text = token_p->pretty_char_text();
		return out_token;
	}
private:
	quex::RoughLexer lexer;
	quex::Token *token_p;
	rough_token_id type_id_table[7];
};

/* A factory function which we will retrieve by way of dlopen() and family
 * and use it to construct an instance of the wrapper class. */
IRoughLexerWrapper* make_quex_wrapper(std::istream *in, char const *encoding = 0x0, bool byte_order_reversion = false) {
	return new RoughLexerWrapper(in, encoding, byte_order_reversion);
}
