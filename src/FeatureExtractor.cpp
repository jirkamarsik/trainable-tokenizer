#include <vector>

#include "FeatureExtractor.hpp"
#include "token_t.hpp"

namespace trtok {

void* FeatureExtractor::operator()(void *input_p) {

    chunk_t *chunk_p = (chunk_t*)input_p;

    for (vector<token_t>::iterator token = chunk_p->tokens.begin();
         token != chunk_p->tokens.end(); token++) {

      token->property_flags = vector<bool>(m_n_properties);

      for (int i = 0; i < m_regex_properties.size(); i++) {
        token->property_flags[i] =
              m_regex_properties[i].FullMatch(token->text);
      }

      typedef multimap<string, int>::const_iterator iter;
      typedef pair<iter, iter> iter_pair;
      iter_pair enum_props = m_word_to_enum_props.equal_range(token->text);
      for (iter i = enum_props.first; i != enum_props.second; i++) {
        token->property_flags[i->second] = true;
      }
    }

    return chunk_p;
}

}
