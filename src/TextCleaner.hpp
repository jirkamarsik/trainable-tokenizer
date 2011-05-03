#ifndef TEXT_CLEANER_INCLUDE_GUARD
#define TEXT_CLEANER_INCLUDE_GUARD

#include <string>
#include <istream>
#include <boost/unordered_map.hpp>
#include <tbb/concurrent_queue.h>
#include <boost/cstdint.hpp>
typedef boost::uint32_t uint32_t;

#include "pipes/pipe.hpp"

#include "cutout_t.hpp"

namespace trtok {

/* The TextCleaner class will be responsible for decoding the input text,
 * stripping off the XML markup and expanding the entities. The class sends
 * the unicode text encoded in UTF-8 to an ostream specified during construction
 * and posts the changes done to the input to a cutout queue so that the XML
 * and entities may be reconstructed on output. */
class TextCleaner
{
public:
  TextCleaner(pipes::opipestream *output_stream_p,
              std::string const &input_encoding,
              bool hide_xml,
              bool expand_entities,
              bool keep_entities_expanded = false,
              tbb::concurrent_bounded_queue<cutout_t> *cutout_queue_p = NULL):
    m_output_stream_p(output_stream_p),
    m_input_encoding(input_encoding),
    m_hide_xml(hide_xml),
    m_expand_entities(expand_entities),
    m_keep_entities_expanded(keep_entities_expanded),
    m_cutout_queue_p(cutout_queue_p)
  {
    if (m_expand_entities)
      prepare_entity_map();
  }

  void setup(std::istream *input_stream_p) {
    m_input_stream_p = input_stream_p;
  }

  void do_work();

private:
  void prepare_entity_map();
  bool expand_entity(std::string const &entity, uint32_t &expanded_str);

  std::istream *m_input_stream_p;
  pipes::opipestream *m_output_stream_p;
  std::string m_input_encoding;
  bool m_expand_entities, m_keep_entities_expanded, m_hide_xml;
  tbb::concurrent_bounded_queue<cutout_t> *m_cutout_queue_p;
  boost::unordered_map<std::string, uint32_t> m_entity_map;
};
}

#endif
