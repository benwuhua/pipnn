#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <vector>

namespace {
struct CommandResult {
  int exit_code = -1;
  std::string out;
  std::string err;
};

std::string ReadAll(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::ostringstream oss;
  oss << in.rdbuf();
  return oss.str();
}

std::string ShellQuote(const std::filesystem::path& path) {
  return "'" + path.string() + "'";
}

std::string ShellQuote(const std::string& value) {
  return "'" + value + "'";
}

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

CommandResult RunCli(const std::string& args, const std::string& env_prefix = "",
                     const std::filesystem::path& working_dir = {}) {
  auto dir = std::filesystem::temp_directory_path() /
             ("pipnn_cli_test_" + std::to_string(std::rand()));
  std::filesystem::create_directories(dir);
  auto stdout_path = dir / "stdout.txt";
  auto stderr_path = dir / "stderr.txt";

  std::string command;
  if (!working_dir.empty()) {
    command += "cd " + ShellQuote(working_dir) + " && ";
  }
  command += env_prefix + std::string(PIPNN_BIN_PATH) + " " + args + " >" + ShellQuote(stdout_path) +
             " 2>" + ShellQuote(stderr_path);
  int status = std::system(command.c_str());

  CommandResult result;
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  } else {
    result.exit_code = status;
  }
  result.out = ReadAll(stdout_path);
  result.err = ReadAll(stderr_path);

  std::filesystem::remove(stdout_path);
  std::filesystem::remove(stderr_path);
  std::filesystem::remove(dir);
  return result;
}
}  // namespace

