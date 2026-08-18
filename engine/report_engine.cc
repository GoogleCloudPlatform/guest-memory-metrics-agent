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
#include <utility>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace guest_memory_metrics {
namespace {

std::string EscapeCsv(absl::string_view field) {
  if (field.find_first_of(",\"\r\n") == absl::string_view::npos) {
    return std::string(field);
  }
  std::string escaped;
  escaped.reserve(field.size() + 8);
  escaped.push_back('"');
  for (char c : field) {
    if (c == '"') {
      escaped.append("\"\"");
    } else {
      escaped.push_back(c);
    }
  }
  escaped.push_back('"');
  return escaped;
}

bool UnescapeJsonString(absl::string_view input, std::string* output) {
  output->clear();
  output->reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '\\') {
      if (i + 1 >= input.size()) {
        return false;
      }
      char c = input[++i];
      switch (c) {
        case '"':
          output->push_back('"');
          break;
        case '\\':
          output->push_back('\\');
          break;
        case '/':
          output->push_back('/');
          break;
        case 'b':
          output->push_back('\b');
          break;
        case 'f':
          output->push_back('\f');
          break;
        case 'n':
          output->push_back('\n');
          break;
        case 'r':
          output->push_back('\r');
          break;
        case 't':
          output->push_back('\t');
          break;
        case 'u': {
          if (i + 4 >= input.size()) {
            return false;
          }
          uint32_t code_point = 0;
          for (size_t j = 0; j < 4; ++j) {
            char hex_char = input[++i];
            code_point <<= 4;
            if (hex_char >= '0' && hex_char <= '9') {
              code_point |= (hex_char - '0');
            } else if (hex_char >= 'a' && hex_char <= 'f') {
              code_point |= (hex_char - 'a' + 10);
            } else if (hex_char >= 'A' && hex_char <= 'F') {
              code_point |= (hex_char - 'A' + 10);
            } else {
              return false;
            }
          }
          if (code_point <= 0x7F) {
            output->push_back(static_cast<char>(code_point));
          } else if (code_point <= 0x7FF) {
            output->push_back(
                static_cast<char>(0xC0 | ((code_point >> 6) & 0x1F)));
            output->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
          } else {
            output->push_back(
                static_cast<char>(0xE0 | ((code_point >> 12) & 0x0F)));
            output->push_back(
                static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
            output->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
          }
          break;
        }
        default:
          return false;
      }
    } else {
      output->push_back(input[i]);
    }
  }
  return true;
}

bool ExtractJsonString(absl::string_view sv, size_t* pos, std::string* output) {
  while (*pos < sv.size() && (sv[*pos] == ' ' || sv[*pos] == '\t' ||
                              sv[*pos] == '\r' || sv[*pos] == '\n')) {
    ++(*pos);
  }
  if (*pos >= sv.size() || sv[*pos] != '"') {
    return false;
  }
  ++(*pos);  // Skip opening quote
  size_t start = *pos;
  while (*pos < sv.size()) {
    if (sv[*pos] == '\\') {
      *pos += 2;
    } else if (sv[*pos] == '"') {
      break;
    } else {
      ++(*pos);
    }
  }
  if (*pos >= sv.size() || sv[*pos] != '"') {
    return false;
  }
  absl::string_view escaped = sv.substr(start, *pos - start);
  ++(*pos);  // Skip closing quote
  return UnescapeJsonString(escaped, output);
}

struct MetricRecord {
  int64_t timestamp = 0;
  std::string source;
  std::string metric;
  uint64_t value = 0;

  std::string FullMetric() const { return absl::StrCat(source, ".", metric); }
};

