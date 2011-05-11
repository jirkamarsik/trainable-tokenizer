#ifndef OUTPUTFORMATTER_INCLUDE_GUARD
#define OUTPUTFORMATTER_INCLUDE_GUARD

#include "tbb/pipeline.h"
#include "tbb/concurrent_queue.h"

#include "pipes/pipe.hpp"

#include "cutout_t.hpp"

namespace trtok {

//test
class OutputFormatter: public tbb::filter {

public:
    OutputFormatter(/* the pipestream to which the output is written,
                       it is closed, and so signals an EOF, after processing
                       the final chunk */
                    pipes::opipestream *output_stream_p,
                    /* whether tokenization decisions (JOIN, SPLIT) are to
                       be ignored */
                    bool detokenize,
                    /* whether segmentation decisions (BREAK_SENTENCE) are to
                       be ignored */
                    bool preserve_segments,
                    /* whethre paragraph breaks are to be preserved in the
                       output */
                    bool preserve_paragraphs,
                    /* a queue of cutout performed by the TextCleaner which
                       are to be undone by reinserting or replacing
                       characters */
                    tbb::concurrent_bounded_queue<cutout_t> *cutout_queue_p):
            tbb::filter(tbb::filter::serial_in_order),
            m_output_stream_p(output_stream_p),
            m_detokenize(detokenize),
            m_preserve_segments(preserve_segments),
            m_preserve_paragraphs(preserve_paragraphs),
            m_cutout_queue_p(cutout_queue_p)
    {
        reset();
    }
    
    // reset prepares the OutputFormatter for processing a different file
    void reset() {
        m_position = 0;
        m_have_cutout = false;
    }

    // the invoke operator takes a chunk pointer and sends its contents
    // along with the mandated whitespace down the output stream
    virtual void* operator()(void *input_p);

private:
    // Configuration
    pipes::opipestream *m_output_stream_p;
    tbb::concurrent_bounded_queue<cutout_t> *m_cutout_queue_p;
    bool m_detokenize, m_preserve_segments, m_preserve_paragraphs;

    // State
    long m_position;
    bool m_have_cutout;
    cutout_t m_last_cutout;
};

}

#endif
