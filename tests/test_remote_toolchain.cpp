#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
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

void MakeExecutable(const std::filesystem::path& path, const std::string& body) {
  std::ofstream out(path);
  out << body;
  out.close();
  auto perms = std::filesystem::status(path).permissions();
  std::filesystem::permissions(path, perms | std::filesystem::perms::owner_exec |
                                         std::filesystem::perms::group_exec |
                                         std::filesystem::perms::others_exec);
}

CommandResult RunCommand(const std::string& command) {
  auto dir = std::filesystem::temp_directory_path() /
             ("pipnn_toolchain_cmd_" + std::to_string(std::rand()));
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

void WriteFakeLlvmTree(const std::filesystem::path& root, bool include_clangxx = true) {
  std::filesystem::create_directories(root / "bin");
  std::filesystem::create_directories(root / "lib");
  MakeExecutable(root / "bin/clang", "#!/usr/bin/env bash\necho 'clang version 17.0.6'\n");
  if (include_clangxx) {
    MakeExecutable(root / "bin/clang++", "#!/usr/bin/env bash\necho 'clang version 17.0.6'\n");
  }
  MakeExecutable(root / "bin/llvm-config", "#!/usr/bin/env bash\necho '17.0.6'\n");
}

void WriteFakeMullTree(const std::filesystem::path& root) {
  std::filesystem::create_directories(root / "bin");
  std::filesystem::create_directories(root / "lib");
  MakeExecutable(root / "bin/mull-runner-17", "#!/usr/bin/env bash\necho 'mull-runner version 0.29.0'\n");
  MakeExecutable(root / "bin/mull-reporter-17", "#!/usr/bin/env bash\necho 'mull-reporter version 0.29.0'\n");
  MakeExecutable(root / "lib/mull-ir-frontend-17", "#!/usr/bin/env bash\necho 'frontend'\n");
}

CommandResult RunInstaller(const std::filesystem::path& llvm_source,
                           const std::filesystem::path& mull_source,
                           const std::filesystem::path& tool_root) {
  const std::filesystem::path repo_root = PIPNN_REPO_ROOT;
  const std::filesystem::path script = repo_root / "scripts/quality/ensure_remote_mull_toolchain.sh";
  std::string command = "env PIPNN_LLVM_SOURCE_DIR=" + ShellQuote(llvm_source) +
                        " PIPNN_MULL_SOURCE_DIR=" + ShellQuote(mull_source) +
                        " PIPNN_TOOLCHAIN_ROOT=" + ShellQuote(tool_root) + " bash " +
                        ShellQuote(script) + " --print-paths";
  return RunCommand(command);
}
}  // namespace

int main() {
  {
    auto dir = std::filesystem::temp_directory_path() / "pipnn_toolchain_fixture_pass";
    std::filesystem::remove_all(dir);
    auto llvm_source = dir / "llvm-source";
    auto mull_source = dir / "mull-source";
    auto tool_root = dir / "tools";
    std::filesystem::create_directories(dir);
    WriteFakeLlvmTree(llvm_source);
    WriteFakeMullTree(mull_source);

    auto result = RunInstaller(llvm_source, mull_source, tool_root);
    if (result.exit_code != 0) {
      std::cerr << "toolchain pass stdout:\n" << result.out << "\n";
      std::cerr << "toolchain pass stderr:\n" << result.err << "\n";
    }
    assert(result.exit_code == 0);
    assert(result.out.find("clang=" + (tool_root / "llvm/current/bin/clang").string()) != std::string::npos);
    assert(result.out.find("clangxx=" + (tool_root / "llvm/current/bin/clang++").string()) !=
           std::string::npos);
    assert(result.out.find("mull_runner=" + (tool_root / "mull/current/bin/mull-runner").string()) !=
           std::string::npos);
    assert(result.out.find("llvm_version=17.0.6") != std::string::npos);
    assert(result.out.find("mull_version=0.29.0") != std::string::npos);
    assert(std::filesystem::exists(tool_root / "llvm/current/bin/clang"));
    assert(std::filesystem::exists(tool_root / "llvm/current/bin/clang++"));
    assert(std::filesystem::exists(tool_root / "mull/current/bin/mull-runner"));
    const auto wrapper_contents = ReadAll(tool_root / "mull/current/bin/mull-runner");
    if (wrapper_contents.find("compat/lib/x86_64-linux-gnu") == std::string::npos) {
      std::cerr << "wrapper contents:\n" << wrapper_contents << "\n";
    }
    assert(wrapper_contents.find("compat/lib/x86_64-linux-gnu") != std::string::npos);
    assert(wrapper_contents.find("llvm/current/lib") == std::string::npos);

    auto version_result =
        RunCommand(ShellQuote(tool_root / "mull/current/bin/mull-runner") + " --version");
    assert(version_result.exit_code == 0);
    assert(version_result.out.find("0.29.0") != std::string::npos);
    std::filesystem::remove_all(dir);
  }

  {
    auto dir = std::filesystem::temp_directory_path() / "pipnn_toolchain_fixture_fail";
    std::filesystem::remove_all(dir);
    auto llvm_source = dir / "llvm-source";
    auto mull_source = dir / "mull-source";
    auto tool_root = dir / "tools";
    std::filesystem::create_directories(dir);
    WriteFakeLlvmTree(llvm_source, false);
    WriteFakeMullTree(mull_source);

    auto result = RunInstaller(llvm_source, mull_source, tool_root);
    if (result.err.find("missing required llvm executable") == std::string::npos) {
      std::cerr << "toolchain fail stdout:\n" << result.out << "\n";
      std::cerr << "toolchain fail stderr:\n" << result.err << "\n";
    }
    assert(result.exit_code == 1);
    assert(result.err.find("missing required llvm executable") != std::string::npos);
    std::filesystem::remove_all(dir);
  }

  return 0;
}
