#ifndef TOKEN_T_INCLUDE_GUARD
#define TOKEN_T_INCLUDE_GUARD

#include <string>
#include <boost/dynamic_bitset.hpp>

enum decision_flags_t {
	MAY_SPLIT = 1;
	DO_SPLIT = 2;
	MAY_JOIN = 4;
	DO_JOIN = 8;
	MAY_BREAK_SENTENCE = 16;
	DO_BREAK_SENTENCE = 32;
}

struct token_t {
	std::string text;
	decision_flags_t decision_flags;
	int ws_newlines;
	boost::dynamic_bitset property_flags;
}

#endif
