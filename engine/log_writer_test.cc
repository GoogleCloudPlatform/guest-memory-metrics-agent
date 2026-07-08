#include "third_party/guest_memory_metrics_agent/engine/log_writer.h"

#include <string>

#include "testing/base/public/gunit.h"
#include "third_party/absl/strings/match.h"

namespace guest_memory_metrics {
namespace {

TEST(LogWriterTest, WriteMetricFormatsJsonCorrectly) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "host", "proc.meminfo.MemFree", 12345);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, R"("source":"host")") ||
              absl::StrContains(output, R"("source": "host")"));
  EXPECT_TRUE(
      absl::StrContains(output, R"("metric":"proc.meminfo.MemFree")") ||
      absl::StrContains(output, R"("metric": "proc.meminfo.MemFree")"));
  EXPECT_TRUE(absl::StrContains(output, R"("value":12345)") ||
              absl::StrContains(output, R"("value": 12345)"));
  EXPECT_TRUE(
      absl::StrContains(output, R"("timestamp":1672531200000)") ||
      absl::StrContains(output, R"("timestamp": 1672531200000)"));
}

TEST(LogWriterTest, ScrubPathRedactsDotSeparatedKeysWithDotsInUsername) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup", "prefix.home.first.last.metric",
                     100);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, "prefix.home.[REDACTED].metric"));
  EXPECT_FALSE(absl::StrContains(output, "first"));
  EXPECT_FALSE(absl::StrContains(output, "last"));
}

TEST(LogWriterTest, ScrubPathRedactsSlashSeparatedKeysWithDotsInUsername) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup", "/home/first.last/metric", 100);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, "/home/[REDACTED]/metric"));
  EXPECT_FALSE(absl::StrContains(output, "first"));
  EXPECT_FALSE(absl::StrContains(output, "last"));
}

TEST(LogWriterTest, ScrubPathRedactsUsersDotAndSlashSeparatedKeys) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup", "prefix.users.first.last.metric",
                     100);
  writer.WriteMetric(1672531200000, "cgroup", "/users/first.last/metric", 200);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, "prefix.users.[REDACTED].metric"));
  EXPECT_TRUE(absl::StrContains(output, "/users/[REDACTED]/metric"));
  EXPECT_FALSE(absl::StrContains(output, "first"));
  EXPECT_FALSE(absl::StrContains(output, "last"));
}

TEST(LogWriterTest, ScrubPathRedactsEdgeCases) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup", "home.first.last.metric", 1);
  writer.WriteMetric(1672531200000, "cgroup", "prefix.home.user.metric", 2);
  writer.WriteMetric(1672531200000, "cgroup", "/home/first.last", 3);
  writer.WriteMetric(1672531200000, "cgroup", "cgroup.home/first.last/stat", 4);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, "home.[REDACTED].metric"));
  EXPECT_TRUE(absl::StrContains(output, "prefix.home.[REDACTED].metric"));
  EXPECT_TRUE(absl::StrContains(output, "/home/[REDACTED]"));
  EXPECT_TRUE(absl::StrContains(output, "cgroup.home/[REDACTED]/stat"));
  EXPECT_FALSE(absl::StrContains(output, "first"));
  EXPECT_FALSE(absl::StrContains(output, "last"));
}

TEST(LogWriterTest, ScrubPathDoesNotRedactStandardCgroupMetrics) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup", "cgroup.home/memory.stat", 1);
  writer.WriteMetric(1672531200000, "cgroup", "cgroup.home/memory.current", 2);
  writer.WriteMetric(1672531200000, "cgroup", "cgroup.home/cpu.stat", 3);
  writer.WriteMetric(1672531200000, "cgroup", "cgroup.home.memory.stat", 4);
  writer.WriteMetric(1672531200000, "cgroup", "cgroup.users/memory.stat", 5);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, "cgroup.home/memory.stat"));
  EXPECT_TRUE(absl::StrContains(output, "cgroup.home/memory.current"));
  EXPECT_TRUE(absl::StrContains(output, "cgroup.home/cpu.stat"));
  EXPECT_TRUE(absl::StrContains(output, "cgroup.home.memory.stat"));
  EXPECT_TRUE(absl::StrContains(output, "cgroup.users/memory.stat"));
  EXPECT_FALSE(absl::StrContains(output, "[REDACTED]"));
}

TEST(LogWriterTest, ScrubPathRedactsUsernameBeforeReservedKeyword) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup",
                     "cgroup.home.first.last.memory.stat", 1);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, "cgroup.home.[REDACTED].memory.stat"));
  EXPECT_FALSE(absl::StrContains(output, "first"));
  EXPECT_FALSE(absl::StrContains(output, "last"));
}

}  // namespace
}  // namespace guest_memory_metrics
