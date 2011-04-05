#include <iostream>
#include <string>
#include <tbb/pipeline.h>

#include "rough_tok/rough_tok_wrapper.hpp"
#include "token_t.hpp"

namespace trtok {

class RoughTokenizer: public tbb::filter {
public:
	RoughTokenizer(IRoughLexerWrapper *wrapper_p): tbb::filter(tbb::filter::serial_in_order),
		m_wrapper_p(wrapper_p), m_hit_end(false), m_last_tok_piece("") {}

	void setup(std::istream* in_p, char const *encoding) {
		m_wrapper_p->setup(in_p, encoding);
		m_hit_end = false;
	}

	void reset() {
		m_wrapper_p->reset();
		m_hit_end = false;
	}

	virtual void* operator()(void*);
private:
	IRoughLexerWrapper *m_wrapper_p;
	bool m_hit_end;
	// We store the text of the last token piece which didn't fit
	// in the chunk
	std::string m_last_tok_piece;
};

}
