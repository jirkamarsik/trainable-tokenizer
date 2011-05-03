#include <vector>
#include <boost/cstdint.hpp>
typedef boost::uint8_t uint8_t;

#include "OutputFormatter.hpp"
#include "token_t.hpp"
#include "cutout_t.hpp"

namespace trtok {

void* OutputFormatter::operator() (void *input_p) {
  if (!m_have_cutout) {
    m_cutout_queue_p->pop(m_last_cutout);
    m_have_cutout = true;
  }

  chunk_t* chunk_p = (chunk_t*)input_p;

  typedef std::vector<token_t>::const_iterator token_iter;
  for (token_iter token = chunk_p->tokens.begin();
       token != chunk_p->tokens.end(); token++) {
    
    bool replacing_entity = false;
    typedef std::string::const_iterator char_iter;
    for (char_iter ch = token->text.begin(); ch != token->text.end(); ch++) {
      // Chars are signed, so we cast them to uint8_t for our purposes
      uint8_t uchar = (uint8_t)(*ch);
      if (uchar >> 6 == 2) {
        // A continuation byte in UTF-8.
        if (!replacing_entity) {
          // Write the character, but only if it won't
          // be replaced by an entity
          *m_output_stream_p << *ch;
        }
        continue;
      }
      replacing_entity = false;
      while (m_last_cutout.position == m_position) {
        if (m_last_cutout.type == XML_CUTOUT) {
          *m_output_stream_p << m_last_cutout.text;
        }
        if (m_last_cutout.type == ENTITY_CUTOUT) {
          // We will be replacing this character with an entity.
          *m_output_stream_p << m_last_cutout.text;
          replacing_entity = true;
        }
        m_cutout_queue_p->pop(m_last_cutout);
      }
      if (!replacing_entity) {
        // Write the character, but only if it won't
        // be replaced by an entity
        *m_output_stream_p << *ch;
      }
      m_position++;
    }

    // If this token is followed by XML closing tags, we print them
    // alongside this token.
    while (m_last_cutout.position == m_position) {
      if (m_last_cutout.type == ENTITY_CUTOUT)
        // We leave entities to be replaced for the next token.
        break;
      if (m_last_cutout.type == XML_CUTOUT) {
        if (m_last_cutout.text[1] == '/')
          *m_output_stream_p << m_last_cutout.text;
        else
          // Opening tags are left for the next token.
          break;
      }
      m_cutout_queue_p->pop(m_last_cutout);
    }

    std::string token_sep = "";
    // Check if we should insert a paragraph break...
    if (m_preserve_paragraphs && (token->n_newlines >= 2)) {
      token_sep = "\n\n";
    }
    // or a simple line break...
    if (token_sep == "") {
      if (m_preserve_segments) {
        if (token->n_newlines >= 1)
          token_sep = "\n";
      } else if (token->decision_flags & DO_BREAK_SENTENCE_FLAG) {
        token_sep = "\n";
      }
    }
    // or only a space.
    if (token_sep == "") {
      if (m_detokenize) {
        if (token->n_newlines >= 0)
          token_sep = " ";
      } else if ((token->decision_flags & DO_SPLIT_FLAG)
               && token->n_newlines == -1) {
        token_sep = " ";
      } else if (!(token->decision_flags & DO_JOIN_FLAG)
                && token->n_newlines >= 0) {
        token_sep = " ";
      }
    }
    if (chunk_p->is_final && (token + 1 == chunk_p->tokens.end()))
      // We make sure there is a terminating newline at the end of output
      token_sep = "\n";
    *m_output_stream_p << token_sep;
  }

  if (chunk_p->is_final) {
    // If this is the final chunk, we send an EOF to the converter
    // so it can flush and stop.
    m_output_stream_p->close();
  }

  // TODO: This could be optimized by sending the pointer back to
  // the rough tokenizer which is responsible for repeatedly allocating
  // these chunks.
  delete chunk_p;

  return NULL;
}

}
