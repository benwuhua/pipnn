#include "cli/app.h"

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
void WriteFvecs(const std::filesystem::path& path,
                const std::vector<std::vector<float>>& rows) {
  std::ofstream out(path, std::ios::binary);
  for (const auto& row : rows) {
    std::int32_t d = static_cast<std::int32_t>(row.size());
    out.write(reinterpret_cast<const char*>(&d), 4);
    out.write(reinterpret_cast<const char*>(row.data()), sizeof(float) * row.size());
  }
}

void WriteIvecs(const std::filesystem::path& path,
                const std::vector<std::vector<int>>& rows) {
  std::ofstream out(path, std::ios::binary);
  for (const auto& row : rows) {
    std::int32_t d = static_cast<std::int32_t>(row.size());
    out.write(reinterpret_cast<const char*>(&d), 4);
    out.write(reinterpret_cast<const char*>(row.data()), sizeof(std::int32_t) * row.size());
  }
}
}  // namespace

int main() {
  {
    std::ostringstream out;
    std::ostringstream err;
    // ST-FUNC-001-001
    int rc = pipnn::cli::Run({"--help"}, out, err);
    assert(rc == 0);
    assert(out.str().find("Usage: pipnn") != std::string::npos);
    assert(err.str().empty());
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--mode"}, out, err);
    assert(rc == 1);
    assert(err.str().find("missing value for --mode") != std::string::npos);
  }

  for (const std::string& option : std::vector<std::string>{
           "--dataset", "--metric", "--output", "--base", "--query", "--truth", "--max-base",
           "--max-query", "--rbc-cmax", "--rbc-fanout", "--leader-frac", "--max-leaders",
           "--replicas", "--leaf-k", "--leaf-scan-cap", "--max-degree", "--hash-bits", "--beam",
           "--bidirected", "--hnsw-m", "--hnsw-ef-construction", "--hnsw-ef-search"}) {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({option}, out, err);
    assert(rc == 1);
    assert(err.str().find("missing value for " + option) != std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--wat"}, out, err);
    assert(rc == 1);
    assert(err.str().find("unknown argument: --wat") != std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--metric", "cosine"}, out, err);
    assert(rc == 1);
    assert(err.str().find("only l2 metric supported") != std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    // ST-BNDRY-001-002
    int rc = pipnn::cli::Run({"--beam", "nope"}, out, err);
    assert(rc == 1);
    assert(err.str().find("invalid value for --beam: nope") != std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--hnsw-m", "0"}, out, err);
    assert(rc == 1);
    assert(err.str().find("invalid value for --hnsw-m: 0") != std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--hnsw-ef-construction", "-1"}, out, err);
    assert(rc == 1);
    assert(err.str().find("invalid value for --hnsw-ef-construction: -1") != std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--hnsw-ef-search", "-1"}, out, err);
    assert(rc == 1);
    assert(err.str().find("invalid value for --hnsw-ef-search: -1") != std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--leader-frac", "bad"}, out, err);
    assert(rc == 1);
    assert(err.str().find("invalid value for --leader-frac: bad") != std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--max-base", "bad"}, out, err);
    assert(rc == 1);
    assert(err.str().find("invalid value for --max-base: bad") != std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--bidirected", "bad"}, out, err);
    assert(rc == 1);
    assert(err.str().find("invalid value for --bidirected: bad") != std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    // ST-BNDRY-001-001
    int rc = pipnn::cli::Run({"--dataset", "bad"}, out, err);
    assert(rc == 1);
    assert(err.str().find("unsupported dataset: bad") != std::string::npos);
  }

  {
    auto output = std::filesystem::temp_directory_path() / "pipnn_cli_app_hnsw.json";
    std::ostringstream out;
    std::ostringstream err;
    // ST-FUNC-001-002
    int rc = pipnn::cli::Run(
        {"--mode", "hnsw", "--dataset", "synthetic", "--metric", "l2", "--output", output.string()},
        out, err);
    assert(rc == 0);
    assert(err.str().empty());
    std::ifstream in(output);
    std::stringstream metrics;
    metrics << in.rdbuf();
    assert(metrics.str().find("\"mode\": \"hnsw\"") != std::string::npos);
    std::filesystem::remove(output);
  }

  {
    auto output = std::filesystem::temp_directory_path() / "pipnn_cli_app_vamana.json";
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run(
        {"--mode", "vamana", "--dataset", "synthetic", "--metric", "l2", "--output", output.string()},
        out, err);
    assert(rc == 0);
    assert(err.str().empty());
    std::ifstream in(output);
    std::stringstream metrics;
    metrics << in.rdbuf();
    assert(metrics.str().find("\"mode\": \"vamana\"") != std::string::npos);
    std::filesystem::remove(output);
  }

  {
    auto output = std::filesystem::temp_directory_path() / "pipnn_cli_app_pipnn_vamana.json";
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--mode",
                              "pipnn_vamana",
                              "--dataset",
                              "synthetic",
                              "--metric",
                              "l2",
                              "--output",
                              output.string()},
                             out, err);
    assert(rc == 0);
    assert(err.str().empty());
    std::ifstream in(output);
    std::stringstream metrics;
    metrics << in.rdbuf();
    assert(metrics.str().find("\"mode\": \"pipnn_vamana\"") != std::string::npos);
    std::filesystem::remove(output);
  }

  {
    auto output = std::filesystem::temp_directory_path() / "pipnn_cli_app_hnsw_bounds.json";
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--mode",
                              "hnsw",
                              "--dataset",
                              "synthetic",
                              "--metric",
                              "l2",
                              "--output",
                              output.string(),
                              "--beam",
                              std::to_string(std::numeric_limits<int>::max()),
                              "--max-base",
                              std::to_string(std::numeric_limits<std::size_t>::max()),
                              "--hnsw-m",
                              std::to_string(std::numeric_limits<int>::max()),
                              "--hnsw-ef-construction",
                              std::to_string(std::numeric_limits<int>::max()),
                              "--hnsw-ef-search",
                              std::to_string(std::numeric_limits<int>::max())},
                             out, err);
    assert(rc == 0);
    assert(err.str().empty());
    std::filesystem::remove(output);
  }

  {
    auto output = std::filesystem::temp_directory_path() / "pipnn_cli_app_hnsw_intmin.json";
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--mode",
                              "hnsw",
                              "--dataset",
                              "synthetic",
                              "--metric",
                              "l2",
                              "--output",
                              output.string(),
                              "--rbc-cmax",
                              std::to_string(std::numeric_limits<int>::min())},
                             out, err);
    assert(rc == 0);
    assert(err.str().empty());
    std::filesystem::remove(output);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--mode", "pipnn", "--dataset", "sift1m", "--metric", "l2"}, out, err);
    assert(rc == 1);
    assert(err.str().find("sift1m requires --base <base.fvecs> and --query <query.fvecs>") !=
           std::string::npos);
  }

  {
    std::ostringstream out;
    std::ostringstream err;
    // ST-FUNC-001-003
    int rc = pipnn::cli::Run({"--mode", "pipnn",
                              "--dataset", "sift1m",
                              "--metric", "l2",
                              "--base", "/tmp/no-such-base.fvecs",
                              "--query", "/tmp/no-such-query.fvecs"},
                             out, err);
    assert(rc == 1);
    assert(err.str().find("cannot open fvecs") != std::string::npos);
  }

  {
    auto dir = std::filesystem::temp_directory_path() / "pipnn_cli_app_data";
    std::filesystem::create_directories(dir);
    auto base = dir / "base.fvecs";
    auto query = dir / "query.fvecs";
    auto truth = dir / "truth.ivecs";
    auto output = dir / "pipnn.json";
    WriteFvecs(base, {{0.0f, 0.0f}, {1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}});
    WriteFvecs(query, {{0.1f, 0.0f}, {2.9f, 0.0f}});
    WriteIvecs(truth, {{0, 1, 2, 3}, {3, 2, 1, 0}});

    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--mode", "pipnn",
                              "--dataset", "sift1m",
                              "--metric", "l2",
                              "--base", base.string(),
                              "--query", query.string(),
                              "--truth", truth.string(),
                              "--output", output.string(),
                              "--max-base", "3",
                              "--max-query", "1",
                              "--rbc-cmax", "2",
                              "--rbc-fanout", "1",
                              "--leader-frac", "0.5",
                              "--max-leaders", "2",
                              "--replicas", "2",
                              "--leaf-k", "1",
                              "--leaf-scan-cap", "1",
                              "--max-degree", "2",
                              "--hash-bits", "2",
                              "--beam", "2",
                              "--bidirected", "0"},
                             out, err);
    assert(rc == 0);
    assert(err.str().empty());
    std::ifstream in(output);
    std::stringstream metrics;
    metrics << in.rdbuf();
    assert(metrics.str().find("\"mode\": \"pipnn\"") != std::string::npos);
    assert(metrics.str().find("\"edges\": ") != std::string::npos);
    std::filesystem::remove(base);
    std::filesystem::remove(query);
    std::filesystem::remove(truth);
    std::filesystem::remove(output);
    std::filesystem::remove(dir);
  }

  {
    auto output = std::filesystem::temp_directory_path() / "pipnn_cli_app_echo" / "metrics.json";
    setenv("PIPNN_ECHO_CONFIG", "1", 1);
    std::ostringstream out;
    std::ostringstream err;
    // ST-BNDRY-001-003
    int rc = pipnn::cli::Run({"--mode", "pipnn",
                              "--dataset", "synthetic",
                              "--metric", "l2",
                              "--output", output.string(),
                              "--max-base", "5",
                              "--max-query", "2",
                              "--rbc-cmax", "16",
                              "--rbc-fanout", "1",
                              "--leader-frac", "0.5",
                              "--max-leaders", "8",
                              "--replicas", "1",
                              "--leaf-k", "4",
                              "--leaf-scan-cap", "2",
                              "--max-degree", "8",
                              "--hash-bits", "4",
                              "--beam", "8",
                              "--bidirected", "1"},
                             out, err);
    unsetenv("PIPNN_ECHO_CONFIG");
    assert(rc == 0);
    assert(err.str().empty());
    assert(out.str().find("cfg mode=pipnn dataset=synthetic max_base=5 max_query=2") !=
           std::string::npos);
    std::filesystem::remove(output);
    std::filesystem::remove(output.parent_path());
  }

  {
    auto output = std::filesystem::temp_directory_path() / "pipnn_cli_app_bidirected_echo.json";
    setenv("PIPNN_ECHO_CONFIG", "1", 1);
    std::ostringstream out0;
    std::ostringstream err0;
    int rc0 = pipnn::cli::Run({"--mode", "pipnn", "--dataset", "synthetic", "--metric", "l2",
                               "--output", output.string(), "--bidirected", "0"},
                              out0, err0);
    assert(rc0 == 0);
    assert(err0.str().empty());
    assert(out0.str().find("bidirected=0") != std::string::npos);

    std::ostringstream out1;
    std::ostringstream err1;
    int rc1 = pipnn::cli::Run({"--mode", "pipnn", "--dataset", "synthetic", "--metric", "l2",
                               "--output", output.string(), "--bidirected", "1"},
                              out1, err1);
    unsetenv("PIPNN_ECHO_CONFIG");
    assert(rc1 == 0);
    assert(err1.str().empty());
    assert(out1.str().find("bidirected=1") != std::string::npos);

    std::filesystem::remove(output);
  }

  {
    auto output = std::filesystem::temp_directory_path() / "pipnn_cli_app_hnsw_echo.json";
    setenv("PIPNN_ECHO_CONFIG", "1", 1);
    std::ostringstream out;
    std::ostringstream err;
    int rc = pipnn::cli::Run({"--mode",
                              "hnsw",
                              "--dataset",
                              "synthetic",
                              "--metric",
                              "l2",
                              "--output",
                              output.string(),
                              "--hnsw-m",
                              "24",
                              "--hnsw-ef-construction",
                              "400",
                              "--hnsw-ef-search",
                              "160"},
                             out, err);
    unsetenv("PIPNN_ECHO_CONFIG");
    assert(rc == 0);
    assert(err.str().empty());
    assert(out.str().find("hnsw_m=24") != std::string::npos);
    assert(out.str().find("hnsw_ef_construction=400") != std::string::npos);
    assert(out.str().find("hnsw_ef_search=160") != std::string::npos);
    std::filesystem::remove(output);
  }

  return 0;
}
