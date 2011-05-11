#ifndef ROUGH_TOK_COMPILE_INCLUDE_GUARD
#define ROUGH_TOK_COMPILE_INCLUDE_GUARD

#include <vector>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace trtok {
    bool compile_rough_lexer(std::vector<fs::path> const &split_files,
                             std::vector<fs::path> const &join_files,
                             std::vector<fs::path> const &begin_files,
                             std::vector<fs::path> const &end_files,
                             fs::path const &build_path);
}

#endif
