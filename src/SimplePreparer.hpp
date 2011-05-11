#ifndef SIMPLEPREPARER_INCLUDE_GUARD
#define SIMPLEPREPARER_INCLUDE_GUARD

#include "tbb/pipeline.h"

namespace trtok {

/* SimplePreparer is a stand-in for FeatureExtractor and Classifier used
   in situations when all we want to do is just split the text into rough
   tokens and candidate segments for manual annotation. */
class SimplePreparer: public tbb::filter {

public:
    SimplePreparer(): tbb::filter(tbb::filter::parallel) {}

    void reset() {}

    // This checks DO_SPLIT and DO_BREAK_SENTENCE on tokens with MAY_SPLIT
    // and MAY_BREAK_SENTENCE respectively.
    virtual void* operator()(void *input_p);
};

}

#endif
