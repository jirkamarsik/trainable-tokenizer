#ifndef SIMPLEPREPARER_INCLUDE_GUARD
#define SIMPLEPREPARER_INCLUDE_GUARD

#include "tbb/pipeline.h"

namespace trtok {

class SimplePreparer: public tbb::filter {

public:
    SimplePreparer(): tbb::filter(tbb::filter::parallel) {}

    void reset() {}

    virtual void* operator()(void *input_p);
};

}

#endif
