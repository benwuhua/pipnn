#include "data/sift_reader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
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

std::string LowercaseExtension(const std::filesystem::path& path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return ext;
}

template <typename T>
std::vector<std::vector<T>> LoadBinVectors(const std::filesystem::path& path, std::size_t max_rows,
                                           const char* open_error, const char* truncated_error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error(open_error);

  std::uint32_t count = 0;
  std::uint32_t dim = 0;
  in.read(reinterpret_cast<char*>(&count), sizeof(std::uint32_t));
  in.read(reinterpret_cast<char*>(&dim), sizeof(std::uint32_t));
  if (!in) throw std::runtime_error("failed to read bin header");
  if (dim == 0) throw std::runtime_error("non-positive dim");

  const std::size_t rows = max_rows > 0 ? std::min<std::size_t>(count, max_rows) : count;
  std::vector<std::vector<T>> out;
  out.reserve(rows);
  for (std::size_t i = 0; i < rows; ++i) {
    std::vector<T> row(dim);
    in.read(reinterpret_cast<char*>(row.data()), sizeof(T) * dim);
    if (!in) throw std::runtime_error(truncated_error);
    out.push_back(std::move(row));
  }
  return out;
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

std::vector<std::vector<float>> LoadFloatVectors(const std::filesystem::path& path,
                                                 std::size_t max_rows) {
  const std::string ext = LowercaseExtension(path);
  if (ext == ".fvecs") return LoadFvecs(path, max_rows);
  if (ext == ".fbin") {
    return LoadBinVectors<float>(path, max_rows, "cannot open fbin", "truncated fbin payload");
  }
  throw std::runtime_error("unsupported float vector file extension: " + ext);
}

std::vector<std::vector<int>> LoadIntVectors(const std::filesystem::path& path, std::size_t max_rows) {
  const std::string ext = LowercaseExtension(path);
  if (ext == ".ivecs") return LoadIvecs(path, max_rows);
  if (ext == ".ibin") {
    return LoadBinVectors<int>(path, max_rows, "cannot open ibin", "truncated ibin payload");
  }
  throw std::runtime_error("unsupported int vector file extension: " + ext);
}
}  // namespace pipnn::data
