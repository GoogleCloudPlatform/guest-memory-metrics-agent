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

#ifndef GUEST_MEMORY_METRICS_AGENT_REPORT_ENGINE_H_
#define GUEST_MEMORY_METRICS_AGENT_REPORT_ENGINE_H_

#include <ostream>
#include <string>

namespace guest_memory_metrics {

enum class ReportType {
  kDelta,
  kComprehensive,
};

enum class ReportFormat {
  kTable,
  kCsv,
};

struct ReportOptions {
  std::string input_path;
  std::string start_str;
  std::string end_str;
  std::string output_path;
  ReportType type = ReportType::kDelta;
  ReportFormat format = ReportFormat::kTable;
};

class ReportEngine {
 public:
  ReportEngine() = default;
  ~ReportEngine() = default;

  // Legacy method for backward compatibility
  void GenerateReport(const std::string& input_path,
                      const std::string& start_str, const std::string& end_str);

  // Modern method with options and optional custom output stream
  bool GenerateReport(const ReportOptions& options,
                      std::ostream* custom_out = nullptr);

  // Specific format and type report generators
  bool GenerateDeltaReport(const ReportOptions& options, std::ostream& out);
  bool GenerateComprehensiveReport(const ReportOptions& options,
                                   std::ostream& out);
};

}  // namespace guest_memory_metrics

#endif  // GUEST_MEMORY_METRICS_AGENT_REPORT_ENGINE_H_
