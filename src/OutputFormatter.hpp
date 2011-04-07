#ifndef OUTPUTFORMATTER_INCLUDE_GUARD
#define OUTPUTFORMATTER_INCLUDE_GUARD

#include <ostream>
#include "tbb/pipeline.h"
#include "tbb/concurrent_queue.h"

#include "cutout_t.hpp"

namespace trtok {

//test
class OutputFormatter: public tbb::filter {
public:
	OutputFormatter(std::ostream *output_stream_p, bool detokenize,
			bool preserve_segments, bool preserve_paragraphs,
			tbb::concurrent_bounded_queue<cutout_t> *cutout_queue_p):
			tbb::filter(tbb::filter::serial_in_order),
			m_output_stream_p(output_stream_p), m_detokenize(detokenize),
			m_preserve_segments(preserve_segments),
			m_preserve_paragraphs(preserve_paragraphs),
			m_cutout_queue_p(cutout_queue_p)
	{
		reset();
	}
	
	void reset() {
		m_position = 0;
		m_have_cutout = false;
	}

	virtual void* operator()(void *input_p);
private:
	std::ostream *m_output_stream_p;
	tbb::concurrent_bounded_queue<cutout_t> *m_cutout_queue_p;
	bool m_detokenize, m_preserve_segments, m_preserve_paragraphs;
	long m_position;
	bool m_have_cutout;
	cutout_t m_last_cutout;
};

}

#endif
