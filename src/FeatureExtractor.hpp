#ifndef FEATURE_EXTRACTOR_INCLUDE_GUARD
#define FEATURE_EXTRACTOR_INCLUDE_GUARD

#include <vector>
#include <string>
#include <map>
#include <pcrecpp.h>
#include "tbb/pipeline.h"

namespace trtok {

/* The FeatureExtractor represents a part of the pipeline which examines
   the textual content of each token and tests it for the user-defined
   predicate properties. */
class FeatureExtractor: public tbb::filter {

public:
    FeatureExtractor(/* The number of predicate properties total. */
                     int n_properties,
                     /* A list of the regular expression predicates. */
                     std::vector<pcrecpp::RE> regex_properties,
                     /* A BST which gives a list of enumerated predicates for
                        a word. */
                     std::multimap<std::string, int> word_to_enum_props):
        tbb::filter(tbb::filter::parallel),
        m_n_properties(n_properties),
        m_regex_properties(regex_properties),
        m_word_to_enum_props(word_to_enum_props)
        {}
    
    void reset() {}

    // The invoke opearator tests the text of every token against the regular
    // expressions and the BST and checks the token's property_flags
    // appropriately.
    virtual void* operator()(void *input_p);

private:
    int m_n_properties;
    std::vector<pcrecpp::RE> m_regex_properties;
    std::multimap<std::string, int> m_word_to_enum_props;
};

}

#endif
