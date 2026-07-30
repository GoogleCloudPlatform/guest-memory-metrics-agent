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

#include "providers/proc_provider.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "providers/metric_snapshot.h"

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
    std::vector<std::string_view> tokens;
    while (std::getline(file, line)) {
      tokens.clear();
      for (absl::string_view token :
           absl::StrSplit(line, absl::ByAnyChar(" \t"), absl::SkipEmpty())) {
        tokens.push_back(token);
      }
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
