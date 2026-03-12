#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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
             ("pipnn_mutation_cmd_" + std::to_string(std::rand()));
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

CommandResult RunValidator(const std::filesystem::path& local_probe,
                           const std::filesystem::path& remote_probe,
                           const std::filesystem::path& st_report,
                           const std::filesystem::path& repro_manifest,
                           const std::filesystem::path& score_path = {},
                           const std::filesystem::path& report_path = {},
                           const std::filesystem::path& survivors_path = {}) {
  const std::filesystem::path repo_root = PIPNN_REPO_ROOT;
  const std::filesystem::path validator = repo_root / "scripts/validate_mutation_evidence.py";
  std::string command = "python3 " + ShellQuote(validator) + " --local-probe " +
                        ShellQuote(local_probe) + " --remote-probe " + ShellQuote(remote_probe) +
                        " --st-report " + ShellQuote(st_report) + " --repro-manifest " +
                        ShellQuote(repro_manifest);
  if (!score_path.empty()) {
    command += " --mutation-score " + ShellQuote(score_path);
  }
  if (!report_path.empty()) {
    command += " --mutation-report " + ShellQuote(report_path);
  }
  if (!survivors_path.empty()) {
    command += " --mutation-survivors " + ShellQuote(survivors_path);
  }
  return RunCommand(command);
}

void WriteProbe(const std::filesystem::path& path, const std::string& status) {
  std::ofstream out(path);
  out << "$ mull-runner --help\n";
  out << "status=" << status << "\n";
  out << "exit_code=" << (status == "blocked" ? 127 : 0) << "\n";
}

void WriteScoredManifest(const std::filesystem::path& path, bool include_score = true) {
  std::ofstream out(path);
  out << "{\n"
      << "  \"quality_evidence\": {\n"
      << "    \"mutation_probe\": {\n"
      << "      \"status\": \"scored\",\n";
  if (include_score) {
    out << "      \"score\": 82,\n";
  }
  out << "      \"score_path\": \"results/st/mutation_score.txt\",\n"
      << "      \"report\": \"results/st/mutation_report.json\",\n"
      << "      \"survivors\": \"results/st/mutation_survivors.txt\",\n"
      << "      \"tool_versions\": {\n"
      << "        \"llvm\": \"18.1.8\",\n"
      << "        \"mull\": \"0.19.1\"\n"
      << "      }\n"
      << "    }\n"
      << "  }\n"
      << "}\n";
}

void WriteScoreArtifacts(const std::filesystem::path& score_path,
                         const std::filesystem::path& report_path,
                         const std::filesystem::path& survivors_path) {
  std::ofstream(score_path) << "82.0\n";
  std::ofstream(report_path)
      << "{\n"
      << "  \"totals\": {\n"
      << "    \"mutation_score\": 82.0,\n"
      << "    \"mutants_total\": 50,\n"
      << "    \"killed\": 41,\n"
      << "    \"survived\": 9\n"
      << "  }\n"
      << "}\n";
  std::ofstream(survivors_path) << "src/core/hashprune.cpp:75:67 cxx_lt_to_ge survived\n";
}
}  // namespace

