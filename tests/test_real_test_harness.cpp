#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>

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

CommandResult RunCommand(const std::string& command,
                         const std::filesystem::path& working_dir = std::filesystem::current_path()) {
  auto scratch = std::filesystem::temp_directory_path() / "pipnn_real_test_harness";
  std::filesystem::create_directories(scratch);
  auto stdout_path = scratch / "stdout.txt";
  auto stderr_path = scratch / "stderr.txt";
  std::string wrapped = "cd " + ShellQuote(working_dir) + " && " + command + " >" +
                        ShellQuote(stdout_path) + " 2>" + ShellQuote(stderr_path);
  int status = std::system(wrapped.c_str());

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
  return result;
}

void WriteText(const std::filesystem::path& path, const std::string& content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  out << content;
}
}  // namespace

int main() {
  const std::filesystem::path repo_root = PIPNN_REPO_ROOT;
  const auto temp_root = std::filesystem::temp_directory_path() / "pipnn_real_test_fixture";
  std::filesystem::remove_all(temp_root);
  std::filesystem::create_directories(temp_root);

  const auto feature_list = temp_root / "feature-list.json";
  WriteText(feature_list,
            R"({
  "real_test": {
    "marker_pattern": "^test_.*\\.cpp$",
    "mock_patterns": ["mock", "stub", "fake"],
    "test_dir": "tests"
  },
  "features": [
    {
      "id": 1,
      "title": "CLI 参数与模式路由",
      "status": "failing"
    }
  ]
}
)");

  WriteText(temp_root / "tests/test_cli_fixture.cpp",
            R"(// [integration] CLI writes metrics to disk without mocks.
int main() { return 0; }
)");

  {
    auto result = RunCommand("python3 " + ShellQuote(repo_root / "scripts/check_real_tests.py") + " " +
                                 ShellQuote(feature_list) + " --feature 1",
                             temp_root);
    assert(result.exit_code == 0);
    assert(result.out.find("PASS") != std::string::npos);
    assert(result.out.find("real_test_markers=1") != std::string::npos);
  }

  std::filesystem::remove(temp_root / "tests/test_cli_fixture.cpp");
  WriteText(temp_root / "tests/test_cli_fixture.cpp",
            R"(// [unit] No integration marker here.
int main() { return 0; }
)");

  {
    auto result = RunCommand("python3 " + ShellQuote(repo_root / "scripts/check_real_tests.py") + " " +
                                 ShellQuote(feature_list) + " --feature 1",
                             temp_root);
    assert(result.exit_code == 1);
    assert(result.out.find("FAIL") != std::string::npos);
    assert(result.out.find("no [integration] markers") != std::string::npos);
  }

  std::filesystem::remove_all(temp_root);
  return 0;
}
