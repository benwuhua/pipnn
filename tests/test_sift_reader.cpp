#include "data/sift_reader.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {
void WriteFvecs(const std::filesystem::path& path,
                const std::vector<std::vector<float>>& rows) {
  std::ofstream out(path, std::ios::binary);
  for (const auto& row : rows) {
    std::int32_t d = static_cast<std::int32_t>(row.size());
    out.write(reinterpret_cast<const char*>(&d), 4);
    out.write(reinterpret_cast<const char*>(row.data()),
              sizeof(float) * row.size());
  }
}

void WriteIvecs(const std::filesystem::path& path,
                const std::vector<std::vector<int>>& rows) {
  std::ofstream out(path, std::ios::binary);
  for (const auto& row : rows) {
    std::int32_t d = static_cast<std::int32_t>(row.size());
    out.write(reinterpret_cast<const char*>(&d), 4);
    out.write(reinterpret_cast<const char*>(row.data()),
              sizeof(std::int32_t) * row.size());
  }
}
}  // namespace

int main() {
  auto path = std::filesystem::temp_directory_path() / "test_sift_reader.fvecs";
  {
    std::ofstream out(path, std::ios::binary);
    std::int32_t d = 2;
    float a[2] = {1.0f, 2.0f};
    float b[2] = {3.0f, 4.0f};
    out.write(reinterpret_cast<const char*>(&d), 4);
    out.write(reinterpret_cast<const char*>(a), sizeof(a));
    out.write(reinterpret_cast<const char*>(&d), 4);
    out.write(reinterpret_cast<const char*>(b), sizeof(b));
  }

  auto vecs = pipnn::data::LoadFvecs(path);
  assert(vecs.size() == 2);
  assert(vecs[0].size() == 2);
  assert(vecs[1][1] == 4.0f);

  auto limited = pipnn::data::LoadFvecs(path, 1);
  assert(limited.size() == 1);
  assert(limited[0][0] == 1.0f);

  auto ivecs_path = std::filesystem::temp_directory_path() / "test_sift_reader.ivecs";
  WriteIvecs(ivecs_path, {{10, 11, 12}, {20, 21, 22}});
  auto ivecs = pipnn::data::LoadIvecs(ivecs_path);
  assert(ivecs.size() == 2);
  assert(ivecs[1][2] == 22);
  auto ivecs_limited = pipnn::data::LoadIvecs(ivecs_path, 1);
  assert(ivecs_limited.size() == 1);
  assert(ivecs_limited[0][1] == 11);

  bool missing_fvecs = false;
  try {
    (void)pipnn::data::LoadFvecs("/tmp/no-such-reader-file.fvecs");
  } catch (const std::runtime_error& ex) {
    missing_fvecs = std::string(ex.what()).find("cannot open fvecs") != std::string::npos;
  }
  assert(missing_fvecs);

  bool missing_ivecs = false;
  try {
    (void)pipnn::data::LoadIvecs("/tmp/no-such-reader-file.ivecs");
  } catch (const std::runtime_error& ex) {
    missing_ivecs = std::string(ex.what()).find("cannot open ivecs") != std::string::npos;
  }
  assert(missing_ivecs);

  auto inconsistent = std::filesystem::temp_directory_path() / "test_sift_inconsistent.fvecs";
  WriteFvecs(inconsistent, {{1.0f, 2.0f}, {3.0f}});
  bool inconsistent_dim = false;
  try {
    (void)pipnn::data::LoadFvecs(inconsistent);
  } catch (const std::runtime_error& ex) {
    inconsistent_dim = std::string(ex.what()).find("inconsistent fvec dim") != std::string::npos;
  }
  assert(inconsistent_dim);

  auto truncated = std::filesystem::temp_directory_path() / "test_sift_truncated.fvecs";
  {
    std::ofstream out(truncated, std::ios::binary);
    std::int32_t d = 2;
    float only_one = 1.0f;
    out.write(reinterpret_cast<const char*>(&d), 4);
    out.write(reinterpret_cast<const char*>(&only_one), sizeof(float));
  }
  bool truncated_record = false;
  try {
    (void)pipnn::data::LoadFvecs(truncated);
  } catch (const std::runtime_error& ex) {
    truncated_record = std::string(ex.what()).find("truncated fvec record") != std::string::npos;
  }
  assert(truncated_record);

  auto bad_dim = std::filesystem::temp_directory_path() / "test_sift_bad_dim.ivecs";
  {
    std::ofstream out(bad_dim, std::ios::binary);
    std::int32_t d = 0;
    out.write(reinterpret_cast<const char*>(&d), 4);
  }
  bool non_positive_dim = false;
  try {
    (void)pipnn::data::LoadIvecs(bad_dim);
  } catch (const std::runtime_error& ex) {
    non_positive_dim = std::string(ex.what()).find("non-positive dim") != std::string::npos;
  }
  assert(non_positive_dim);

  auto inconsistent_ivecs = std::filesystem::temp_directory_path() / "test_sift_inconsistent.ivecs";
  WriteIvecs(inconsistent_ivecs, {{1, 2}, {3}});
  bool inconsistent_ivec_dim = false;
  try {
    (void)pipnn::data::LoadIvecs(inconsistent_ivecs);
  } catch (const std::runtime_error& ex) {
    inconsistent_ivec_dim = std::string(ex.what()).find("inconsistent ivec dim") != std::string::npos;
  }
  assert(inconsistent_ivec_dim);

  auto truncated_ivecs = std::filesystem::temp_directory_path() / "test_sift_truncated.ivecs";
  {
    std::ofstream out(truncated_ivecs, std::ios::binary);
    std::int32_t d = 2;
    std::int32_t only_one = 7;
    out.write(reinterpret_cast<const char*>(&d), 4);
    out.write(reinterpret_cast<const char*>(&only_one), sizeof(std::int32_t));
  }
  bool truncated_ivec_record = false;
  try {
    (void)pipnn::data::LoadIvecs(truncated_ivecs);
  } catch (const std::runtime_error& ex) {
    truncated_ivec_record = std::string(ex.what()).find("truncated ivec record") != std::string::npos;
  }
  assert(truncated_ivec_record);

  std::filesystem::remove(path);
  std::filesystem::remove(ivecs_path);
  std::filesystem::remove(inconsistent);
  std::filesystem::remove(truncated);
  std::filesystem::remove(bad_dim);
  std::filesystem::remove(inconsistent_ivecs);
  std::filesystem::remove(truncated_ivecs);
  return 0;
}
