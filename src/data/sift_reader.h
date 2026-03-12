#pragma once

#include "common/types.h"

#include <filesystem>
#include <vector>

namespace pipnn::data {
std::vector<std::vector<float>> LoadFvecs(const std::filesystem::path& path,
                                          std::size_t max_rows = 0);
std::vector<std::vector<int>> LoadIvecs(const std::filesystem::path& path,
                                        std::size_t max_rows = 0);
}  // namespace pipnn::data
