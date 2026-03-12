#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace pipnn::cli {
int Run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);
}  // namespace pipnn::cli
