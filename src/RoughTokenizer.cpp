#include <tbb/pipeline.h>
#include <cassert>

#include "RoughTokenizer.hpp"
#include "configuration.hpp"
#include "roughtok/roughtok_wrapper.hpp"
#include "token_t.hpp"

#include <tbb/tick_count.h>

namespace trtok {

void* RoughTokenizer::operator()(void*) {
  tbb::tick_count start_time = tbb::tick_count::now();

  if (m_hit_end) {
    // Returing NULL signifies the end of processing to TBB
    return NULL;
  }

  // TODO: This repeated allocation could be diminished by using some sort
  // of thread safe memory pool, like a tbb::concurrent_vector<chunk_t*>
  chunk_t *chunk_p = new chunk_t;
  
  size_t n_tokens = 0;

  if (m_first_chunk) {
    // This is our first go at the input
    // We screen out all the non-textual or blank tokens and
    // find the first token piece
    do {
      m_last_rough_tok = m_wrapper_p->receive();
    } while ((m_last_rough_tok.type_id != TOKEN_PIECE_ID)
          && (m_last_rough_tok.type_id != TERMINATION_ID));
    if (m_last_rough_tok.type_id != TERMINATION_ID) {
      m_last_rough_tok.text = m_last_rough_tok.text;
    }
    m_first_chunk = false;
  }

  // Pre-condition: m_last_rough_tok.text contains a non-empty string with
  // the text of the next token to be placed in the chunk
  while ((n_tokens < CHUNK_SIZE)
   && (m_last_rough_tok.type_id != TERMINATION_ID)) {

    chunk_p->tokens.push_back(token_t());
    token_t &cur_token = chunk_p->tokens[n_tokens];
    cur_token.text = m_last_rough_tok.text;

    m_last_rough_tok = m_wrapper_p->receive();

    while ((m_last_rough_tok.type_id != TOKEN_PIECE_ID)
        && (m_last_rough_tok.type_id != TERMINATION_ID)) {

      switch (m_last_rough_tok.type_id) {
        case MAY_SPLIT_ID:
          cur_token.decision_flags = (decision_flags_t)
                (cur_token.decision_flags | MAY_SPLIT_FLAG);
          break;
        case MAY_JOIN_ID:
          cur_token.decision_flags = (decision_flags_t)
                (cur_token.decision_flags | MAY_JOIN_FLAG);
          break;
        case MAY_BREAK_SENTENCE_ID:
          cur_token.decision_flags = (decision_flags_t)
                (cur_token.decision_flags | MAY_BREAK_SENTENCE_FLAG);
          break;
        case WHITESPACE_ID:
          cur_token.n_newlines = m_last_rough_tok.n_newlines;
          break;
      }

      m_last_rough_tok = m_wrapper_p->receive();
    }

    n_tokens++;
  }

  if (m_last_rough_tok.type_id == TERMINATION_ID) {
    m_hit_end = true;
  }
  
  chunk_p->is_final = m_hit_end;

  m_time_spent += (tbb::tick_count::now() - start_time).seconds();

  return chunk_p;
}

}
