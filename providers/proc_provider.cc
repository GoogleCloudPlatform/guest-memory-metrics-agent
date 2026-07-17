#include "third_party/guest_memory_metrics_agent/providers/proc_provider.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/absl/strings/numbers.h"
#include "third_party/absl/strings/str_cat.h"
#include "third_party/absl/strings/str_split.h"
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
      std::vector<std::string_view> tokens =
          absl::StrSplit(line, absl::ByAnyChar(" \t"), absl::SkipEmpty());
      if (tokens.size() >= 2) {
        std::string_view key = tokens[0];
        if (!key.empty() && key.back() == ':') {
          key.remove_suffix(1);
        }
        int64_t signed_value;
        if (absl::SimpleAtoi(tokens[1], &signed_value)) {
          if (tokens.size() >= 3 && tokens[2] == "kB") {
            signed_value *= 1024;
          }
          snapshot.metrics[absl::StrCat(prefix, key)] =
              static_cast<uint64_t>(signed_value);
        }
      }
    }
  };

  parse_file(base_path_ + "/meminfo", "proc.meminfo.");
  parse_file(base_path_ + "/vmstat", "proc.vmstat.");

  return snapshot;
}

}  // namespace guest_memory_metrics
