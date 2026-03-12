#include "data/sift_reader.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

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

  std::filesystem::remove(path);
  return 0;
}
