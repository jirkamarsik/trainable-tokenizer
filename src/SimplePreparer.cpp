#include <vector>

#include "SimplePreparer.hpp"
#include "token_t.hpp"

using namespace std;

namespace trtok {

void* SimplePreparer::operator()(void* input_p) {
  chunk_t* chunk_p = (chunk_t*)input_p;

  for (vector<token_t>::iterator token = chunk_p->tokens.begin();
       token != chunk_p->tokens.end(); token++) {
    if (token->decision_flags & MAY_SPLIT_FLAG) {
      token->decision_flags = (decision_flags_t)
            (token->decision_flags | DO_SPLIT_FLAG);
    }
    if (token->decision_flags & MAY_BREAK_SENTENCE_FLAG) {
      token->decision_flags = (decision_flags_t)
            (token->decision_flags | DO_BREAK_SENTENCE_FLAG);
    }
  }

  return chunk_p;
}

}
