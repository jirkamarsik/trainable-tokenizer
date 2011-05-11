#ifndef ENCODER_INCLUDE_GUARD
#define ENCODER_INCLUDE_GUARD

#include <string>
#include <istream>
#include <ostream>

namespace trtok {

class Encoder {

public:
    Encoder(std::istream *input_stream_p /* The UTF-8 stream of text.*/,
            std::string output_encoding /* The name of the target encoding.*/):
        m_input_stream_p(input_stream_p),
        m_output_encoding(output_encoding)
    {}
    
    // setup is called in between input files to switch the output file.
    void setup(std::ostream *output_stream_p)
    {
        m_output_stream_p = output_stream_p;
    }

    // do_work reads all the UTF-8 text from the input stream and writes it
    // to the target stream in the desired encoding.
    void do_work();

private:
    std::istream* m_input_stream_p;
    std::ostream* m_output_stream_p;
    std::string m_output_encoding;
};

}

#endif
