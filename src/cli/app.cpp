#include "cli/app.h"

#include "bench/runner.h"
#include "data/sift_reader.h"

#include <cstdlib>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <random>
#include <string>
#include <unordered_map>

namespace pipnn::cli {
namespace {
pipnn::Matrix MakeSynthetic(int n, int dim, int seed) {
  std::mt19937 gen(seed);
  std::normal_distribution<float> nd(0.0f, 1.0f);
  pipnn::Matrix m(n, pipnn::Vec(dim));
  for (auto& v : m) {
    for (auto& x : v) x = nd(gen);
  }
  return m;
}

void PrintHelp(std::ostream& out) {
  out << "Usage: pipnn --mode <pipnn|hnsw|vamana|pipnn_vamana> --dataset <synthetic|sift1m|file> --metric l2 "
         "--output <path> [--base <base.fvecs> --query <query.fvecs> --truth <gt.ivecs> "
         "--max-base N --max-query N --rbc-cmax N --rbc-fanout N --leader-frac F "
         "--max-leaders N --replicas N --leaf-k N --leaf-scan-cap N --max-degree N --hash-bits N --beam N "
         "--bidirected 0|1 --hnsw-m N --hnsw-ef-construction N --hnsw-ef-search N]\n";
}

bool NeedValue(const std::vector<std::string>& args, std::size_t& i, const std::string& option,
               std::string& value, std::ostream& err) {
  if (i + 1 >= args.size()) {
    err << "missing value for " << option << "\n";
    return false;
  }
  value = args[++i];
  return true;
}

bool FailInvalidValue(const std::string& option, const std::string& value, std::ostream& err) {
  err << "invalid value for " << option << ": " << value << "\n";
  return false;
}

bool ParseIntValue(const std::string& option, const std::string& value, int& out, std::ostream& err) {
  if (value.empty()) return FailInvalidValue(option, value, err);
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (errno == ERANGE || end != value.c_str() + value.size() ||
      parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
    return FailInvalidValue(option, value, err);
  }
  out = static_cast<int>(parsed);
  return true;
}

bool ParsePositiveIntValue(const std::string& option, const std::string& value, int& out,
                           std::ostream& err) {
  if (!ParseIntValue(option, value, out, err)) return false;
  if (out <= 0) return FailInvalidValue(option, value, err);
  return true;
}

bool ParseNonNegativeIntValue(const std::string& option, const std::string& value, int& out,
                              std::ostream& err) {
  if (!ParseIntValue(option, value, out, err)) return false;
  if (out < 0) return FailInvalidValue(option, value, err);
  return true;
}

bool ParseSizeValue(const std::string& option, const std::string& value, std::size_t& out,
                    std::ostream& err) {
  if (value.empty() || value.front() == '-') return FailInvalidValue(option, value, err);
  errno = 0;
  char* end = nullptr;
  const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);
  if (errno == ERANGE || end != value.c_str() + value.size() ||
      parsed > std::numeric_limits<std::size_t>::max()) {
    return FailInvalidValue(option, value, err);
  }
  out = static_cast<std::size_t>(parsed);
  return true;
}

bool ParseFloatValue(const std::string& option, const std::string& value, float& out, std::ostream& err) {
  if (value.empty()) return FailInvalidValue(option, value, err);
  errno = 0;
  char* end = nullptr;
  const float parsed = std::strtof(value.c_str(), &end);
  if (errno == ERANGE || end != value.c_str() + value.size()) {
    return FailInvalidValue(option, value, err);
  }
  out = parsed;
  return true;
}

bool ParseBoolValue(const std::string& option, const std::string& value, bool& out, std::ostream& err) {
  int parsed = 0;
  if (!ParseIntValue(option, value, parsed, err)) return false;
  if (parsed != 0 && parsed != 1) return FailInvalidValue(option, value, err);
  out = (parsed != 0);
  return true;
}
}  // namespace

