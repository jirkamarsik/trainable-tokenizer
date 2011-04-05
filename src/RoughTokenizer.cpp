#include <tbb/pipeline.h>
#include <cassert>

#include "RoughTokenizer.hpp"
#include "rough_tok/rough_tok_wrapper.hpp"
#include "token_t.hpp"

namespace trtok {

void* RoughTokenizer::operator()(void*) {
	if (m_hit_end) {
		// Returing NULL signifies the end of the input sequence to TBB
		return NULL;
	}

	// TODO: This repeated allocation could be diminished by using some sort
	// of thread safe memory pool, like a tbb::concurrent_vector<chunk_t*>
	chunk_t *chunk_p = new chunk_t;
	
	size_t n_tokens = 0;
	rough_token_t rough_tok;

	if (m_last_tok_piece == "") {
		// This is our first go at the input
		// We screen out all the non-textual or blank tokens and
		// find the first token piece
		do {
			rough_tok = m_wrapper_p->receive();
		} while ((rough_tok.type_id != TOKEN_PIECE_ID) && (rough_tok.type_id != TERMINATION_ID));
		if (rough_tok.type_id == TERMINATION_ID) {
			// The input was empty
			return NULL;
		}
		else {
			m_last_tok_piece = rough_tok.text;
		}
	}

	// Pre-condition: m_last_tok_piece contains a non-empty string with
	// the text of the next token to be placed in the chunk
	do {
		chunk_p->tokens.push_back(token_t());
		token_t *cur_token = &chunk_p->tokens[n_tokens];
		cur_token->text = m_last_tok_piece;
		rough_tok = m_wrapper_p->receive();
		while ((rough_tok.type_id != TOKEN_PIECE_ID) && (rough_tok.type_id != TERMINATION_ID)) {
			switch (rough_tok.type_id) {
				case MAY_SPLIT_ID:
					cur_token->decision_flags = (decision_flags_t)(cur_token->decision_flags | MAY_SPLIT_FLAG);
					break;
				case MAY_JOIN_ID:
					cur_token->decision_flags = (decision_flags_t)(cur_token->decision_flags | MAY_JOIN_FLAG);
					break;
				case MAY_BREAK_SENTENCE_ID:
					cur_token->decision_flags = (decision_flags_t)(cur_token->decision_flags | MAY_BREAK_SENTENCE_FLAG);
					break;
				case WHITESPACE_ID:
					assert(cur_token->n_newlines == -1);
					cur_token->n_newlines = 0;
					break;
				case LINE_BREAK_ID:
					assert(cur_token->n_newlines == -1);
					cur_token->n_newlines = 1;
					break;
				case PARAGRAPH_BREAK_ID:
					assert(cur_token->n_newlines == -1);
					cur_token->n_newlines = 2;
					break;
			}
			rough_tok = m_wrapper_p->receive();
		}
		n_tokens++;
		if (rough_tok.type_id == TOKEN_PIECE_ID) {
			m_last_tok_piece = rough_tok.text;
		}
	} while ((n_tokens < CHUNK_SIZE) && (rough_tok.type_id != TERMINATION_ID));

	if (rough_tok.type_id == TERMINATION_ID) {
		m_hit_end = true;
	}
	
	chunk_p->is_final = m_hit_end;
	return chunk_p;
}

}
