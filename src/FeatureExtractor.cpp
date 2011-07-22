#include <vector>

#include "FeatureExtractor.hpp"
#include "token_t.hpp"

#include <tbb/tick_count.h>

namespace trtok {

void* FeatureExtractor::operator()(void *input_p) {
    tbb::tick_count start_time = tbb::tick_count::now();

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
      iter_pair list_props = m_word_to_list_props.equal_range(token->text);
      for (iter i = list_props.first; i != list_props.second; i++) {
        token->property_flags[i->second] = true;
      }
    }

    m_time_spent += (tbb::tick_count::now() - start_time).seconds();

    return chunk_p;
}

}