int main() {
  const std::filesystem::path repo_root = PIPNN_REPO_ROOT;

  {
    auto dir = std::filesystem::temp_directory_path() / "pipnn_mutation_fixture_pass";
    std::filesystem::create_directories(dir);
    auto local_probe = dir / "local.txt";
    auto remote_probe = dir / "remote.txt";
    auto st_report = dir / "st.md";
    auto repro_manifest = dir / "repro.json";
    WriteProbe(local_probe, "blocked");
    WriteProbe(remote_probe, "blocked");
    std::ofstream(st_report)
        << "Mutation remains blocked-state evidence. See mutation_probe_local.txt and "
           "mutation_probe_remote.txt. Follow-up: install mull-runner on remote x86. "
           "Verdict: Conditional-Go.\n";
    std::ofstream(repro_manifest)
        << "{\n"
        << "  \"quality_evidence\": {\n"
        << "    \"mutation_probe\": {\n"
        << "      \"status\": \"blocked\",\n"
        << "      \"local_probe\": \"results/st/mutation_probe_local.txt\",\n"
        << "      \"remote_probe\": \"results/st/remote/mutation_probe_remote.txt\",\n"
        << "      \"follow_up\": \"install mull-runner on remote x86\"\n"
        << "    }\n"
        << "  }\n"
        << "}\n";
    auto result = RunValidator(local_probe, remote_probe, st_report, repro_manifest);
    if (result.exit_code != 0) {
      std::cerr << "fixture pass stdout:\n" << result.out << "\n";
      std::cerr << "fixture pass stderr:\n" << result.err << "\n";
    }
    assert(result.exit_code == 0);
    assert(result.out.find("mutation_status=blocked") != std::string::npos);
    std::filesystem::remove(local_probe);
    std::filesystem::remove(remote_probe);
    std::filesystem::remove(st_report);
    std::filesystem::remove(repro_manifest);
    std::filesystem::remove(dir);
  }

  {
    auto dir = std::filesystem::temp_directory_path() / "pipnn_mutation_fixture_fail";
    std::filesystem::create_directories(dir);
    auto local_probe = dir / "local.txt";
    auto remote_probe = dir / "remote.txt";
    auto st_report = dir / "st.md";
    auto repro_manifest = dir / "repro.json";
    WriteProbe(local_probe, "blocked");
    WriteProbe(remote_probe, "blocked");
    std::ofstream(st_report) << "Mutation note missing.\n";
    std::ofstream(repro_manifest)
        << "{\n"
        << "  \"quality_evidence\": {\n"
        << "    \"mutation_probe\": {\n"
        << "      \"status\": \"blocked\",\n"
        << "      \"local_probe\": \"results/st/mutation_probe_local.txt\",\n"
        << "      \"remote_probe\": \"results/st/remote/mutation_probe_remote.txt\",\n"
        << "      \"follow_up\": \"install mull-runner on remote x86\"\n"
        << "    }\n"
        << "  }\n"
        << "}\n";
    auto result = RunValidator(local_probe, remote_probe, st_report, repro_manifest);
    if (result.err.find("blocked mutation evidence missing from ST report") == std::string::npos) {
      std::cerr << "fixture fail stdout:\n" << result.out << "\n";
      std::cerr << "fixture fail stderr:\n" << result.err << "\n";
    }
    assert(result.exit_code == 1);
    assert(result.err.find("blocked mutation evidence missing from ST report") != std::string::npos);
    std::filesystem::remove(local_probe);
    std::filesystem::remove(remote_probe);
    std::filesystem::remove(st_report);
    std::filesystem::remove(repro_manifest);
    std::filesystem::remove(dir);
  }

  {
    auto dir = std::filesystem::temp_directory_path() / "pipnn_mutation_fixture_scored";
    std::filesystem::create_directories(dir);
    auto local_probe = dir / "missing-local.txt";
    auto remote_probe = dir / "missing-remote.txt";
    auto st_report = dir / "st.md";
    auto repro_manifest = dir / "repro.json";
    auto score_path = dir / "mutation_score.txt";
    auto report_path = dir / "mutation_report.json";
    auto survivors_path = dir / "mutation_survivors.txt";
    std::ofstream(st_report)
        << "Mutation score 82.0 recorded from remote LLVM + Mull toolchain. "
           "LLVM 18.1.8. Mull 0.19.1. Score artifact: mutation_score.txt. "
           "Report: mutation_report.json. Survivors: mutation_survivors.txt. Verdict: Go.\n";
    WriteScoreArtifacts(score_path, report_path, survivors_path);
    WriteScoredManifest(repro_manifest);
    auto result =
        RunValidator(local_probe, remote_probe, st_report, repro_manifest, score_path, report_path,
                     survivors_path);
    if (result.exit_code != 0) {
      std::cerr << "fixture scored stdout:\n" << result.out << "\n";
      std::cerr << "fixture scored stderr:\n" << result.err << "\n";
    }
    assert(result.exit_code == 0);
    assert(result.out.find("mutation_status=scored") != std::string::npos);
    std::filesystem::remove(score_path);
    std::filesystem::remove(report_path);
    std::filesystem::remove(survivors_path);
    std::filesystem::remove(st_report);
    std::filesystem::remove(repro_manifest);
    std::filesystem::remove(dir);
  }

  {
    auto dir = std::filesystem::temp_directory_path() / "pipnn_mutation_fixture_scored_fail";
    std::filesystem::create_directories(dir);
    auto local_probe = dir / "missing-local.txt";
    auto remote_probe = dir / "missing-remote.txt";
    auto st_report = dir / "st.md";
    auto repro_manifest = dir / "repro.json";
    auto score_path = dir / "mutation_score.txt";
    auto report_path = dir / "mutation_report.json";
    auto survivors_path = dir / "mutation_survivors.txt";
    std::ofstream(st_report)
        << "Mutation score 82.0 recorded from remote LLVM + Mull toolchain. "
           "Score artifact: mutation_score.txt. Report: mutation_report.json. "
           "Survivors: mutation_survivors.txt. Verdict: Go.\n";
    WriteScoreArtifacts(score_path, report_path, survivors_path);
    WriteScoredManifest(repro_manifest, false);
    auto result =
        RunValidator(local_probe, remote_probe, st_report, repro_manifest, score_path, report_path,
                     survivors_path);
    if (result.err.find("mutation score missing from reproducibility manifest") == std::string::npos) {
      std::cerr << "fixture scored fail stdout:\n" << result.out << "\n";
      std::cerr << "fixture scored fail stderr:\n" << result.err << "\n";
    }
    assert(result.exit_code == 1);
    assert(result.err.find("mutation score missing from reproducibility manifest") != std::string::npos);
    std::filesystem::remove(score_path);
    std::filesystem::remove(report_path);
    std::filesystem::remove(survivors_path);
    std::filesystem::remove(st_report);
    std::filesystem::remove(repro_manifest);
    std::filesystem::remove(dir);
  }

  return 0;
}
