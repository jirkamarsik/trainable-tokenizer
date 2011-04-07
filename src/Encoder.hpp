#ifndef ENCODER_INCLUDE_GUARD
#define ENCODER_INCLUDE_GUARD

#include <string>
#include <istream>
#include <ostream>

namespace trtok {

class Encoder {
public:
	Encoder(std::istream *input_stream_p, std::string output_encoding):
		m_input_stream_p(input_stream_p), m_output_encoding(output_encoding) {}
	
	void setup(std::ostream *output_stream_p)
	{
		m_output_stream_p = output_stream_p;
	}

	void do_work();
private:
	std::istream* m_input_stream_p;
	std::ostream* m_output_stream_p;
	std::string m_output_encoding;
};

}

#endif
