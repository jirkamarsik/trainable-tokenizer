#include <iostream>
#include <vector>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/unordered_map.hpp>

#include <trtok_read_features_FeaturesReader>

#include <read_features_file.hpp>

using namespace std;

namespace trtok {

#define FEATURES_ERROR(message) {\
  cerr << features_file << ":" << features_token_p->line_number()\
<< ":" << features_token_p->column_number() << ": Error: "\
<< message << endl;\
  return 1;\
}

#define FEATURES_SYNTAX_ERROR(expected) {\
  cerr << features_file << ":" << features_token_p->line_number()\
<< ":" << features_token_p->column_number() << ": Error: "\
<< "Syntax error! Expected " << expected << "." << endl;\
  return 1;\
}

#define FEATURES_MASK(offset, property)\
  features_mask[(offset) * n_properties + property]

int read_features_file(
                  string const &features_file,
                  boost::unordered_map<string, int> const &prop_name_to_id,
                  int n_properties,
                  int n_basic_properties,
                  bool *&features_mask,
                  vector< vector< pair<int,int> > > &combined_features,
                  int &precontext,
                  int &postcontext) {

    read_features::FeaturesReader features_lex(features_file, "UTF-8");
    read_features::Token *features_token_p = 0x0;

    // A list of lines consisting of a set of offsets and a set of properties
    // desired of tokens at those offsets.
    vector< pair< vector<int>,vector<int> > > features;

    // Features are first read in to a structure which mirrors the syntax
    // of the file and after the file has been parsed and the size of the
    // context window determined, the parsed structure is interpreted and
    // the feature selection mask is filled out.

    try {
    typedef boost::unordered_map<std::string, int>::const_iterator lookup_iter;
    do {

      features_lex.receive(&features_token_p);

      // A regular line selecting common features for a set of offsets.
      // Example: -3,-1..2,4: number, %length, (uppercase ^ something_else)
      if (features_token_p->type_id() == QUEX_FEATURES_NUMBER) {

        //First, we parse the set of offsets.
        vector<int> offsets;

        do {
          int left = boost::lexical_cast<int>
                (features_token_p->pretty_char_text());

          features_lex.receive(&features_token_p);
          // An interval of offsets, ...
          if (features_token_p->type_id() == QUEX_FEATURES_DOUBLEDOT) {

            features_lex.receive(&features_token_p);
            if (features_token_p->type_id() != QUEX_FEATURES_NUMBER)
              FEATURES_SYNTAX_ERROR("a number");

            int right = boost::lexical_cast<int>
                  (features_token_p->pretty_char_text());

            if (right < left)
              FEATURES_ERROR("Interval upper bound lower than lower bound.");

            for (int offset = left; offset <= right; offset++) {
              offsets.push_back(offset);
            }
            features_lex.receive(&features_token_p);
          // a single offset...
          } else if ((features_token_p->type_id() == QUEX_FEATURES_COMMA)
                  || (features_token_p->type_id() == QUEX_FEATURES_COLON)) {
            offsets.push_back(left);
          // or a syntax error.
          } else FEATURES_SYNTAX_ERROR("a double-dot, a comma or a colon");

          if (features_token_p->type_id() == QUEX_FEATURES_COMMA) {
            features_lex.receive(&features_token_p);
          } else if (features_token_p->type_id() != QUEX_FEATURES_COLON)
            FEATURES_SYNTAX_ERROR("a comma or a colon");

        } while (features_token_p->type_id() == QUEX_FEATURES_NUMBER);

        features_lex.receive(&features_token_p);
        
        // Now we are past the colon and it is time to collect the property
        // names.
        vector<int> properties;

        while ((features_token_p->type_id() == QUEX_FEATURES_IDENTIFIER)
            || (features_token_p->type_id() == QUEX_FEATURES_LPAREN)
            || (features_token_p->type_id() == QUEX_FEATURES_STAR)) {
          
          // A star is a wildcard for all user-defined properties.
          if (features_token_p->type_id() == QUEX_FEATURES_STAR) {

            for (int property = 0;
                 property != n_basic_properties; property++) {
              properties.push_back(property);
            }

            features_lex.receive(&features_token_p);
            if (features_token_p->type_id() == QUEX_FEATURES_COMMA)
              features_lex.receive(&features_token_p);
            else if (features_token_p->type_id() != QUEX_FEATURES_SEMICOLON)
              FEATURES_SYNTAX_ERROR("a comma or a semicolon");

          // An identifier simply names a single property we are interested in.
          } else if (features_token_p->type_id() == QUEX_FEATURES_IDENTIFIER) {

            lookup_iter lookup =
              prop_name_to_id.find(features_token_p->pretty_char_text());
            if (lookup == prop_name_to_id.end())
              FEATURES_ERROR("Property \""
                << features_token_p->pretty_char_text() << "\" not defined.");
            int property = lookup->second;

            properties.push_back(property);

            features_lex.receive(&features_token_p);
            if (features_token_p->type_id() == QUEX_FEATURES_COMMA)
              features_lex.receive(&features_token_p);
            else if (features_token_p->type_id() != QUEX_FEATURES_SEMICOLON)
              FEATURES_SYNTAX_ERROR("a comma or a semicolon");

          // A list of hat-separated identifiers in parenthesis signals a
          // combined feature consisting of properties on a single token.
          // Such a combined feature is created for every offset presented
          // before the colon. These features are stored in the
          // combined_features vector.
          } else if (features_token_p->type_id() == QUEX_FEATURES_LPAREN) {

            features_lex.receive(&features_token_p);

            // Collect and identify the constituent properties.
            std::vector<int> constituent_properties;
            while (features_token_p->type_id() == QUEX_FEATURES_IDENTIFIER) {
              lookup_iter lookup = 
                prop_name_to_id.find(features_token_p->pretty_char_text());
              if (lookup == prop_name_to_id.end())
                FEATURES_ERROR("Property \""
                 << features_token_p->pretty_char_text() << "\" not defined.");
              int property = lookup->second;

              constituent_properties.push_back(property);

              features_lex.receive(&features_token_p);
              if (features_token_p->type_id() == QUEX_FEATURES_COMBINE) {
                features_lex.receive(&features_token_p);
                if (features_token_p->type_id() != QUEX_FEATURES_IDENTIFIER)
                  FEATURES_SYNTAX_ERROR("an identifier");
              }
              else if (features_token_p->type_id() != QUEX_FEATURES_RPAREN)
                FEATURES_SYNTAX_ERROR("a hat or a right parenthesis");
            }

            if (features_token_p->type_id() != QUEX_FEATURES_RPAREN)
              FEATURES_SYNTAX_ERROR("a hat or a right parenthesis");

            for (vector<int>::const_iterator offset = offsets.begin();
                 offset != offsets.end(); offset++) {
              // We instantiate the combined feature for a specific offset...
              vector< pair <int,int> > combined_feature;
              for (vector<int>::const_iterator property =
                   constituent_properties.begin();
                   property != constituent_properties.end(); property++) {
                combined_feature.push_back(make_pair(*offset, *property));
              }
              // and we store it in combined_features.
              combined_features.push_back(combined_feature);
            }

            features_lex.receive(&features_token_p);
            if (features_token_p->type_id() == QUEX_FEATURES_COMMA)
              features_lex.receive(&features_token_p);
            else if (features_token_p->type_id() != QUEX_FEATURES_SEMICOLON)
              FEATURES_SYNTAX_ERROR("a comma or a semicolon");
          }
        }

        // We have read the line and now we store its parsed form in the
        // features vector.
        features.push_back(std::make_pair(offsets, properties));

      // A line defining a single combined feature which might span tokens
      // at multiple offsets.
      } else if (features_token_p->type_id() == QUEX_FEATURES_LPAREN) {

        features_lex.receive(&features_token_p); 

        vector< pair <int,int> > combined_feature;

        while (features_token_p->type_id() == QUEX_FEATURES_NUMBER) {

            int offset = boost::lexical_cast<int>
                  (features_token_p->pretty_char_text());

            features_lex.receive(&features_token_p);
            if (features_token_p->type_id() != QUEX_FEATURES_COLON)
              FEATURES_SYNTAX_ERROR("a colon");

            features_lex.receive(&features_token_p);
            if (features_token_p->type_id() != QUEX_FEATURES_IDENTIFIER)
              FEATURES_SYNTAX_ERROR("an identifier");

            lookup_iter lookup = 
              prop_name_to_id.find(features_token_p->pretty_char_text());
            if (lookup == prop_name_to_id.end())
              FEATURES_ERROR("Property \""
                << features_token_p->pretty_char_text() << "\" not defined.");
            int property = lookup->second;

            combined_feature.push_back(make_pair(offset, property));

            features_lex.receive(&features_token_p);
            if (features_token_p->type_id() == QUEX_FEATURES_COMBINE) {
              features_lex.receive(&features_token_p);
              if (features_token_p->type_id() != QUEX_FEATURES_NUMBER)
                FEATURES_SYNTAX_ERROR("a number");
            } else if (features_token_p->type_id() != QUEX_FEATURES_RPAREN)
              FEATURES_SYNTAX_ERROR("a hat or a right parenthesis");
        }

        combined_features.push_back(combined_feature);

        if (features_token_p->type_id() != QUEX_FEATURES_RPAREN)
          FEATURES_SYNTAX_ERROR("a hat or a right parenthesis");
      // End of file
      } else if (features_token_p->type_id() != QUEX_FEATURES_TERMINATION)
        FEATURES_SYNTAX_ERROR("a number or a left parenthesis");

    } while (features_token_p->type_id() != QUEX_FEATURES_TERMINATION);

    } catch (std::runtime_error) {
      cerr << features_file << ":" << features_lex.line_number()
        << ":" << features_lex.column_number() << ": Error: "
        << "Unexpected symbol." << std::endl;
      return 1;
    }


    // INTERPRETING THE FEATURES FILE

    typedef vector< pair< vector<int>,vector<int> > >:: const_iterator
        features_iter;
    typedef vector< vector< pair<int,int> > >::const_iterator
        combined_features_iter;

  
    // We look at all the regular and combined feature selections and determine
    // the leftmost and rightmost offset, which will define our context window.

    precontext = 0, postcontext = 0;

    for (features_iter features_line = features.begin();
         features_line != features.end(); features_line++) {
      for (vector<int>::const_iterator offset = features_line->first.begin();
           offset != features_line->first.end(); offset++) {
        postcontext = max(postcontext, *offset);
        precontext = max(precontext, -(*offset));
      }
    }

    for (combined_features_iter combined_feature = combined_features.begin();
         combined_feature != combined_features.end(); combined_feature++) {
      for (vector< pair<int,int> >::const_iterator
           constituent_feature = combined_feature->begin();
           constituent_feature != combined_feature->end();
           constituent_feature++) {
        postcontext = max(postcontext, constituent_feature->first);
        precontext = max(precontext, -constituent_feature->first);
      }
    }


    // We allocate and zero-initialize the features_mask array...
    features_mask = new bool [(precontext + 1 + postcontext) * n_properties];

    for (int offset = 0; offset < precontext + 1 + postcontext; offset++) {
      for (int property = 0; property < n_properties; property++) {
        FEATURES_MASK(offset, property) = false;
      }
    }

    // and then fill the selected features with 'true'
    for (vector< pair< vector<int>,vector<int> > >::const_iterator
         features_line = features.begin();
         features_line != features.end(); features_line++) {
      for (vector<int>::const_iterator offset = features_line->first.begin();
           offset != features_line->first.end(); offset++) {
        for (vector<int>::const_iterator
             property = features_line->second.begin();
             property != features_line->second.end(); property++)
          FEATURES_MASK(*offset + precontext, *property) = true;
      }
    }

    return 0;
}

}
