#include "third_party/guest_memory_metrics_agent/providers/cgroup_provider.h"

#include <filesystem>  // NOLINT
#include <fstream>
#include <string>
#include <sstream>
#include <cstdint>
#include <system_error>

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

  std::error_code ec;
  auto it = fs::recursive_directory_iterator(
      base_path_, fs::directory_options::skip_permission_denied, ec);
  auto end = fs::recursive_directory_iterator();

  if (ec) return snapshot;

  while (it != end) {
    const auto& entry = *it;
    std::error_code file_ec;
    if (entry.is_regular_file(file_ec)) {
      std::string filename = entry.path().filename().string();
      if (filename == "memory.stat" || filename == "memory.current" ||
          filename == "memory.max") {
        std::ifstream file(entry.path());
        if (file.is_open()) {
          std::string rel_path = fs::relative(entry.path(), base_path_).string();

          if (filename == "memory.stat") {
            std::string line;
            while (std::getline(file, line)) {
              std::istringstream iss(line);
              std::string key;
              uint64_t value;
              if (iss >> key >> value) {
                snapshot.metrics["cgroup." + rel_path + "." + key] = value;
              }
            }
          } else {
            uint64_t value;
            if (file >> value) {
              snapshot.metrics["cgroup." + rel_path] = value;
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

  return snapshot;
}

}  // namespace guest_memory_metrics