int main() {
  // [integration] CLI entrypoint should expose the documented interface.
  // ST-FUNC-001-001
  auto help = RunCli("--help");
  assert(help.exit_code == 0);
  assert(help.out.find("Usage: pipnn") != std::string::npos);
  assert(help.out.find("--mode <pipnn|hnsw>") != std::string::npos);
  assert(help.out.find("--hnsw-m N") != std::string::npos);

  // [integration] HNSW mode should route through the standard baseline and emit the shared JSON schema.
  // ST-FUNC-001-002
  auto json_path = std::filesystem::temp_directory_path() / "pipnn_cli_hnsw_metrics.json";
  auto hnsw =
      RunCli("--mode hnsw --dataset synthetic --metric l2 --output " + ShellQuote(json_path));
  assert(hnsw.exit_code == 0);
  std::string metrics = ReadAll(json_path);
  assert(metrics.find("\"mode\": \"hnsw\"") != std::string::npos);
  assert(metrics.find("\"build_sec\": ") != std::string::npos);
  assert(metrics.find("\"recall_at_10\": ") != std::string::npos);
  std::filesystem::remove(json_path);

  auto invalid_metric =
      RunCli("--mode pipnn --dataset synthetic --metric cosine --output '/tmp/pipnn_invalid_metric.json'");
  assert(invalid_metric.exit_code == 1);
  assert(invalid_metric.err.find("only l2 metric supported") != std::string::npos);

  auto invalid_beam =
      RunCli("--mode pipnn --dataset synthetic --metric l2 --output '/tmp/pipnn_invalid_beam.json' --beam nope");
  assert(invalid_beam.exit_code == 1);
  // ST-BNDRY-001-002
  assert(invalid_beam.err.find("invalid value for --beam: nope") != std::string::npos);

  auto invalid_hnsw_m =
      RunCli("--mode hnsw --dataset synthetic --metric l2 --output '/tmp/pipnn_invalid_hnsw_m.json' --hnsw-m 0");
  assert(invalid_hnsw_m.exit_code == 1);
  assert(invalid_hnsw_m.err.find("invalid value for --hnsw-m: 0") != std::string::npos);

  auto invalid_hnsw_ef_search = RunCli(
      "--mode hnsw --dataset synthetic --metric l2 --output '/tmp/pipnn_invalid_hnsw_ef_search.json' "
      "--hnsw-ef-search -1");
  assert(invalid_hnsw_ef_search.exit_code == 1);
  assert(invalid_hnsw_ef_search.err.find("invalid value for --hnsw-ef-search: -1") != std::string::npos);

  // ST-BNDRY-001-001
  auto unsupported =
      RunCli("--mode pipnn --dataset nope --metric l2 --output '/tmp/pipnn_unsupported.json'");
  assert(unsupported.exit_code == 1);
  assert(unsupported.err.find("unsupported dataset: nope") != std::string::npos);

  auto missing_query = RunCli("--mode pipnn --dataset sift1m --metric l2 --base '/tmp/base_only.fvecs' "
                              "--output '/tmp/pipnn_missing_query.json'");
  assert(missing_query.exit_code == 1);
  assert(missing_query.err.find("sift1m requires --base <base.fvecs> and --query <query.fvecs>") !=
         std::string::npos);

  // [integration] Missing SIFT files should be reported as a controlled CLI error, not an abort.
  // ST-FUNC-001-003
  auto missing = RunCli("--mode pipnn --dataset sift1m --metric l2 --base '/tmp/no-such-base.fvecs' "
                        "--query '/tmp/no-such-query.fvecs' --output '/tmp/pipnn_missing.json'");
  assert(missing.exit_code == 1);
  assert(missing.err.find("cannot open") != std::string::npos);

  auto dir = std::filesystem::temp_directory_path() / "pipnn_cli_dataset";
  std::filesystem::create_directories(dir);
  auto base_path = dir / "base.fvecs";
  auto query_path = dir / "query.fvecs";
  auto truth_path = dir / "truth.ivecs";
  auto pipnn_json = dir / "pipnn_metrics.json";
  auto hnsw_sift_json = dir / "hnsw_sift_metrics.json";
  WriteFvecs(base_path, {
                            {0.0f, 0.0f},
                            {1.0f, 0.0f},
                            {0.0f, 1.0f},
                            {1.0f, 1.0f},
                            {2.0f, 2.0f},
                            {3.0f, 3.0f},
                            {4.0f, 4.0f},
                            {5.0f, 5.0f},
                            {6.0f, 6.0f},
                            {7.0f, 7.0f},
                        });
  WriteFvecs(query_path, {{0.1f, 0.1f}, {6.2f, 6.1f}});
  WriteIvecs(truth_path, {
                            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
                            {8, 9, 7, 6, 5, 4, 3, 2, 1, 0},
                        });

  auto pipnn_sift = RunCli(
      "--mode pipnn --dataset sift1m --metric l2 --base " + ShellQuote(base_path) +
          " --query " + ShellQuote(query_path) + " --truth " + ShellQuote(truth_path) +
          " --output " + ShellQuote(pipnn_json) +
          " --max-base 8 --max-query 2 --rbc-cmax 4 --rbc-fanout 1 --leader-frac 0.5 "
          "--max-leaders 4 --replicas 2 --leaf-k 2 --leaf-scan-cap 4 --max-degree 4 "
          "--hash-bits 4 --beam 8 --bidirected 0",
      "PIPNN_ECHO_CONFIG=1 PIPNN_PROFILE=1 ");
  assert(pipnn_sift.exit_code == 0);
  assert(pipnn_sift.out.find("cfg mode=pipnn dataset=sift1m max_base=8 max_query=2") != std::string::npos);
  assert(pipnn_sift.out.find("pipnn_profile_build partition_sec=") != std::string::npos);
  std::string pipnn_metrics = ReadAll(pipnn_json);
  assert(pipnn_metrics.find("\"mode\": \"pipnn\"") != std::string::npos);

  auto hnsw_sift =
      RunCli("--mode hnsw --dataset sift1m --metric l2 --base " + ShellQuote(base_path) +
             " --query " + ShellQuote(query_path) + " --truth " + ShellQuote(truth_path) +
             " --output " + ShellQuote(hnsw_sift_json) +
             " --max-base 8 --max-query 2 --hnsw-m 24 --hnsw-ef-construction 400 --hnsw-ef-search 160",
             "PIPNN_ECHO_CONFIG=1 ");
  assert(hnsw_sift.exit_code == 0);
  assert(hnsw_sift.out.find("hnsw_m=24") != std::string::npos);
  assert(hnsw_sift.out.find("hnsw_ef_construction=400") != std::string::npos);
  assert(hnsw_sift.out.find("hnsw_ef_search=160") != std::string::npos);
  std::string hnsw_sift_metrics = ReadAll(hnsw_sift_json);
  assert(hnsw_sift_metrics.find("\"mode\": \"hnsw\"") != std::string::npos);

  std::filesystem::remove(base_path);
  std::filesystem::remove(query_path);
  std::filesystem::remove(truth_path);
  std::filesystem::remove(pipnn_json);
  std::filesystem::remove(hnsw_sift_json);
  std::filesystem::remove(dir);

  auto basename_dir = std::filesystem::temp_directory_path() / "pipnn_cli_basename";
  std::filesystem::create_directories(basename_dir);
  auto basename_json = basename_dir / "metrics.json";
  // ST-BNDRY-001-003
  auto basename = RunCli("--mode pipnn --dataset synthetic --metric l2 --output metrics.json", "",
                         basename_dir);
  assert(basename.exit_code == 0);
  std::string basename_metrics = ReadAll(basename_json);
  assert(basename_metrics.find("\"mode\": \"pipnn\"") != std::string::npos);
  std::filesystem::remove(basename_json);
  std::filesystem::remove(basename_dir);

  return 0;
}
