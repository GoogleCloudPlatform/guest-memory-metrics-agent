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
  EXPECT_TRUE(absl::StrContains(output, R"("metric":"proc.meminfo.MemFree")") ||
              absl::StrContains(output, R"("metric": "proc.meminfo.MemFree")"));
  EXPECT_TRUE(absl::StrContains(output, R"("value":12345)") ||
              absl::StrContains(output, R"("value": 12345)"));
  EXPECT_TRUE(absl::StrContains(output, R"("timestamp":1672531200000)") ||
              absl::StrContains(output, R"("timestamp": 1672531200000)"));
}

TEST(LogWriterTest, ScrubPathRedactsDotSeparatedKeysWithDotsInUsername) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup",
                     "prefix.home.first.last.memory.stat", 100);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, "prefix.home.[REDACTED]"));
  EXPECT_FALSE(absl::StrContains(output, "first"));
  EXPECT_FALSE(absl::StrContains(output, "last"));
}

TEST(LogWriterTest, ScrubPathRedactsSlashSeparatedKeysWithDotsInUsername) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup", "/home/first.last/memory.stat",
                     100);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, "/home/[REDACTED]/memory.stat"));
  EXPECT_FALSE(absl::StrContains(output, "first"));
  EXPECT_FALSE(absl::StrContains(output, "last"));
}

TEST(LogWriterTest, ScrubPathRedactsUsersDotAndSlashSeparatedKeys) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup",
                     "prefix.users.first.last.memory.stat", 100);
  writer.WriteMetric(1672531200000, "cgroup", "/users/first.last/memory.stat",
                     200);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, "prefix.users.[REDACTED]"));
  EXPECT_TRUE(absl::StrContains(output, "/users/[REDACTED]/memory.stat"));
  EXPECT_FALSE(absl::StrContains(output, "first"));
  EXPECT_FALSE(absl::StrContains(output, "last"));
}

TEST(LogWriterTest, ScrubPathRedactsEdgeCases) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup", "home.first.last.memory.stat", 1);
  writer.WriteMetric(1672531200000, "cgroup", "prefix.home.user.memory.stat",
                     2);
  writer.WriteMetric(1672531200000, "cgroup", "/home/first.last", 3);
  writer.WriteMetric(1672531200000, "cgroup", "cgroup.home/first.last/stat", 4);
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_TRUE(absl::StrContains(output, "home.[REDACTED]"));
  EXPECT_TRUE(absl::StrContains(output, "prefix.home.[REDACTED]"));
  EXPECT_TRUE(absl::StrContains(output, "/home/[REDACTED]"));
  EXPECT_TRUE(absl::StrContains(output, "cgroup.home/[REDACTED]/stat"));
  EXPECT_FALSE(absl::StrContains(output, "first"));
  EXPECT_FALSE(absl::StrContains(output, "last"));
  EXPECT_FALSE(absl::StrContains(output, "user"));
}

TEST(LogWriterTest, ScrubPathRedactsUsernamesStartingWithReservedKeywords) {
  LogWriter writer;

  testing::internal::CaptureStdout();
  writer.WriteMetric(1672531200000, "cgroup", "/home/memory.user/memory.stat",
                     1);
  writer.WriteMetric(1672531200000, "cgroup",
                     "cgroup.home.memory.user.memory.stat", 2);
  writer.WriteMetric(1672531200000, "cgroup", "cgroup.home.myuser", 3);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(absl::StrContains(output, "/home/[REDACTED]/memory.stat"));
  EXPECT_TRUE(absl::StrContains(output, "cgroup.home.[REDACTED]"));
  EXPECT_FALSE(absl::StrContains(output, "memory.user"));
  EXPECT_FALSE(absl::StrContains(output, "myuser"));
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

TEST(LogWriterTest, ScrubDockerId) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  std::string raw_id =
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
  writer.WriteMetric(123, "host", "cgroup.docker/" + raw_id + "/memory.current",
                     123);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find(raw_id) == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "[HASH:"));
  EXPECT_TRUE(absl::StrContains(output, "cgroup.docker/[HASH:"));
}

TEST(LogWriterTest, ScrubKubernetesPodUid) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  std::string raw_id = "123e4567-e89b-12d3-a456-426614174000";
  writer.WriteMetric(123, "host",
                     "cgroup.kubepods/pod" + raw_id + "/memory.current", 123);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find(raw_id) == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "[HASH:"));
}

TEST(LogWriterTest, ScrubSystemdPodSlice) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  std::string raw_id = "12345678\\x2d1234\\x2d1234\\x2d1234\\x2d123456789012";
  writer.WriteMetric(
      123, "host",
      "cgroup.kubepods.slice/pod" + raw_id + ".slice/memory.current", 123);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find(raw_id) == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "[HASH:"));
}

TEST(LogWriterTest, ScrubSystemdUserSlice) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  std::string raw_id = "1000";
  writer.WriteMetric(
      123, "host", "cgroup.user.slice/user-" + raw_id + ".slice/memory.current",
      123);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find("user-" + raw_id + ".slice") == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "[HASH:"));
}

TEST(LogWriterTest, ScrubRunUserDirectory) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  std::string raw_id = "1000";
  writer.WriteMetric(123, "host",
                     "cgroup/run/user/" + raw_id + "/memory.current", 123);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find("/run/user/" + raw_id + "/") == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "[HASH:"));
}

TEST(LogWriterTest, ScrubUsernames) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  std::string raw_id = "alice";
  writer.WriteMetric(123, "host", "cgroup/home/" + raw_id + "/memory.current",
                     123);
  writer.WriteMetric(123, "host", "cgroup/users/bob/memory.current", 123);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find("/home/" + raw_id + "/") == std::string::npos);
  EXPECT_TRUE(!absl::StrContains(output, "/users/bob/"));
  EXPECT_TRUE(absl::StrContains(output, "[REDACTED]"));
}

TEST(LogWriterTest, ScrubMachineScope) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  std::string raw_id = "qemu-1-my-vm";
  writer.WriteMetric(
      123, "host",
      "cgroup.machine.slice/machine-" + raw_id + ".scope/memory.current", 123);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find("machine-" + raw_id + ".scope") == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "[HASH:"));
}

TEST(LogWriterTest, ScrubContainerdId) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  std::string raw_id =
      "f4c8996fb92427ae41e4649b934ca495991b7852b855e3b0c44298fc1c149afb";
  writer.WriteMetric(123, "host",
                     "cgroup.containerd/" + raw_id + "/memory.current", 123);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find(raw_id) == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "[HASH:"));
}

TEST(LogWriterTest, ScrubGenericUuidAndHash) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  std::string raw_uuid = "550e8400-e29b-41d4-a716-446655440000";
  std::string raw_hash =
      "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f60000";
  writer.WriteMetric(123, "host",
                     "cgroup/" + raw_uuid + "/" + raw_hash + "/memory.current",
                     123);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find(raw_uuid) == std::string::npos);
  EXPECT_TRUE(output.find(raw_hash) == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "[HASH:"));
}

}  // namespace
}  // namespace guest_memory_metrics