bool ParseMetricLine(absl::string_view line, MetricRecord* record) {
  size_t start_brace = line.find('{');
  size_t end_brace = line.rfind('}');
  if (start_brace == absl::string_view::npos ||
      end_brace == absl::string_view::npos || end_brace <= start_brace) {
    return false;
  }

  absl::string_view content =
      line.substr(start_brace + 1, end_brace - start_brace - 1);
  size_t pos = 0;
  bool has_timestamp = false;
  bool has_source = false;
  bool has_metric = false;
  bool has_value = false;

  auto skip_ws = [&]() {
    while (pos < content.size() &&
           (content[pos] == ' ' || content[pos] == '\t' ||
            content[pos] == '\r' || content[pos] == '\n')) {
      ++pos;
    }
  };

  while (pos < content.size()) {
    skip_ws();
    if (pos >= content.size()) break;

    std::string key;
    if (!ExtractJsonString(content, &pos, &key)) {
      return false;
    }

    skip_ws();
    if (pos >= content.size() || content[pos] != ':') {
      return false;
    }
    ++pos;  // Skip ':'
    skip_ws();

    if (pos >= content.size()) return false;

    if (content[pos] == '"') {
      std::string str_val;
      if (!ExtractJsonString(content, &pos, &str_val)) {
        return false;
      }
      if (key == "source") {
        record->source = std::move(str_val);
        has_source = true;
      } else if (key == "metric") {
        record->metric = std::move(str_val);
        has_metric = true;
      }
    } else {
      size_t val_start = pos;
      while (pos < content.size() && content[pos] != ',' &&
             content[pos] != ' ' && content[pos] != '\t' &&
             content[pos] != '\r' && content[pos] != '\n') {
        ++pos;
      }
      absl::string_view num_sv = content.substr(val_start, pos - val_start);
      if (key == "timestamp") {
        if (!absl::SimpleAtoi(num_sv, &record->timestamp)) {
          return false;
        }
        has_timestamp = true;
      } else if (key == "value") {
        if (!absl::SimpleAtoi(num_sv, &record->value)) {
          return false;
        }
        has_value = true;
      }
    }

    skip_ws();
    if (pos < content.size()) {
      if (content[pos] == ',') {
        ++pos;  // Skip ','
      } else {
        return false;
      }
    }
  }

  return has_timestamp && has_source && has_metric && has_value;
}

std::string FormatPercentage(uint64_t start_val, int64_t delta) {
  if (delta == 0) {
    return "0.00%";
  }
  if (start_val == 0) {
    return "N/A";
  }
  double pct_change = (static_cast<double>(delta) / start_val) * 100.0;
  return absl::StrFormat("%.2f%%", pct_change);
}

}  // namespace

void ReportEngine::GenerateReport(const std::string& input_path,
                                  const std::string& start_str,
                                  const std::string& end_str) {
  ReportOptions options;
  options.input_path = input_path;
  options.start_str = start_str;
  options.end_str = end_str;
  options.type = ReportType::kDelta;
  options.format = ReportFormat::kTable;
  GenerateReport(options, &std::cout);
}

bool ReportEngine::GenerateReport(const ReportOptions& options,
                                  std::ostream* custom_out) {
  int64_t start_ts = 0;
  int64_t end_ts = 0;
  if (!absl::SimpleAtoi(options.start_str, &start_ts) ||
      !absl::SimpleAtoi(options.end_str, &end_ts)) {
    std::cerr << "Failed to parse start or end timestamp. Must be numeric."
              << std::endl;
    return false;
  }
  if (options.type == ReportType::kDelta && start_ts > end_ts) {
    std::cerr << "Start timestamp must be less than or equal to end timestamp "
                 "for delta report."
              << std::endl;
    return false;
  }
  {
    std::ifstream in_check(options.input_path);
    if (!in_check.is_open()) {
      std::cerr << "Failed to open input file: " << options.input_path
                << std::endl;
      return false;
    }
  }

  if (custom_out != nullptr) {
    if (options.type == ReportType::kComprehensive) {
      return GenerateComprehensiveReport(options, *custom_out);
    }
    return GenerateDeltaReport(options, *custom_out);
  }

  if (!options.output_path.empty()) {
    std::ofstream out_file(options.output_path);
    if (!out_file.is_open()) {
      std::cerr << "Failed to open output report file: " << options.output_path
                << std::endl;
      return false;
    }
    bool success = false;
    if (options.type == ReportType::kComprehensive) {
      success = GenerateComprehensiveReport(options, out_file);
    } else {
      success = GenerateDeltaReport(options, out_file);
    }
    out_file.close();
    if (success) {
      std::cout << "Report generated successfully: " << options.output_path
                << std::endl;
    }
    return success;
  }

  if (options.type == ReportType::kComprehensive) {
    return GenerateComprehensiveReport(options, std::cout);
  }
  return GenerateDeltaReport(options, std::cout);
}

