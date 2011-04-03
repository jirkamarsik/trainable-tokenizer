#ifndef TOKEN_T_INCLUDE_GUARD
#define TOKEN_T_INCLUDE_GUARD

#include <string>
#include <vector>
#include <boost/dynamic_bitset.hpp>

#include "configuration.hpp"

namespace trtok {

enum decision_flags_t {
	NO_FLAG = 0,
	MAY_SPLIT_FLAG = 1,
	DO_SPLIT_FLAG = 2,
	MAY_JOIN_FLAG = 4,
	DO_JOIN_FLAG = 8,
	MAY_BREAK_SENTENCE_FLAG = 16,
	DO_BREAK_SENTENCE_FLAG = 32
};

struct token_t {
	token_t(): decision_flags(NO_FLAG), n_newlines(-1) {}

	//The text of the token.
	std::string text;
	//Whether there are MAY_SPLIT, MAY_JOIN or MAY_BREAK_SENTENCE
	//decision points between this token and the next one and
	//what is their outcome.
	decision_flags_t decision_flags;
	//Number of newlines between this token and the next one;
	//-1 signifies no whitespace following this token at all
	int n_newlines;
	//Which user-defined properties hold for this token.
	std::vector<bool> property_flags;
};

struct chunk_t {
	chunk_t(): is_final(false), tokens() {}

	//Is this the last chunk of tokens?
	bool is_final;
	std::vector<token_t> tokens;
};

}
#endif
