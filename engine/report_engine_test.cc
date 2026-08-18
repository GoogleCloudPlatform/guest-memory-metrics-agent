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

#include <fstream>
#include <sstream>
#include <string>

#include "gtest/gtest.h"

namespace guest_memory_metrics {
namespace {

std::string CreateTestLogFile() {
  std::string temp_file = std::string(testing::TempDir()) + "/fake_metrics.log";
  std::ofstream out(temp_file);
  out << R"({"timestamp": 1000, "source": "host", "metric": "MemFree", "value": 5000})"
      << "\n";
  out << R"({"timestamp": 1000, "source": "host", "metric": "Active", "value": 2000})"
      << "\n";
  out << R"({"timestamp": 2000, "source": "host", "metric": "MemFree", "value": 4000})"
      << "\n";
  out << R"({"timestamp": 2000, "source": "host", "metric": "Active", "value": 2500})"
      << "\n";
  out << R"({"timestamp": 3000, "source": "host", "metric": "MemFree", "value": 2500})"
      << "\n";
  out << R"({"timestamp": 3000, "source": "host", "metric": "Active", "value": 3000})"
      << "\n";
  out.close();
  return temp_file;
}

TEST(ReportEngineTest, GeneratesReportSuccessfully) {
  std::string temp_file = CreateTestLogFile();
  ReportEngine engine;

  testing::internal::CaptureStdout();
  engine.GenerateReport(temp_file, "1000", "3000");
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_NE(output.find("host.MemFree"), std::string::npos);
  EXPECT_NE(output.find("host.Active"), std::string::npos);
  EXPECT_NE(output.find("Delta Report (1000 to 3000)"), std::string::npos);
}

TEST(ReportEngineTest, GeneratesCsvDeltaReportSuccessfully) {
  std::string temp_file = CreateTestLogFile();
  ReportEngine engine;

  ReportOptions options;
  options.input_path = temp_file;
  options.start_str = "1000";
  options.end_str = "3000";
  options.type = ReportType::kDelta;
  options.format = ReportFormat::kCsv;

  std::ostringstream ss;
  EXPECT_TRUE(engine.GenerateReport(options, &ss));
  std::string csv = ss.str();

  EXPECT_NE(
      csv.find("Metric,StartTimestamp,EndTimestamp,StartValue,EndValue,Delta,"
               "PctChange\n"),
      std::string::npos);
  EXPECT_NE(csv.find("host.MemFree,1000,3000,5000,2500,-2500,-50.00%"),
            std::string::npos);
  EXPECT_NE(csv.find("host.Active,1000,3000,2000,3000,1000,50.00%"),
            std::string::npos);
}

TEST(ReportEngineTest, GeneratesComprehensiveCsvReportSuccessfully) {
  std::string temp_file = CreateTestLogFile();
  ReportEngine engine;

  ReportOptions options;
  options.input_path = temp_file;
  options.start_str = "1000";
  options.end_str = "2500";
  options.type = ReportType::kComprehensive;
  options.format = ReportFormat::kCsv;

  std::ostringstream ss;
  EXPECT_TRUE(engine.GenerateReport(options, &ss));
  std::string csv = ss.str();

  EXPECT_NE(csv.find("timestamp_ms,metric,value\n"), std::string::npos);
  // Timestamps 1000 and 2000 are in range [1000, 2500], while 3000 is excluded
  EXPECT_NE(csv.find("1000,host.MemFree,5000\n"), std::string::npos);
  EXPECT_NE(csv.find("1000,host.Active,2000\n"), std::string::npos);
  EXPECT_NE(csv.find("2000,host.MemFree,4000\n"), std::string::npos);
  EXPECT_NE(csv.find("2000,host.Active,2500\n"), std::string::npos);
  EXPECT_EQ(csv.find("3000,host.MemFree"), std::string::npos);
}

TEST(ReportEngineTest, HandlesReversedTimestampsCorrectly) {
  std::string temp_file = CreateTestLogFile();
  ReportEngine engine;

  ReportOptions options;
  options.input_path = temp_file;
  options.start_str = "2500";
  options.end_str = "1000";  // Reversed
  options.type = ReportType::kComprehensive;
  options.format = ReportFormat::kCsv;

  std::ostringstream ss;
  EXPECT_TRUE(engine.GenerateReport(options, &ss));
  std::string csv = ss.str();

  EXPECT_NE(csv.find("1000,host.MemFree,5000\n"), std::string::npos);
  EXPECT_NE(csv.find("2000,host.MemFree,4000\n"), std::string::npos);
}

TEST(ReportEngineTest, EscapesCsvSpecialCharactersProperly) {
  std::string temp_file =
      std::string(testing::TempDir()) + "/special_metrics.log";
  std::ofstream out(temp_file);
  out << R"({"timestamp": 1000, "source": "host", "metric": "test,comma", "value": 100})"
      << "\n";
  out << R"({"timestamp": 1000, "source": "host", "metric": "test_normal", "value": 200})"
      << "\n";
  out.close();

  ReportEngine engine;
  ReportOptions options;
  options.input_path = temp_file;
  options.start_str = "1000";
  options.end_str = "1000";
  options.type = ReportType::kComprehensive;
  options.format = ReportFormat::kCsv;

  std::ostringstream ss;
  EXPECT_TRUE(engine.GenerateReport(options, &ss));
  std::string csv = ss.str();

  EXPECT_NE(csv.find("1000,\"host.test,comma\",100\n"), std::string::npos);
  EXPECT_NE(csv.find("1000,host.test_normal,200\n"), std::string::npos);
}

TEST(ReportEngineTest, GeneratesReportToFileSuccessfully) {
  std::string temp_log = CreateTestLogFile();
  std::string temp_out = std::string(testing::TempDir()) + "/out_report.csv";

  ReportEngine engine;
  ReportOptions options;
  options.input_path = temp_log;
  options.start_str = "1000";
  options.end_str = "3000";
  options.output_path = temp_out;
  options.type = ReportType::kComprehensive;
  options.format = ReportFormat::kCsv;

  EXPECT_TRUE(engine.GenerateReport(options));

  std::ifstream in(temp_out);
  ASSERT_TRUE(in.is_open());
  std::stringstream ss;
  ss << in.rdbuf();
  std::string file_content = ss.str();

  EXPECT_NE(file_content.find("timestamp_ms,metric,value\n"),
            std::string::npos);
  EXPECT_NE(file_content.find("3000,host.MemFree,2500\n"), std::string::npos);
}

TEST(ReportEngineTest, HandlesInvalidTimestampsGracefully) {
  ReportEngine engine;
  testing::internal::CaptureStderr();
  engine.GenerateReport("nonexistent_file.log", "invalid_start", "invalid_end");
  std::string err_output = testing::internal::GetCapturedStderr();
  EXPECT_NE(err_output.find("Failed to parse start or end timestamp"),
            std::string::npos);
}

TEST(ReportEngineTest, HandlesMissingFileGracefully) {
  ReportEngine engine;
  ReportOptions options;
  options.input_path = "/nonexistent/path/to/log.jsonl";
  options.start_str = "1000";
  options.end_str = "2000";

  testing::internal::CaptureStderr();
  EXPECT_FALSE(engine.GenerateReport(options));
  std::string err = testing::internal::GetCapturedStderr();
  EXPECT_NE(err.find("Failed to open input file"), std::string::npos);
}

TEST(ReportEngineTest, DeltaReportFailsOnReversedTimestamps) {
  std::string temp_file = CreateTestLogFile();
  ReportEngine engine;

  ReportOptions options;
  options.input_path = temp_file;
  options.start_str = "3000";
  options.end_str = "1000";  // Reversed for delta mode
  options.type = ReportType::kDelta;
  options.format = ReportFormat::kTable;

  testing::internal::CaptureStderr();
  EXPECT_FALSE(engine.GenerateReport(options));
  std::string err = testing::internal::GetCapturedStderr();
  EXPECT_NE(err.find("Start timestamp must be less than or equal to end "
                     "timestamp for delta report"),
            std::string::npos);
}

TEST(ReportEngineTest, FormatPercentageEdgeCasesInDeltaReport) {
  std::string temp_file =
      std::string(testing::TempDir()) + "/pct_edge_cases.log";
  std::ofstream out(temp_file);
  // Timestamp 1000 snapshot
  out << R"({"timestamp": 1000, "source": "host", "metric": "ZeroZero", "value": 0})"
      << "\n";
  out << R"({"timestamp": 1000, "source": "host", "metric": "ZeroToNonZero", "value": 0})"
      << "\n";
  out << R"({"timestamp": 1000, "source": "host", "metric": "SameVal", "value": 500})"
      << "\n";
  // Timestamp 2000 snapshot
  out << R"({"timestamp": 2000, "source": "host", "metric": "ZeroZero", "value": 0})"
      << "\n";
  out << R"({"timestamp": 2000, "source": "host", "metric": "ZeroToNonZero", "value": 100})"
      << "\n";
  out << R"({"timestamp": 2000, "source": "host", "metric": "SameVal", "value": 500})"
      << "\n";
  out.close();

  ReportEngine engine;
  ReportOptions options;
  options.input_path = temp_file;
  options.start_str = "1000";
  options.end_str = "2000";
  options.type = ReportType::kDelta;
  options.format = ReportFormat::kCsv;

  std::ostringstream ss;
  EXPECT_TRUE(engine.GenerateReport(options, &ss));
  std::string csv = ss.str();

  EXPECT_NE(csv.find("host.ZeroZero,1000,2000,0,0,0,0.00%"), std::string::npos);
  EXPECT_NE(csv.find("host.ZeroToNonZero,1000,2000,0,100,100,N/A"),
            std::string::npos);
  EXPECT_NE(csv.find("host.SameVal,1000,2000,500,500,0,0.00%"),
            std::string::npos);
}

TEST(ReportEngineTest, ParsesJsonEscapesAndOutputsProperCsv) {
  std::string temp_file =
      std::string(testing::TempDir()) + "/escaped_metrics.log";
  std::ofstream out(temp_file);
  out << R"({"timestamp": 1000, "source": "cgroup", "metric": "quote\"metric", "value": 10})"
      << "\n";
  out << R"({"timestamp": 1000, "source": "cgroup", "metric": "slash\\metric", "value": 20})"
      << "\n";
  out << R"({"timestamp": 1000, "source": "cgroup", "metric": "unicode\u0020metric", "value": 30})"
      << "\n";
  out << R"({"timestamp": 1000, "source": "cgroup", "metric": "unicode\u00a9metric", "value": 40})"
      << "\n";
  out << R"({"timestamp": 1000, "source": "cgroup", "metric": "unicode\u20acmetric", "value": 50})"
      << "\n";
  out << R"({"timestamp": 1000, "source": "cgroup", "metric": "escapes\/\b\f\tmetric", "value": 60})"
      << "\n";
  out << R"({"timestamp": 1000, "source": "cgroup", "metric": "newline\nmetric", "value": 70})"
      << "\n";
  out.close();

