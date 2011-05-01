#include <iostream>
#include <string>
#include <vector>

#include "Classifier.hpp"
#include "token_t.hpp"
#include "utils.hpp"
#include "alignment_exception.hpp"

namespace trtok {

bool Classifier::consume_whitespace() {
  bool line_break = false;
  do
  {
    if (annot_char == 0x0A)
      line_break = true;
    annot_char = get_unicode_from_utf8(m_annot_stream_p);
  } while (is_whitespace(annot_char) && (m_annot_stream_p->gcount() > 0));
  return line_break;
}

void Classifier::report_alignment_warning(std::string occurence_type,
    std::string prefix, std::string suffix, std::string advice) {
  std::cerr << "Warning: Unexpected " << occurence_type
    << " encountered in annotated data." << std::endl;
  std::cerr << "         Prefix=" << prefix;
  if (suffix != "")
    std::cerr << " Suffix=" << suffix;
  std::cerr << std::endl;
  std::cerr << "         " << advice << std::endl;
}

void* Classifier::operator()(void* input_p) {
  chunk_t* chunk_p = (chunk_t*)input_p;

  // We clean up any leading whitespace from the text
  if (first_chunk)
  {
    consume_whitespace();
    first_chunk = false;
  }

  for (std::vector<token_t>::iterator token = chunk_p->tokens.begin();
   token != chunk_p->tokens.end(); token++)
  {
    std::basic_string<uint32_t> token_text = utf8_to_unicode(token->text);

    for (size_t i = 0; i != token_text.length(); i++)
    {
      if (m_annot_stream_p->gcount() == 0)
        throw alignment_exception("Annotated data truncated!");
      // An unexpected word break
      if (is_whitespace(annot_char))
      {
        bool line_break = consume_whitespace();

        report_alignment_warning("word break",
          unicode_to_utf8(token_text.substr(0, i)),
          unicode_to_utf8(token_text.substr(i)),
          "Consider adding a tokenization rule to a *.split file.");

        if (line_break)
          report_alignment_warning("sentence break",
            unicode_to_utf8(token_text.substr(0, i)),
            unicode_to_utf8(token_text.substr(i)),
            "Consider adding more sentence terminators or starters.");
      }
      // Different text in the annotated data
      if (annot_char != token_text[i])
      {
        // TODO: Possibly better error reporting.
        throw alignment_exception("Annotated data mismatch!");
      }
      annot_char = get_unicode_from_utf8(m_annot_stream_p);
    } // for (size_t i = 0; i != token_text.length(); i++)

    // Check for the presence of whitespace/newlines between this and
    // the next token
    if ((token + 1 != chunk_p->tokens.end()) || !chunk_p->is_final) {
      if (is_whitespace(annot_char)) {
        bool line_break = consume_whitespace();

        if (token->n_newlines == -1)
          if (token->decision_flags & MAY_SPLIT_FLAG)
            token->decision_flags = (decision_flags_t)
                (token->decision_flags | DO_SPLIT_FLAG);
          else
            report_alignment_warning("word break",
              token->text,
              (token + 1 != chunk_p->tokens.end()) ? (token + 1)->text : "",
              "Consider adding a tokenization rule to a *.split file.");
        if (line_break)
          if (token->decision_flags & MAY_BREAK_SENTENCE_FLAG)
            token->decision_flags = (decision_flags_t)
                (token->decision_flags | DO_BREAK_SENTENCE_FLAG);
          else
            report_alignment_warning("sentence break",
              token->text,
              (token + 1 != chunk_p->tokens.end()) ? (token + 1)->text : "",
              "Consider adding more sentence terminators or starters.");
      } else {
        if (token->n_newlines >= 0)
          if (token->decision_flags & MAY_JOIN_FLAG)
            token->decision_flags = (decision_flags_t)
              (token->decision_flags | DO_JOIN_FLAG);
          else
            report_alignment_warning("joining of words",
              token->text,
              (token + 1 != chunk_p->tokens.end()) ? (token + 1)->text : "",
              "Consider adding a tokenization rule to a *.join file.");
      }
    }
  } // for (std::vector<token_t>::iterator token = chunk_p->tokens.begin();...
  
  if (is_whitespace(annot_char)) {
    consume_whitespace();
  }
  if (chunk_p->is_final && !m_annot_stream_p->eof()) {
    std::cerr << "Warning: Extra text at the end of annotated data.\n";
  }

  return chunk_p;
}

}
