#include "third_party/guest_memory_metrics_agent/providers/numa_provider.h"

#include <cstdint>
#include <filesystem>  // NOLINT
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>  // NOLINT

#include "third_party/absl/strings/match.h"
#include "third_party/absl/strings/str_cat.h"
#include "third_party/absl/time/clock.h"
#include "third_party/absl/time/time.h"
#include "third_party/guest_memory_metrics_agent/providers/metric_snapshot.h"

namespace guest_memory_metrics {

namespace fs = std::filesystem;

NumaProvider::NumaProvider(const std::string& base_path)
    : base_path_(base_path) {}

MetricSnapshot NumaProvider::GetSnapshot() const {
  MetricSnapshot snapshot;
  snapshot.timestamp_ms = absl::ToUnixMillis(absl::Now());

  if (!fs::exists(base_path_)) return snapshot;

  std::error_code ec;
  fs::directory_iterator iter(
      base_path_, fs::directory_options::skip_permission_denied, ec);
  if (ec) return snapshot;

  while (iter != fs::directory_iterator()) {
    const auto& entry = *iter;
    std::error_code dir_ec;
    if (entry.is_directory(dir_ec)) {
      std::string dir_name = entry.path().filename().string();
      if (absl::StartsWith(dir_name, "node")) {
        std::string meminfo_path = (entry.path() / "meminfo").string();
        std::ifstream file(meminfo_path);
        if (file.is_open()) {
          std::string line;
          while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string first_word;
            std::string key;
            uint64_t value;
            std::string unit;

            if (iss >> first_word) {
              if (first_word == "Node") {
                std::string node_num;
                if (!(iss >> node_num >> key)) {
                  continue;
                }
              } else {
                key = first_word;
              }

              if (iss >> value) {
                if (!key.empty() && key.back() == ':') {
                  key.pop_back();
                }
                if (iss >> unit) {
                  if (unit == "kB" || unit == "kb") {
                    value *= 1024;
                  }
                }
                snapshot.metrics[absl::StrCat("numa.", dir_name, ".", key)] =
                    value;
              }
            }
          }
        }
      }
    }
    iter.increment(ec);
    if (ec) {
      ec.clear();
    }
  }

  return snapshot;
}

}  // namespace guest_memory_metrics
