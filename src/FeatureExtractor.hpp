#ifndef FEATURE_EXTRACTOR_INCLUDE_GUARD
#define FEATURE_EXTRACTOR_INCLUDE_GUARD

#include <vector>
#include <string>
#include <map>
#include <pcrecpp.h>
#include "tbb/pipeline.h"

namespace trtok {

class FeatureExtractor: public tbb::filter {
public:
	FeatureExtractor(int n_properties, std::vector<pcrecpp::RE> regex_properties,
			 std::multimap<std::string, int> word_to_enum_props):
		tbb::filter(tbb::filter::parallel),
		m_n_properties(n_properties), m_regex_properties(regex_properties),
		m_word_to_enum_props(word_to_enum_props) {}
	
	void reset() {}

	virtual void* operator()(void *input_p);
private:
	int m_n_properties;
	std::vector<pcrecpp::RE> m_regex_properties;
	std::multimap<std::string, int> m_word_to_enum_props;
};

}

#endif
