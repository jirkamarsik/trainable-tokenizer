#ifndef READ_FEATURES_FILE_INCLUDE_GUARD
#define READ_FEATURES_FILE_INCLUDE_GUARD

#include <vector>
#include <string>
#include <boost/unordered_map.hpp>

namespace trtok {

int read_features_file(
          /* The path to the features file. */
          std::string const &features_file,
          /* A mapping from property names to property IDs. */
          boost::unordered_map<std::string, int> const &prop_name_to_id,
          /* The total number of properties (including special builtin ones).*/
          int n_properties,
          /* The total number of user-defined properties. */
          int n_basic_properties,
          /* Output: A zero-based mask[offset, property], which tells whether
             the Classifier should be interested in the property at the offset.
          */
          bool *&features_mask,
          /* Output: A list of combined features which are in turn represented
             by a list of offset-property pairs. */
          std::vector< std::vector< std::pair<int,int> > > &combined_features,
          /* Output: The farthest token before the current token in which
             the user is interesetd. */
          int &precontext,
          /* Output: The farthest token after the curent token in which
             the user is interested. */
          int &postcontext);

}

#endif
