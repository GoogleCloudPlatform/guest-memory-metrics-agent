#include "third_party/guest_memory_metrics_agent/providers/proc_provider.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "third_party/absl/time/clock.h"
#include "third_party/absl/time/time.h"
#include "third_party/guest_memory_metrics_agent/providers/metric_snapshot.h"

namespace guest_memory_metrics {

ProcProvider::ProcProvider(const std::string& base_path)
    : base_path_(base_path) {}

MetricSnapshot ProcProvider::GetSnapshot() const {
  MetricSnapshot snapshot;
  snapshot.timestamp_ms = absl::ToUnixMillis(absl::Now());

  auto parse_file = [&snapshot](const std::string& path,
                                const std::string& prefix) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
      std::istringstream iss(line);
      std::string key;
      uint64_t value;
      std::string unit;
      if (iss >> key >> value) {
        if (!key.empty() && key.back() == ':') {
          key.pop_back();
        }
        if (iss >> unit) {
          if (unit == "kB") value *= 1024;
        }
        snapshot.metrics[prefix + key] = value;
      }
    }
  };

  parse_file(base_path_ + "/meminfo", "proc.meminfo.");
  parse_file(base_path_ + "/vmstat", "proc.vmstat.");

  return snapshot;
}

}  // namespace guest_memory_metrics
