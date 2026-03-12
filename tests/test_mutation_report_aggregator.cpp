#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>

// [unit] fixture-driven contract for raw mutation report aggregation
// [integration] invokes the real python entrypoint against real fixture files

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

CommandResult RunCommand(const std::string& command) {
  const auto dir = std::filesystem::temp_directory_path() /
                   ("pipnn_mutation_agg_" + std::to_string(std::rand()));
  std::filesystem::create_directories(dir);
  const auto stdout_path = dir / "stdout.txt";
  const auto stderr_path = dir / "stderr.txt";
  const int status =
      std::system((command + " >" + ShellQuote(stdout_path) + " 2>" + ShellQuote(stderr_path)).c_str());

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

void WriteReport(const std::filesystem::path& path, const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  out << contents;
}

CommandResult RunAggregator(const std::filesystem::path& input_dir,
                            const std::filesystem::path& output_path) {
  const auto repo_root = std::filesystem::path(PIPNN_REPO_ROOT);
  const auto script = repo_root / "scripts/quality/aggregate_mutation_reports.py";
  const std::string command = "python3 " + ShellQuote(script) + " --input-dir " +
                              ShellQuote(input_dir) + " --output " + ShellQuote(output_path);
  return RunCommand(command);
}
}  // namespace

int main() {
  {
    const auto dir = std::filesystem::temp_directory_path() / "pipnn_mutation_agg_pass";
    std::filesystem::remove_all(dir);
    const auto input_dir = dir / "raw";
    const auto output_path = dir / "summary.json";

    WriteReport(input_dir / "hashprune.json",
                R"JSON({
  "schemaVersion": "1",
  "files": {
    "src/core/hashprune.cpp": {
      "language": "cpp",
      "mutants": [
        { "id": "hp-1", "mutatorName": "cxx_eq_to_ne", "status": "Killed" },
        { "id": "hp-2", "mutatorName": "cxx_eq_to_ne", "status": "Survived" }
      ]
    }
  }
})JSON");
    WriteReport(input_dir / "graph.json",
                R"JSON({
  "schemaVersion": "1",
  "files": {
    "src/core/rbc.cpp": {
      "language": "cpp",
      "mutants": [
        { "id": "rbc-1", "mutatorName": "cxx_lt_to_le", "status": "Killed" }
      ]
    },
    "src/search/greedy_beam.cpp": {
      "language": "cpp",
      "mutants": [
        { "id": "beam-1", "mutatorName": "cxx_add_to_sub", "status": "Killed" },
        { "id": "beam-2", "mutatorName": "cxx_add_to_sub", "status": "Survived" }
      ]
    }
  }
})JSON");

    const auto result = RunAggregator(input_dir, output_path);
    if (result.exit_code != 0) {
      std::cerr << "aggregator pass stdout:\n" << result.out << "\n";
      std::cerr << "aggregator pass stderr:\n" << result.err << "\n";
    }
    assert(result.exit_code == 0);
    const auto summary = ReadAll(output_path);
    assert(summary.find("\"report_count\": 2") != std::string::npos);
    assert(summary.find("\"mutants_total\": 5") != std::string::npos);
    assert(summary.find("\"killed\": 3") != std::string::npos);
    assert(summary.find("\"survived\": 2") != std::string::npos);
    assert(summary.find("\"mutation_score\": 60.0") != std::string::npos);
    assert(summary.find("\"src/core/hashprune.cpp\"") != std::string::npos);
    assert(summary.find("\"src/search/greedy_beam.cpp\"") != std::string::npos);
    std::filesystem::remove_all(dir);
  }

  {
    const auto dir = std::filesystem::temp_directory_path() / "pipnn_mutation_agg_fail";
    std::filesystem::remove_all(dir);
    const auto input_dir = dir / "raw";
    const auto output_path = dir / "summary.json";

    WriteReport(input_dir / "invalid.json",
                R"JSON({
  "schemaVersion": "1",
  "not_files": {}
})JSON");

    const auto result = RunAggregator(input_dir, output_path);
    if (result.err.find("missing files object") == std::string::npos) {
      std::cerr << "aggregator fail stdout:\n" << result.out << "\n";
      std::cerr << "aggregator fail stderr:\n" << result.err << "\n";
    }
    assert(result.exit_code == 1);
    assert(result.err.find("missing files object") != std::string::npos);
    std::filesystem::remove_all(dir);
  }

  return 0;
}
