#include "third_party/guest_memory_metrics_agent/engine/report_engine.h"

#include <fstream>
#include <string>

#include "testing/base/public/gunit.h"

namespace guest_memory_metrics {
namespace {

TEST(ReportEngineTest, GeneratesReportSuccessfully) {
  // Create a temporary file with fake JSON log data
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
  out << R"({"timestamp": 3000, "source": "host", "metric": "MemFree", "value": 3000})"
      << "\n";
  out << R"({"timestamp": 3000, "source": "host", "metric": "Active", "value": 3000})"
      << "\n";
  out.close();

  ReportEngine engine;

  testing::internal::CaptureStdout();
  engine.GenerateReport(temp_file, "1000", "3000");
  std::string output = testing::internal::GetCapturedStdout();
  
  EXPECT_NE(output.find("host.MemFree"), std::string::npos);
  EXPECT_NE(output.find("host.Active"), std::string::npos);
}

TEST(ReportEngineTest, HandlesInvalidTimestampsGracefully) {
  ReportEngine engine;
  testing::internal::CaptureStderr();
  engine.GenerateReport("nonexistent_file.log", "invalid_start", "invalid_end");
  std::string err_output = testing::internal::GetCapturedStderr();
  EXPECT_NE(err_output.find("Failed to parse start or end timestamp"),
            std::string::npos);
}

}  // namespace
}  // namespace guest_memory_metrics
