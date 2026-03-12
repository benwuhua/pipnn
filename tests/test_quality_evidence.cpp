#include <cassert>
#include <cstdio>
#include <cstdlib>
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

CommandResult RunCommand(const std::string& command) {
  auto dir = std::filesystem::temp_directory_path() /
             ("pipnn_quality_cmd_" + std::to_string(std::rand()));
  std::filesystem::create_directories(dir);
  auto stdout_path = dir / "stdout.txt";
  auto stderr_path = dir / "stderr.txt";
  int status = std::system((command + " >" + ShellQuote(stdout_path) + " 2>" + ShellQuote(stderr_path)).c_str());

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

CommandResult RunValidator(const std::filesystem::path& line_path,
                           const std::filesystem::path& branch_path) {
  const std::filesystem::path repo_root = PIPNN_REPO_ROOT;
  const std::filesystem::path validator = repo_root / "scripts/validate_quality_evidence.py";
  const std::filesystem::path feature_list = repo_root / "feature-list.json";

  std::string command = "python3 " + ShellQuote(validator) + " --feature-list " + ShellQuote(feature_list) +
                        " --line-report " + ShellQuote(line_path) + " --branch-report " + ShellQuote(branch_path);
  return RunCommand(command);
}

void WriteCoverageReport(const std::filesystem::path& path, int percent) {
  std::ofstream out(path);
  out << "------------------------------------------------------------------------------\n";
  out << "                           GCC Code Coverage Report\n";
  out << "Directory: .\n";
  out << "------------------------------------------------------------------------------\n";
  out << "File                                       Lines     Exec  Cover   Missing\n";
  out << "------------------------------------------------------------------------------\n";
  out << "src/foo.cpp                                  10       10   100%\n";
  out << "------------------------------------------------------------------------------\n";
  out << "TOTAL                                        10       10    " << percent
      << "%\n";
  out << "------------------------------------------------------------------------------\n";
}
}  // namespace

int main() {
  const std::filesystem::path repo_root = PIPNN_REPO_ROOT;

  {
    auto dir = std::filesystem::temp_directory_path() / "pipnn_quality_fixture_pass";
    std::filesystem::create_directories(dir);
    auto line = dir / "line.txt";
    auto branch = dir / "branch.txt";
    WriteCoverageReport(line, 95);
    WriteCoverageReport(branch, 92);
    auto result = RunValidator(line, branch);
    assert(result.exit_code == 0);
    assert(result.out.find("line_coverage=95") != std::string::npos);
    assert(result.out.find("branch_coverage=92") != std::string::npos);
    std::filesystem::remove(line);
    std::filesystem::remove(branch);
    std::filesystem::remove(dir);
  }

  {
    auto dir = std::filesystem::temp_directory_path() / "pipnn_quality_fixture_fail";
    std::filesystem::create_directories(dir);
    auto line = dir / "line.txt";
    auto branch = dir / "branch.txt";
    WriteCoverageReport(line, 95);
    WriteCoverageReport(branch, 79);
    auto result = RunValidator(line, branch);
    assert(result.exit_code == 1);
    assert(result.err.find("branch coverage 79 is below required 80") != std::string::npos);
    std::filesystem::remove(line);
    std::filesystem::remove(branch);
    std::filesystem::remove(dir);
  }

  {
    auto result = RunValidator(repo_root / "results/st/line_coverage.txt",
                               repo_root / "results/st/branch_coverage.txt");
    assert(result.exit_code == 0);
    assert(result.out.find("line_coverage=95") != std::string::npos);
    assert(result.out.find("branch_coverage=92") != std::string::npos);
  }

  {
    auto command = "python3 " + ShellQuote(repo_root / "scripts/get_tool_commands.py") + " " +
                   ShellQuote(repo_root / "feature-list.json");
    auto result = RunCommand(command);
    assert(result.exit_code == 0);
    assert(result.out.find("coverage: bash scripts/quality/remote_coverage.sh") != std::string::npos);
  }

  return 0;
}