int Run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  try {
    pipnn::RunnerConfig cfg;
    cfg.mode = "pipnn";
    cfg.dataset = "synthetic";
    cfg.output = "results/metrics.json";
    std::string base_path;
    std::string query_path;
    std::string truth_path;
    std::size_t max_base = 0;
    std::size_t max_query = 0;
    int rbc_cmax = 128;
    int rbc_fanout = 2;
    float leader_frac = 0.02f;
    int max_leaders = 1000;
    int replicas = 1;
    int leaf_k = 12;
    int leaf_scan_cap = 0;
    int max_degree = 32;
    int hash_bits = 12;
    int beam = 128;
    bool bidirected = true;
    int hnsw_m = 32;
    int hnsw_ef_construction = 200;
    int hnsw_ef_search = 0;
    using Handler = std::function<bool(const std::string&)>;
    const std::unordered_map<std::string, Handler> handlers = {
        {"--mode", [&](const std::string& value) {
           if (value != "pipnn" && value != "hnsw" && value != "vamana" &&
               value != "pipnn_vamana") {
             err << "unsupported mode: " << value << "\n";
             return false;
           }
           cfg.mode = value;
           return true;
         }},
        {"--dataset", [&](const std::string& value) {
           cfg.dataset = value;
           return true;
         }},
        {"--metric", [&](const std::string& value) {
           if (value != "l2") {
             err << "only l2 metric supported\n";
             return false;
           }
           return true;
         }},
        {"--output", [&](const std::string& value) {
           cfg.output = value;
           return true;
         }},
        {"--base", [&](const std::string& value) {
           base_path = value;
           return true;
         }},
        {"--query", [&](const std::string& value) {
           query_path = value;
           return true;
         }},
        {"--truth", [&](const std::string& value) {
           truth_path = value;
           return true;
         }},
        {"--max-base", [&](const std::string& value) {
           return ParseSizeValue("--max-base", value, max_base, err);
         }},
        {"--max-query", [&](const std::string& value) {
           return ParseSizeValue("--max-query", value, max_query, err);
         }},
        {"--rbc-cmax", [&](const std::string& value) {
           return ParseIntValue("--rbc-cmax", value, rbc_cmax, err);
         }},
        {"--rbc-fanout", [&](const std::string& value) {
           return ParseIntValue("--rbc-fanout", value, rbc_fanout, err);
         }},
        {"--leader-frac", [&](const std::string& value) {
           return ParseFloatValue("--leader-frac", value, leader_frac, err);
         }},
        {"--max-leaders", [&](const std::string& value) {
           return ParseIntValue("--max-leaders", value, max_leaders, err);
         }},
        {"--replicas", [&](const std::string& value) {
           return ParseIntValue("--replicas", value, replicas, err);
         }},
        {"--leaf-k", [&](const std::string& value) {
           return ParseIntValue("--leaf-k", value, leaf_k, err);
         }},
        {"--leaf-scan-cap", [&](const std::string& value) {
           return ParseIntValue("--leaf-scan-cap", value, leaf_scan_cap, err);
         }},
        {"--max-degree", [&](const std::string& value) {
           return ParseIntValue("--max-degree", value, max_degree, err);
         }},
        {"--hash-bits", [&](const std::string& value) {
           return ParseIntValue("--hash-bits", value, hash_bits, err);
         }},
        {"--beam", [&](const std::string& value) {
           return ParseIntValue("--beam", value, beam, err);
         }},
        {"--bidirected", [&](const std::string& value) {
          return ParseBoolValue("--bidirected", value, bidirected, err);
         }},
        {"--hnsw-m", [&](const std::string& value) {
           return ParsePositiveIntValue("--hnsw-m", value, hnsw_m, err);
         }},
        {"--hnsw-ef-construction", [&](const std::string& value) {
           return ParsePositiveIntValue("--hnsw-ef-construction", value, hnsw_ef_construction, err);
         }},
        {"--hnsw-ef-search", [&](const std::string& value) {
           return ParseNonNegativeIntValue("--hnsw-ef-search", value, hnsw_ef_search, err);
         }},
    };

    for (std::size_t i = 0; i < args.size(); ++i) {
      const std::string& a = args[i];
      std::string value;
      if (a == "--help") {
        PrintHelp(out);
        return 0;
      }
      const auto it = handlers.find(a);
      if (it == handlers.end()) {
        err << "unknown argument: " << a << "\n";
        return 1;
      }
      if (!NeedValue(args, i, a, value, err)) return 1;
      if (!it->second(value)) return 1;
    }

    pipnn::Matrix base;
    pipnn::Matrix queries;
    std::vector<std::vector<int>> truth;
    if (cfg.dataset == "synthetic") {
      base = MakeSynthetic(2000, 32, 42);
      queries = MakeSynthetic(100, 32, 4242);
    } else if (cfg.dataset == "sift1m") {
      if (base_path.empty() || query_path.empty()) {
        err << "sift1m requires --base <base.fvecs> and --query <query.fvecs>\n";
        return 1;
      }
      base = pipnn::data::LoadFloatVectors(base_path, max_base);
      queries = pipnn::data::LoadFloatVectors(query_path, max_query);
      if (!truth_path.empty()) truth = pipnn::data::LoadIntVectors(truth_path, queries.size());
    } else if (cfg.dataset == "file") {
      if (base_path.empty() || query_path.empty()) {
        err << "file requires --base <vectors> and --query <vectors>\n";
        return 1;
      }
      base = pipnn::data::LoadFloatVectors(base_path, max_base);
      queries = pipnn::data::LoadFloatVectors(query_path, max_query);
      if (!truth_path.empty()) truth = pipnn::data::LoadIntVectors(truth_path, queries.size());
    } else {
      err << "unsupported dataset: " << cfg.dataset << "\n";
      return 1;
    }

    pipnn::PipnnBuildParams bp;
    bp.rbc.cmax = rbc_cmax;
    bp.rbc.fanout = rbc_fanout;
    bp.rbc.leader_frac = leader_frac;
    bp.rbc.max_leaders = max_leaders;
    bp.replicas = replicas;
    bp.leaf_k = leaf_k;
    bp.leaf_scan_cap = leaf_scan_cap;
    bp.bidirected = bidirected;
    bp.hashprune.max_degree = max_degree;
    bp.hashprune.hash_bits = hash_bits;

    pipnn::SearchParams sp;
    sp.beam = beam;
    sp.topk = 10;
    cfg.hnsw.m = hnsw_m;
    cfg.hnsw.ef_construction = hnsw_ef_construction;
    cfg.hnsw.ef_search = hnsw_ef_search;

    if (std::getenv("PIPNN_ECHO_CONFIG") != nullptr) {
      out << "cfg mode=" << cfg.mode << " dataset=" << cfg.dataset << " max_base=" << max_base
          << " max_query=" << max_query << " cmax=" << bp.rbc.cmax << " fanout=" << bp.rbc.fanout
          << " leader_frac=" << bp.rbc.leader_frac << " max_leaders=" << bp.rbc.max_leaders
          << " replicas=" << bp.replicas
          << " leaf_k=" << bp.leaf_k << " leaf_scan_cap=" << bp.leaf_scan_cap
          << " max_degree=" << bp.hashprune.max_degree
          << " hash_bits=" << bp.hashprune.hash_bits << " beam=" << sp.beam
          << " bidirected=" << bp.bidirected << " hnsw_m=" << cfg.hnsw.m
          << " hnsw_ef_construction=" << cfg.hnsw.ef_construction
          << " hnsw_ef_search=" << cfg.hnsw.ef_search << "\n";
    }

    auto metrics = pipnn::RunBenchmark(cfg, base, queries, truth, bp, sp);
    auto json = pipnn::ToJson(metrics);

    const std::filesystem::path output_path(cfg.output);
    const auto parent = output_path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::ofstream file(output_path);
    file << json;
    out << json;
    return 0;
  } catch (const std::exception& ex) {
    err << ex.what() << "\n";
    return 1;
  }
}
}  // namespace pipnn::cli
