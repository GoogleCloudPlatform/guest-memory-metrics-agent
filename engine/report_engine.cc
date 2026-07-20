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

#include "engine/report_engine.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>  // NOLINT
#include <set>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"

namespace guest_memory_metrics {

void ReportEngine::GenerateReport(const std::string& input_path,
                                  const std::string& start_str,
                                  const std::string& end_str) {
  // Convert start_str and end_str to int64_t.
  int64_t start_ts = 0;
  int64_t end_ts = 0;
  if (!absl::SimpleAtoi(start_str, &start_ts) ||
      !absl::SimpleAtoi(end_str, &end_ts)) {
    std::cerr << "Failed to parse start or end timestamp. Must be numeric."
              << std::endl;
    return;
  }

  std::ifstream file(input_path);
  if (!file.is_open()) {
    std::cerr << "Failed to open input file: " << input_path << std::endl;
    return;
  }

  // Regex to match: {"timestamp": 123, "source": "...", "metric": "...",
  // "value": 456}
  std::regex line_regex(
      R"regex(\{"timestamp":\s*(\d+),\s*"source":\s*"([^"]+)",\s*"metric":\s*"([^"]+)",\s*"value":\s*(\d+)\})regex");
  std::smatch match;

  std::string line;
  int64_t closest_start_diff = -1;
  int64_t closest_end_diff = -1;

  struct Snapshot {
    int64_t timestamp;
    std::map<std::string, uint64_t> metrics;
  };

  std::vector<Snapshot> snapshots;
  Snapshot current_snapshot;
  current_snapshot.timestamp = -1;

  while (std::getline(file, line)) {
    if (std::regex_search(line, match, line_regex)) {
      int64_t ts = 0;
      uint64_t value = 0;
      if (!absl::SimpleAtoi(match[1].str(), &ts) ||
          !absl::SimpleAtoi(match[4].str(), &value)) {
        continue;
      }
      std::string source = match[2];
      std::string metric = match[3];

      std::string full_metric = source + "." + metric;

      if (ts != current_snapshot.timestamp) {
        if (current_snapshot.timestamp != -1) {
          snapshots.push_back(current_snapshot);
        }
        current_snapshot.timestamp = ts;
        current_snapshot.metrics.clear();
      }
      current_snapshot.metrics[full_metric] = value;
    }
  }
  if (current_snapshot.timestamp != -1) {
    snapshots.push_back(current_snapshot);
  }

  if (snapshots.empty()) {
    std::cerr << "No valid snapshots found in log." << std::endl;
    return;
  }

  Snapshot* best_start = &snapshots[0];
  Snapshot* best_end = &snapshots[0];

  for (auto& snap : snapshots) {
    int64_t s_diff = std::abs(snap.timestamp - start_ts);
    int64_t e_diff = std::abs(snap.timestamp - end_ts);

    if (closest_start_diff == -1 || s_diff < closest_start_diff) {
      closest_start_diff = s_diff;
      best_start = &snap;
    }
    if (closest_end_diff == -1 || e_diff < closest_end_diff) {
      closest_end_diff = e_diff;
      best_end = &snap;
    }
  }

  std::set<std::string> all_metrics;
  for (const auto& [metric, _] : best_start->metrics) all_metrics.insert(metric);
  for (const auto& [metric, _] : best_end->metrics) all_metrics.insert(metric);

  size_t max_metric_len = 6;  // Length of "Metric"
  for (const auto& metric : all_metrics) {
    uint64_t start_val = 0;
    uint64_t end_val = 0;
    if (best_start->metrics.count(metric)) {
      start_val = best_start->metrics.at(metric);
    }
    if (best_end->metrics.count(metric)) {
      end_val = best_end->metrics.at(metric);
    }
    int64_t delta =
        static_cast<int64_t>(end_val) - static_cast<int64_t>(start_val);
    if (delta != 0) {
      if (metric.length() > max_metric_len) {
        max_metric_len = metric.length();
      }
    }
  }

  int metric_col_width = static_cast<int>(max_metric_len) + 5;
  int total_width = metric_col_width + 60;

  std::cout << "Delta Report (" << best_start->timestamp << " to "
            << best_end->timestamp << ")\n";
  std::cout << std::string(total_width, '-') << "\n";
  std::cout << std::left << std::setw(metric_col_width) << "Metric"
            << std::right << std::setw(15) << "Start" << std::setw(15) << "End"
            << std::setw(15) << "Delta" << std::setw(15) << "Pct Change" << "\n";
  std::cout << std::string(total_width, '-') << "\n";

  for (const auto& metric : all_metrics) {
    uint64_t start_val = 0;
    uint64_t end_val = 0;
    if (best_start->metrics.count(metric)) {
      start_val = best_start->metrics.at(metric);
    }
    if (best_end->metrics.count(metric)) {
      end_val = best_end->metrics.at(metric);
    }

    int64_t delta =
        static_cast<int64_t>(end_val) - static_cast<int64_t>(start_val);

    if (delta != 0) {
      std::cout << std::left << std::setw(metric_col_width) << metric
                << std::right << std::setw(15) << start_val << std::setw(15)
                << end_val << std::setw(15) << delta;

      if (start_val > 0) {
        double pct_change = (static_cast<double>(delta) / start_val) * 100.0;
        std::cout << std::setw(14) << std::fixed << std::setprecision(2) << pct_change << "%";
      } else {
        std::cout << std::setw(15) << "N/A";
      }
      std::cout << "\n";
    }
  }
}

}  // namespace guest_memory_metrics
