#include "bench/runner.h"
#include "data/sift_reader.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

namespace {
pipnn::Matrix MakeSynthetic(int n, int dim, int seed) {
  std::mt19937 gen(seed);
  std::normal_distribution<float> nd(0.0f, 1.0f);
  pipnn::Matrix m(n, pipnn::Vec(dim));
  for (auto& v : m) for (auto& x : v) x = nd(gen);
  return m;
}

void PrintHelp() {
  std::cout << "Usage: pipnn --mode <pipnn|hnsw> --dataset <synthetic|sift1m> --metric l2 "
               "--output <path> [--base <base.fvecs> --query <query.fvecs> --truth <gt.ivecs> "
               "--max-base N --max-query N --rbc-cmax N --rbc-fanout N --leader-frac F "
               "--max-leaders N --replicas N --leaf-k N --leaf-scan-cap N --max-degree N --hash-bits N --beam N --bidirected 0|1]\n";
}
}  // namespace

int main(int argc, char** argv) {
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

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--help") {
      PrintHelp();
      return 0;
    }
    if (a == "--mode" && i + 1 < argc) cfg.mode = argv[++i];
    else if (a == "--dataset" && i + 1 < argc) cfg.dataset = argv[++i];
    else if (a == "--metric" && i + 1 < argc) {
      std::string metric = argv[++i];
      if (metric != "l2") {
        std::cerr << "only l2 metric supported\n";
        return 1;
      }
    } else if (a == "--output" && i + 1 < argc) cfg.output = argv[++i];
    else if (a == "--base" && i + 1 < argc) base_path = argv[++i];
    else if (a == "--query" && i + 1 < argc) query_path = argv[++i];
    else if (a == "--truth" && i + 1 < argc) truth_path = argv[++i];
    else if (a == "--max-base" && i + 1 < argc) max_base = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (a == "--max-query" && i + 1 < argc) max_query = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (a == "--rbc-cmax" && i + 1 < argc) rbc_cmax = std::stoi(argv[++i]);
    else if (a == "--rbc-fanout" && i + 1 < argc) rbc_fanout = std::stoi(argv[++i]);
    else if (a == "--leader-frac" && i + 1 < argc) leader_frac = std::stof(argv[++i]);
    else if (a == "--max-leaders" && i + 1 < argc) max_leaders = std::stoi(argv[++i]);
    else if (a == "--replicas" && i + 1 < argc) replicas = std::stoi(argv[++i]);
    else if (a == "--leaf-k" && i + 1 < argc) leaf_k = std::stoi(argv[++i]);
    else if (a == "--leaf-scan-cap" && i + 1 < argc) leaf_scan_cap = std::stoi(argv[++i]);
    else if (a == "--max-degree" && i + 1 < argc) max_degree = std::stoi(argv[++i]);
    else if (a == "--hash-bits" && i + 1 < argc) hash_bits = std::stoi(argv[++i]);
    else if (a == "--beam" && i + 1 < argc) beam = std::stoi(argv[++i]);
    else if (a == "--bidirected" && i + 1 < argc) bidirected = (std::stoi(argv[++i]) != 0);
  }

  pipnn::Matrix base;
  pipnn::Matrix queries;
  std::vector<std::vector<int>> truth;
  if (cfg.dataset == "synthetic") {
    base = MakeSynthetic(2000, 32, 42);
    queries = MakeSynthetic(100, 32, 4242);
  } else if (cfg.dataset == "sift1m") {
    if (base_path.empty() || query_path.empty()) {
      std::cerr << "sift1m requires --base <base.fvecs> and --query <query.fvecs>\n";
      return 1;
    }
    base = pipnn::data::LoadFvecs(base_path, max_base);
    queries = pipnn::data::LoadFvecs(query_path, max_query);
    if (!truth_path.empty()) {
      truth = pipnn::data::LoadIvecs(truth_path, queries.size());
    }
  } else {
    std::cerr << "unsupported dataset: " << cfg.dataset << "\n";
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

  if (std::getenv("PIPNN_ECHO_CONFIG") != nullptr) {
    std::cout << "cfg mode=" << cfg.mode << " dataset=" << cfg.dataset << " max_base=" << max_base
              << " max_query=" << max_query << " cmax=" << bp.rbc.cmax << " fanout=" << bp.rbc.fanout
              << " leader_frac=" << bp.rbc.leader_frac << " max_leaders=" << bp.rbc.max_leaders
              << " replicas=" << bp.replicas
              << " leaf_k=" << bp.leaf_k << " leaf_scan_cap=" << bp.leaf_scan_cap
              << " max_degree=" << bp.hashprune.max_degree
              << " hash_bits=" << bp.hashprune.hash_bits << " beam=" << sp.beam
              << " bidirected=" << bp.bidirected << "\n";
  }

  auto metrics = pipnn::RunBenchmark(cfg, base, queries, truth, bp, sp);
  auto json = pipnn::ToJson(metrics);

  std::filesystem::create_directories(std::filesystem::path(cfg.output).parent_path());
  std::ofstream out(cfg.output);
  out << json;
  out.close();
  std::cout << json;
  return 0;
}