bool ReportEngine::GenerateDeltaReport(const ReportOptions& options,
                                       std::ostream& out) {
  int64_t start_ts = 0;
  int64_t end_ts = 0;
  if (!absl::SimpleAtoi(options.start_str, &start_ts) ||
      !absl::SimpleAtoi(options.end_str, &end_ts)) {
    std::cerr << "Failed to parse start or end timestamp. Must be numeric."
              << std::endl;
    return false;
  }
  if (start_ts > end_ts) {
    std::cerr << "Start timestamp must be less than or equal to end timestamp "
                 "for delta report."
              << std::endl;
    return false;
  }

  std::ifstream file(options.input_path);
  if (!file.is_open()) {
    std::cerr << "Failed to open input file: " << options.input_path
              << std::endl;
    return false;
  }

  std::string line;
  int64_t closest_start_diff = -1;
  int64_t closest_end_diff = -1;

  struct Snapshot {
    int64_t timestamp = -1;
    std::map<std::string, uint64_t> metrics;
  };

  Snapshot current_snapshot;
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
    MetricRecord record;
    if (!ParseMetricLine(line, &record)) {
      continue;
    }

    std::string full_metric = record.FullMetric();

    if (record.timestamp != current_snapshot.timestamp) {
      process_snapshot();
      current_snapshot.timestamp = record.timestamp;
      current_snapshot.metrics.clear();
    }
    current_snapshot.metrics[full_metric] = record.value;
  }
  process_snapshot();

  if (!has_snapshots) {
    std::cerr << "No valid snapshots found in log." << std::endl;
    return false;
  }

  Snapshot* best_start = &best_start_snap;
  Snapshot* best_end = &best_end_snap;

  std::set<std::string> all_metrics;
  for (const auto& [metric, _] : best_start->metrics) all_metrics.insert(metric);
  for (const auto& [metric, _] : best_end->metrics) all_metrics.insert(metric);

  if (options.format == ReportFormat::kCsv) {
    out << "Metric,StartTimestamp,EndTimestamp,StartValue,EndValue,Delta,"
           "PctChange\n";
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

      std::string pct_str = FormatPercentage(start_val, delta);

      out << EscapeCsv(metric) << "," << best_start->timestamp << ","
          << best_end->timestamp << "," << start_val << "," << end_val << ","
          << delta << "," << pct_str << "\n";
    }
    return true;
  }

  // Output ASCII Table format
  size_t max_metric_len = 6;  // Length of "Metric"
  for (const auto& metric : all_metrics) {
    if (metric.length() > max_metric_len) {
      max_metric_len = metric.length();
    }
  }

  int metric_col_width = static_cast<int>(max_metric_len) + 5;
  int total_width = metric_col_width + 60;

  out << "Delta Report (" << best_start->timestamp << " to "
      << best_end->timestamp << ")\n";
  out << std::string(total_width, '-') << "\n";
  out << std::left << std::setw(metric_col_width) << "Metric" << std::right
      << std::setw(15) << "Start" << std::setw(15) << "End" << std::setw(15)
      << "Delta" << std::setw(15) << "Pct Change" << "\n";
  out << std::string(total_width, '-') << "\n";

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

    std::string pct_str = FormatPercentage(start_val, delta);

    out << std::left << std::setw(metric_col_width) << metric << std::right
        << std::setw(15) << start_val << std::setw(15) << end_val
        << std::setw(15) << delta << std::setw(15) << pct_str << "\n";
  }
  return true;
}

bool ReportEngine::GenerateComprehensiveReport(const ReportOptions& options,
                                               std::ostream& out) {
  int64_t start_ts = 0;
  int64_t end_ts = 0;
  if (!absl::SimpleAtoi(options.start_str, &start_ts) ||
      !absl::SimpleAtoi(options.end_str, &end_ts)) {
    std::cerr << "Failed to parse start or end timestamp. Must be numeric."
              << std::endl;
    return false;
  }
  if (start_ts > end_ts) {
    std::swap(start_ts, end_ts);
  }

  std::ifstream file(options.input_path);
  if (!file.is_open()) {
    std::cerr << "Failed to open input file: " << options.input_path
              << std::endl;
    return false;
  }

  std::string line;
  out << "timestamp_ms,metric,value\n";

  while (std::getline(file, line)) {
    MetricRecord record;
    if (!ParseMetricLine(line, &record)) {
      continue;
    }

    if (record.timestamp >= start_ts && record.timestamp <= end_ts) {
      std::string full_metric = record.FullMetric();
      out << record.timestamp << "," << EscapeCsv(full_metric) << ","
          << record.value << "\n";
    }
  }

  return true;
}

}  // namespace guest_memory_metrics
