#include "third_party/guest_memory_metrics_agent/providers/cgroup_provider.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>  // NOLINT
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "third_party/absl/algorithm/container.h"
#include "third_party/absl/strings/match.h"
#include "third_party/absl/strings/numbers.h"
#include "third_party/absl/strings/str_cat.h"
#include "third_party/absl/strings/str_split.h"
#include "third_party/absl/time/clock.h"
#include "third_party/absl/time/time.h"
#include "third_party/guest_memory_metrics_agent/providers/metric_snapshot.h"

namespace guest_memory_metrics {

namespace fs = std::filesystem;

CgroupProvider::CgroupProvider(const std::string& base_path)
    : base_path_(base_path) {}

MetricSnapshot CgroupProvider::GetSnapshot() const {
  MetricSnapshot snapshot;
  snapshot.timestamp_ms = absl::ToUnixMillis(absl::Now());

  if (!fs::exists(base_path_)) return snapshot;

  auto is_blocked_controller = [](std::string_view name) {
    static constexpr std::array<std::string_view, 14> kBlockedControllers = {
        "cpu",     "cpuacct", "cpuset",      "blkio",           "devices",
        "freezer", "net_cls", "net_prio",    "perf_event",      "hugetlb",
        "pids",    "rdma",    "cpu,cpuacct", "net_cls,net_prio"};
    return absl::c_linear_search(kBlockedControllers, name);
  };

  std::error_code ec;
  bool is_v2_root = fs::exists(fs::path(base_path_) / "cgroup.controllers", ec);
  // If we are already inside a controller/hierarchy (e.g., pointed directly at
  // /sys/fs/cgroup/memory on V1), we don't want to blacklist depth 0 subdirectories.
  bool is_cgroup_node = fs::exists(fs::path(base_path_) / "cgroup.procs", ec) ||
                        fs::exists(fs::path(base_path_) / "tasks", ec);
  bool apply_blocklist = !is_v2_root && !is_cgroup_node;

  auto it = fs::recursive_directory_iterator(
      base_path_, fs::directory_options::skip_permission_denied, ec);
  auto end = fs::recursive_directory_iterator();

  while (it != end) {
    const auto& entry = *it;
    std::error_code entry_ec;

    if (entry.is_directory(entry_ec)) {
      if (it.depth() == 0) {
        const std::string& path_str = entry.path().native();
        size_t last_slash = path_str.find_last_of('/');
        std::string_view filename =
            (last_slash == std::string::npos)
                ? path_str
                : std::string_view(path_str).substr(last_slash + 1);
        if (apply_blocklist && is_blocked_controller(filename)) {
          it.disable_recursion_pending();
        }
      }
    } else if (entry.is_regular_file(entry_ec)) {
      const std::string& path_str = entry.path().native();
      size_t last_slash = path_str.find_last_of('/');
      std::string_view filename =
          (last_slash == std::string::npos)
              ? path_str
              : std::string_view(path_str).substr(last_slash + 1);

      if (filename == "memory.stat" || filename == "memory.current" ||
          filename == "memory.max" || filename == "memory.usage_in_bytes" ||
          filename == "memory.limit_in_bytes") {
        std::ifstream file(entry.path());
        if (file.is_open()) {
          std::string rel_path;
          if (path_str.length() > base_path_.length()) {
            rel_path = path_str.substr(base_path_.length());
            if (!rel_path.empty() && rel_path[0] == '/') {
              rel_path = rel_path.substr(1);
            }
          }

          if (filename == "memory.stat") {
            std::string line;
            while (std::getline(file, line)) {
              std::vector<std::string_view> tokens =
                  absl::StrSplit(line, absl::ByAnyChar(" \t"), absl::SkipEmpty());
              if (tokens.size() >= 2) {
                int64_t signed_value;
                if (absl::SimpleAtoi(tokens[1], &signed_value)) {
                  snapshot.metrics[absl::StrCat("cgroup.", rel_path, ".",
                                                tokens[0])] =
                      static_cast<uint64_t>(signed_value);
                }
              }
            }
          } else {
            int64_t signed_value;
            if (file >> signed_value) {
              snapshot.metrics[absl::StrCat("cgroup.", rel_path)] =
                  static_cast<uint64_t>(signed_value);
            }
          }
        }
      }
    }
    it.increment(ec);
    if (ec) {
      ec.clear();
    }
  }

  std::vector<std::string> keys_to_delete;
  for (const auto& kv : snapshot.metrics) {
    const std::string& key = kv.first;
    if (absl::EndsWith(key, "memory.current")) {
      absl::string_view base = absl::string_view(key).substr(0, key.length() - 14);
      std::string usage_key = absl::StrCat(base, "memory.usage_in_bytes");
      if (snapshot.metrics.contains(usage_key)) {
        keys_to_delete.push_back(usage_key);
      }
    }
    if (absl::EndsWith(key, "memory.max")) {
      absl::string_view base = absl::string_view(key).substr(0, key.length() - 10);
      std::string limit_key = absl::StrCat(base, "memory.limit_in_bytes");
      if (snapshot.metrics.contains(limit_key)) {
        keys_to_delete.push_back(limit_key);
      }
    }
  }

  for (const auto& k : keys_to_delete) {
    snapshot.metrics.erase(k);
  }

  return snapshot;
}

}  // namespace guest_memory_metrics
