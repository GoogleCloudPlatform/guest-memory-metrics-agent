// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "providers/numa_provider.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>  // NOLINT
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>  // NOLINT
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "providers/metric_snapshot.h"

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
      const std::string& path_str = entry.path().native();
      size_t last_slash = path_str.find_last_of('/');
      std::string_view dir_name =
          (last_slash == std::string::npos)
              ? path_str
              : std::string_view(path_str).substr(last_slash + 1);

      if (absl::StartsWith(dir_name, "node")) {
        std::string meminfo_path = absl::StrCat(path_str, "/meminfo");
        std::ifstream file(meminfo_path);
        if (file.is_open()) {
          std::string line;
          std::vector<std::string_view> tokens;
          while (std::getline(file, line)) {
            tokens.clear();
            for (absl::string_view token : absl::StrSplit(
                     line, absl::ByAnyChar(" \t"), absl::SkipEmpty())) {
              tokens.push_back(token);
            }
            if (tokens.empty()) continue;

            size_t val_idx = 0;
            std::string_view key;

            if (tokens[0] == "Node") {
              if (tokens.size() < 4) continue;
              // Node <num> <key> <value>
              key = tokens[2];
              val_idx = 3;
            } else {
              if (tokens.size() < 2) continue;
              // <key> <value>
              key = tokens[0];
              val_idx = 1;
            }

            int64_t signed_value;
            if (absl::SimpleAtoi(tokens[val_idx], &signed_value)) {
              if (!key.empty() && key.back() == ':') {
                key.remove_suffix(1);
              }
              if (tokens.size() > val_idx + 1) {
                std::string_view unit = tokens[val_idx + 1];
                if (unit == "kB" || unit == "kb") {
                  signed_value *= 1024;
                }
              }
              snapshot.metrics[absl::StrCat("numa.", dir_name, ".", key)] =
                  static_cast<uint64_t>(signed_value);
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
