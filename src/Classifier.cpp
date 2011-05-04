#include <iostream>
#include <string>
#include <vector>
#include <boost/lexical_cast.hpp>

#include "Classifier.hpp"
#include "token_t.hpp"
#include "utils.hpp"
#include "alignment_exception.hpp"

#define WINDOW_OFFSET(offset) ((m_center_token + offset < 0) ?\
(((m_center_token + offset) % m_window_size) + m_window_size) :\
((m_center_token + offset) % m_window_size))

#define CHECK_DECISION_FLAG(flag) {\
  if (questioned_token.decision_flags & flag##_FLAG) {\
    context.push_back(std::make_pair(offset_str + "%" #flag, 1.0));\
  }\
}

#define FEATURES_MAP(offset, property)\
  m_features_map[offset * m_window_size + property]

namespace trtok {

bool Classifier::consume_whitespace() {
  bool line_break = false;
  do
  {
    if (m_annot_char == 0x0A)
      line_break = true;
    m_annot_char = get_unicode_from_utf8(m_annot_stream_p);
  } while (is_whitespace(m_annot_char) && (m_annot_stream_p->gcount() > 0));
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

void Classifier::process_center_token(chunk_t *out_chunk_p) {

  token_t &center_token = m_window[m_center_token];

  if (center_token.text == "") {
    return;
  }

  if ((center_token.decision_flags & MAY_SPLIT_FLAG)
   || (center_token.decision_flags & MAY_JOIN_FLAG)
   || (center_token.decision_flags & MAY_BREAK_SENTENCE_FLAG)) {

    int n_defined_properties = center_token.property_flags.size();
    int length_property = n_defined_properties;
    int word_property = n_defined_properties + 1;

    std::vector< std::pair<std::string,float> > context;
    
    // Simple features
    for (int offset = -m_precontext; offset != m_postcontext + 1; offset++) {
      
      int window_offset = WINDOW_OFFSET(offset);
      std::string offset_str = boost::lexical_cast<std::string>(offset) + ":";
      token_t &questioned_token = m_window[window_offset];

      // end of input marks
      if (questioned_token.text == "") {
        context.push_back(std::make_pair(
              offset_str + "%END_OF_INPUT",
              1.0));
        continue;
      }
      
      // decision points
      CHECK_DECISION_FLAG(MAY_SPLIT);
      CHECK_DECISION_FLAG(MAY_JOIN);
      CHECK_DECISION_FLAG(MAY_BREAK_SENTENCE);

      // we want to fill out only previous decisions in the context,
      // as will be the case with real questions
      if (offset < 0) {
        CHECK_DECISION_FLAG(DO_SPLIT);
        CHECK_DECISION_FLAG(DO_JOIN);
        CHECK_DECISION_FLAG(DO_BREAK_SENTENCE);
      }

      // whitespace features
      if (center_token.n_newlines >= 0) {
        context.push_back(std::make_pair(
              offset_str + "%WHITESPACE",
              1.0));
      }
      if (center_token.n_newlines >= 1) {
        context.push_back(std::make_pair(
              offset_str + "%LINE_BREAK",
              1.0));
      }
      if (center_token.n_newlines >= 2) {
        context.push_back(std::make_pair(
              offset_str + "%PARAGRAPH_BREAK",
              1.0));
      }

      // user-defined features
      for (int property = 0; property != n_defined_properties; property++) {
        if (FEATURES_MAP(offset + m_precontext, property)) {
          context.push_back(std::make_pair(
              offset_str + m_property_names[property],
              questioned_token.property_flags[property] ? 1.0 : 0.0));
        }
      } 

      // special features
      if (FEATURES_MAP(offset + m_precontext, length_property)) {
        context.push_back(std::make_pair(
              offset_str + m_property_names[length_property],
              questioned_token.text.length()));
      }
      if (FEATURES_MAP(offset + m_precontext, word_property)) {
        context.push_back(std::make_pair(
              offset_str + m_property_names[word_property]
              + "=" + questioned_token.text,
              1.0));
      }
    }

    // combined features
    for (std::vector< std::vector< std::pair<int,int> > >::const_iterator
         combined_feature = m_combined_features.begin();
         combined_feature != m_combined_features.end();
         combined_feature++) {
      
      std::string feature_string = "";
      bool crossed_end_of_input = false;

      for (std::vector< std::pair<int,int> >::const_iterator
           constituent_feature = combined_feature->begin();
           constituent_feature != combined_feature->end();
           constituent_feature++) {

        int offset = constituent_feature->first;
        int window_offset = WINDOW_OFFSET(offset);
        token_t &questioned_token = m_window[window_offset];
        std::string offset_str = boost::lexical_cast<std::string>(offset);

        if (questioned_token.text == "") {
          crossed_end_of_input = true;
          break;
        }
        
        int property = constituent_feature->second;
        std::string property_name = m_property_names[property];
        std::string single_feature_string = offset_str + ":"
                                          + property_name + "=";

        if (property < n_defined_properties) {
          single_feature_string +=
              questioned_token.property_flags[property] ? "1.0" : "0.0";
        } else if (property == length_property) {
          single_feature_string +=
              boost::lexical_cast<std::string>(questioned_token.text.length());
        } else if (property == word_property) {
          single_feature_string +=
              questioned_token.text;
        }

        if (feature_string != "") {
          feature_string += "^";
        }
        feature_string += single_feature_string;
      }

      if (!crossed_end_of_input) {
        context.push_back(std::make_pair(feature_string, 1.0));
      }
    }

    std::string true_outcome;
    std::string predicted_outcome;

    if ((m_mode == "train") || (m_mode == "evaluate")) {
      if (center_token.decision_flags & DO_BREAK_SENTENCE_FLAG) {
        true_outcome = "BREAK_SENTENCE";
      } else if ((center_token.decision_flags & DO_SPLIT_FLAG)
             || ((center_token.n_newlines >= 0)
                && !(center_token.decision_flags & DO_JOIN_FLAG))) {
        true_outcome = "SPLIT";
      }
      else {
        true_outcome = "JOIN";
      }
    }

    if ((m_mode == "tokenize") || (m_mode == "evaluate")) {
      predicted_outcome = m_model.predict(context);
    }

    if (m_mode == "prepare") {
      if (center_token.decision_flags & MAY_SPLIT_FLAG) {
        center_token.decision_flags = (decision_flags_t)
              (center_token.decision_flags | DO_SPLIT_FLAG);
      }
      if (center_token.decision_flags & MAY_BREAK_SENTENCE_FLAG) {
        center_token.decision_flags = (decision_flags_t)
              (center_token.decision_flags | DO_BREAK_SENTENCE_FLAG);
      }
    }
    if (m_mode == "train") {
      m_model.add_event(context, true_outcome);
      m_n_events_registered++;
    }
    else if (m_mode == "tokenize") {
      if ((predicted_outcome == "BREAK_SENTENCE")
       && (center_token.decision_flags & MAY_BREAK_SENTENCE_FLAG)) {
        center_token.decision_flags = (decision_flags_t)
              (center_token.decision_flags | DO_BREAK_SENTENCE_FLAG);
      }
      if (((predicted_outcome == "BREAK_SENTENCE")
            || (predicted_outcome == "SPLIT"))
       && (center_token.decision_flags & MAY_SPLIT_FLAG)) {
        center_token.decision_flags = (decision_flags_t)
              (center_token.decision_flags | DO_SPLIT_FLAG);
      } 
      if ((predicted_outcome == "JOIN")
       && (center_token.decision_flags & MAY_JOIN_FLAG)) {
        center_token.decision_flags = (decision_flags_t)
              (center_token.decision_flags | DO_JOIN_FLAG);
      }
    }
  }

  if (out_chunk_p != NULL) {
    out_chunk_p->tokens.push_back(center_token);
  }
}

void Classifier::process_tokens(std::vector<token_t> &tokens,
                                chunk_t *out_chunk_p) {
  for (std::vector<token_t>::iterator token = tokens.begin();
       token != tokens.end(); token++) {
    m_center_token = WINDOW_OFFSET(1);
    m_window[WINDOW_OFFSET(m_postcontext)] = *token;
    process_center_token(out_chunk_p);
  }
}

void Classifier::align_chunk_with_solution(chunk_t *in_chunk_p) {
  // We clean up any leading whitespace from the text
  if (m_first_chunk)
  {
    consume_whitespace();
    m_first_chunk = false;
  }

  for (std::vector<token_t>::iterator token = in_chunk_p->tokens.begin();
   token != in_chunk_p->tokens.end(); token++)
  {
    std::basic_string<uint32_t> token_text = utf8_to_unicode(token->text);

    for (size_t i = 0; i != token_text.length(); i++)
    {
      if (m_annot_stream_p->gcount() == 0)
        throw alignment_exception("Annotated data truncated!");
      // An unexpected word break
      if (is_whitespace(m_annot_char))
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
      if (m_annot_char != token_text[i])
      {
        // TODO: Possibly better error reporting.
        throw alignment_exception("Annotated data mismatch!");
      }
      m_annot_char = get_unicode_from_utf8(m_annot_stream_p);
    } // for (size_t i = 0; i != token_text.length(); i++)

    // Check for the presence of whitespace/newlines between this and
    // the next token
    if ((token + 1 != in_chunk_p->tokens.end()) || !in_chunk_p->is_final) {
      if (is_whitespace(m_annot_char)) {
        bool line_break = consume_whitespace();

        if (token->n_newlines == -1)
          if (token->decision_flags & MAY_SPLIT_FLAG)
            token->decision_flags = (decision_flags_t)
                (token->decision_flags | DO_SPLIT_FLAG);
          else
            report_alignment_warning("word break",
              token->text,
              (token + 1 != in_chunk_p->tokens.end()) ? (token + 1)->text : "",
              "Consider adding a tokenization rule to a *.split file.");
        if (line_break)
          if (token->decision_flags & MAY_BREAK_SENTENCE_FLAG)
            token->decision_flags = (decision_flags_t)
                (token->decision_flags | DO_BREAK_SENTENCE_FLAG);
          else
            report_alignment_warning("sentence break",
              token->text,
              (token + 1 != in_chunk_p->tokens.end()) ? (token + 1)->text : "",
              "Consider adding more sentence terminators or starters.");
      } else {
        if (token->n_newlines >= 0)
          if (token->decision_flags & MAY_JOIN_FLAG)
            token->decision_flags = (decision_flags_t)
              (token->decision_flags | DO_JOIN_FLAG);
          else
            report_alignment_warning("joining of words",
              token->text,
              (token + 1 != in_chunk_p->tokens.end()) ? (token + 1)->text : "",
              "Consider adding a tokenization rule to a *.join file.");
      }
    }
  } // for (std::vector<token_t>::iterator token = in_chunk_p->tokens.begin();
  
  if (is_whitespace(m_annot_char)) {
    consume_whitespace();
  }
  if (in_chunk_p->is_final && !m_annot_stream_p->eof()) {
    std::cerr << "Warning: Extra text at the end of annotated data.\n";
  }
}

void* Classifier::operator()(void* input_p) {
  chunk_t* in_chunk_p = (chunk_t*)input_p;

  if ((m_mode == "train") || (m_mode == "evaluate")) {
    align_chunk_with_solution(in_chunk_p);
  }

  chunk_t *out_chunk_p = NULL;
  if ((m_mode == "tokenize") || (m_mode == "prepare")) {
    out_chunk_p = new chunk_t;
    out_chunk_p->is_final = in_chunk_p->is_final;
  }

  process_tokens(in_chunk_p->tokens, out_chunk_p);

  if (in_chunk_p->is_final) {
    token_t end_token;
    end_token.text = "";
    std::vector<token_t> end_tokens(m_postcontext + 1, end_token);
    process_tokens(end_tokens, out_chunk_p);
  }

  delete in_chunk_p;
  return out_chunk_p;
}

}
