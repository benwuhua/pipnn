#include "data/sift_reader.h"

#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace pipnn::data {
namespace {
int ReadDim(std::ifstream& in) {
  std::int32_t dim = 0;
  in.read(reinterpret_cast<char*>(&dim), sizeof(std::int32_t));
  if (!in) throw std::runtime_error("failed to read dim header");
  if (dim <= 0) throw std::runtime_error("non-positive dim");
  return static_cast<int>(dim);
}
}  // namespace

std::vector<std::vector<float>> LoadFvecs(const std::filesystem::path& path, std::size_t max_rows) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open fvecs");

  std::vector<std::vector<float>> out;
  int expected_dim = -1;
  while (in.peek() != EOF) {
    if (max_rows > 0 && out.size() >= max_rows) break;
    int dim = ReadDim(in);
    if (expected_dim == -1) expected_dim = dim;
    if (dim != expected_dim) throw std::runtime_error("inconsistent fvec dim");
    std::vector<float> v(dim);
    in.read(reinterpret_cast<char*>(v.data()), sizeof(float) * dim);
    if (!in) throw std::runtime_error("truncated fvec record");
    out.push_back(std::move(v));
  }
  return out;
}

std::vector<std::vector<int>> LoadIvecs(const std::filesystem::path& path, std::size_t max_rows) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open ivecs");

  std::vector<std::vector<int>> out;
  int expected_dim = -1;
  while (in.peek() != EOF) {
    if (max_rows > 0 && out.size() >= max_rows) break;
    int dim = ReadDim(in);
    if (expected_dim == -1) expected_dim = dim;
    if (dim != expected_dim) throw std::runtime_error("inconsistent ivec dim");
    std::vector<int> v(dim);
    in.read(reinterpret_cast<char*>(v.data()), sizeof(std::int32_t) * dim);
    if (!in) throw std::runtime_error("truncated ivec record");
    out.push_back(std::move(v));
  }
  return out;
}
}  // namespace pipnn::data
