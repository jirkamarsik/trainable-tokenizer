#ifndef ROUGH_TOKENIZER_INCLUDE_GUARD
#define ROUGH_TOKENIZER_INCLUDE_GUARD

#include <tbb/pipeline.h>

#include "roughtok/roughtok_wrapper.hpp"
#include "token_t.hpp"

namespace trtok {

/* RoughTokenizer connects the dynamically loaded rough tokenizer to the
   input stream. It then calls the loaded tokenizer, interprets its output
   and builds a stream of our rich token_t tokens which it bundles in
   a chunk to be processed in the rest of the pipeline.*/
class RoughTokenizer: public tbb::filter {

public:
    RoughTokenizer(/* The dynamically loaded rough tokenizer class.*/
                   IRoughLexerWrapper *wrapper_p ):
                tbb::filter(tbb::filter::serial_in_order),
                m_wrapper_p(wrapper_p),
                m_hit_end(false),
                m_first_chunk(true),
                m_time_spent(0)
    {}

    // setup prepares the RoughTokenizer to read from an encoded stream
    // and produce UTF-8 rough tokens.
    void setup(istream* in_p, char const *encoding) {
      m_wrapper_p->setup(in_p, encoding);
      m_first_chunk = true;
      m_hit_end = false;
    }

    // reset prepares the RoughTokenizer to process another file.
    void reset() {
      m_wrapper_p->reset();
      m_first_chunk = true;
      m_hit_end = false;
    }

    // The invoke operator repeatedly calls receive on the in the loaded
    // tokenizer until it gets a decent amount of tokens (constant CHUNK_SIZE
    // specified during compilation) and then sends them down the pipeline
    // as a chunk_t.
    virtual void* operator()(void*);

    double m_time_spent;

private:
    // Configuration
    IRoughLexerWrapper *m_wrapper_p;

    // State
    bool m_first_chunk;
    bool m_hit_end;
    rough_token_t m_last_rough_tok;
};

}

#endif
