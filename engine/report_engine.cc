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
#include <set>
#include <string>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

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

  std::string line;
  int64_t closest_start_diff = -1;
  int64_t closest_end_diff = -1;

  struct Snapshot {
    int64_t timestamp;
    std::map<std::string, uint64_t> metrics;
  };

  Snapshot current_snapshot;
  current_snapshot.timestamp = -1;

  Snapshot best_start_snap;
  Snapshot best_end_snap;
  bool has_snapshots = false;

  auto process_snapshot = [&]() {
    if (current_snapshot.timestamp == -1) return;
    has_snapshots = true;

    int64_t s_diff = std::abs(current_snapshot.timestamp - start_ts);
    int64_t e_diff = std::abs(current_snapshot.timestamp - end_ts);

    if (closest_start_diff == -1 || s_diff < closest_start_diff) {
      closest_start_diff = s_diff;
      best_start_snap = current_snapshot;
    }
    if (closest_end_diff == -1 || e_diff < closest_end_diff) {
      closest_end_diff = e_diff;
      best_end_snap = current_snapshot;
    }
  };

  while (std::getline(file, line)) {
    absl::string_view sv(line);

    size_t ts_pos = sv.find("\"timestamp\": ");
    if (ts_pos == absl::string_view::npos) continue;
    ts_pos += 13;

    size_t src_pos = sv.find(", \"source\": \"", ts_pos);
    if (src_pos == absl::string_view::npos) continue;
    absl::string_view ts_str = sv.substr(ts_pos, src_pos - ts_pos);
    src_pos += 13;

    size_t metric_pos = sv.find("\", \"metric\": \"", src_pos);
    if (metric_pos == absl::string_view::npos) continue;
    absl::string_view source_str = sv.substr(src_pos, metric_pos - src_pos);
    metric_pos += 14;

    size_t val_pos = sv.find("\", \"value\": ", metric_pos);
    if (val_pos == absl::string_view::npos) continue;
    absl::string_view metric_str = sv.substr(metric_pos, val_pos - metric_pos);
    val_pos += 12;

    size_t end_pos = sv.find('}', val_pos);
    if (end_pos == absl::string_view::npos) continue;
    absl::string_view val_str = sv.substr(val_pos, end_pos - val_pos);

    int64_t ts = 0;
    uint64_t value = 0;
    if (!absl::SimpleAtoi(ts_str, &ts) || !absl::SimpleAtoi(val_str, &value)) {
      continue;
    }

    // Use absl::StrCat to concatenate string_views with a single allocation
    // of exact size.
    std::string full_metric = absl::StrCat(source_str, ".", metric_str);

    if (ts != current_snapshot.timestamp) {
      process_snapshot();
      current_snapshot.timestamp = ts;
      current_snapshot.metrics.clear();
    }
    current_snapshot.metrics[full_metric] = value;
  }
  process_snapshot();

  if (!has_snapshots) {
    std::cerr << "No valid snapshots found in log." << std::endl;
    return;
  }

  Snapshot* best_start = &best_start_snap;
  Snapshot* best_end = &best_end_snap;

  std::set<std::string> all_metrics;
  for (const auto& [metric, _] : best_start->metrics) all_metrics.insert(metric);
  for (const auto& [metric, _] : best_end->metrics) all_metrics.insert(metric);

  size_t max_metric_len = 6;  // Length of "Metric"
  for (const auto& metric : all_metrics) {
    if (metric.length() > max_metric_len) {
      max_metric_len = metric.length();
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

}  // namespace guest_memory_metrics
