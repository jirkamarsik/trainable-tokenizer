#ifndef READ_FEATURES_FILE_INCLUDE_GUARD
#define READ_FEATURES_FILE_INCLUDE_GUARD

#include <vector>
#include <string>
#include <boost/unordered_map.hpp>

using namespace std;

namespace trtok {

int read_features_file(
                  string const &features_file,
                  boost::unordered_map<string, int> const &prop_name_to_id,
                  int n_properties,
                  int n_basic_properties,
                  bool *&features_mask,
                  vector< vector< pair<int,int> > > &combined_features,
                  int &precontext,
                  int &postcontext);

}

#endif