  ReportEngine engine;
  ReportOptions options;
  options.input_path = temp_file;
  options.start_str = "1000";
  options.end_str = "1000";
  options.type = ReportType::kComprehensive;
  options.format = ReportFormat::kCsv;

  std::ostringstream ss;
  EXPECT_TRUE(engine.GenerateReport(options, &ss));
  std::string csv = ss.str();

  EXPECT_NE(csv.find("1000,\"cgroup.quote\"\"metric\",10\n"),
            std::string::npos);
  EXPECT_NE(csv.find("1000,cgroup.slash\\metric,20\n"), std::string::npos);
  EXPECT_NE(csv.find("1000,cgroup.unicode metric,30\n"), std::string::npos);
  EXPECT_NE(csv.find("1000,cgroup.unicode\xC2\xA9metric,40\n"),
            std::string::npos);
  EXPECT_NE(csv.find("1000,cgroup.unicode\xE2\x82\xACmetric,50\n"),
            std::string::npos);
  EXPECT_NE(csv.find("1000,cgroup.escapes/\b\f\tmetric,60\n"),
            std::string::npos);
  EXPECT_NE(csv.find("1000,\"cgroup.newline\nmetric\",70\n"),
            std::string::npos);
}

TEST(ReportEngineTest, PreValidationPreventsOutputFileCreationOnFailure) {
  std::string temp_out =
      std::string(testing::TempDir()) + "/nonexistent_output_file.csv";
  // Ensure the file does not exist before test
  std::remove(temp_out.c_str());

  ReportEngine engine;
  ReportOptions options;
  options.input_path = "/nonexistent/input/path.log";
  options.start_str = "1000";
  options.end_str = "2000";
  options.output_path = temp_out;
  options.type = ReportType::kComprehensive;

  testing::internal::CaptureStderr();
  EXPECT_FALSE(engine.GenerateReport(options));
  testing::internal::GetCapturedStderr();

  // Verify that the output file was never created
  std::ifstream check(temp_out);
  EXPECT_FALSE(check.is_open());
}

TEST(ReportEngineTest, HandlesMalformedJsonLinesGracefully) {
  std::string temp_file =
      std::string(testing::TempDir()) + "/malformed_metrics.log";
  std::ofstream out(temp_file);
  out << "not a json line\n";
  out << "{\"incomplete\": true}\n";
  out << "{\"timestamp\": \"not_a_number\", \"source\": \"host\", \"metric\": "
         "\"test\", \"value\": 1}\n";
  out << R"({"timestamp": 1000, "source": "host", "metric": "valid_metric", "value": 42})"
      << "\n";
  out.close();

  ReportEngine engine;
  ReportOptions options;
  options.input_path = temp_file;
  options.start_str = "1000";
  options.end_str = "1000";
  options.type = ReportType::kComprehensive;
  options.format = ReportFormat::kCsv;

  std::ostringstream ss;
  EXPECT_TRUE(engine.GenerateReport(options, &ss));
  std::string csv = ss.str();

  EXPECT_NE(csv.find("1000,host.valid_metric,42\n"), std::string::npos);
}

}  // namespace
}  // namespace guest_memory_metrics
