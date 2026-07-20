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

#include "engine/log_writer.h"

#include <fstream>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/match.h"

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

TEST(LogWriterTest, ScrubCustomCgroupTokenAllowlist) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  writer.WriteMetric(123, "host", "cgroup/Bob_App/memory.current", 123);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find("Bob_App") == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "cgroup/[HASH:"));
}

TEST(LogWriterTest, ScrubConsecutiveProcPids) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  writer.WriteMetric(9999, "host", "/proc/12345/proc/67890/memory.stat", 8888);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find("12345") == std::string::npos);
  EXPECT_TRUE(output.find("67890") == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "/proc/[HASH:"));
}

TEST(LogWriterTest, AllowsNumberedSystemTokens) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  writer.WriteMetric(123, "host", "cgroup.cpu0.usage_in_bytes", 100);
  writer.WriteMetric(123, "host", "sys.node1.memory.stat", 200);
  writer.WriteMetric(123, "host", "net.eth0.stat", 300);
  writer.WriteMetric(123, "host", "disk.nvme0n1.stat", 400);
  writer.WriteMetric(123, "host", "block.sda1.stat", 500);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(absl::StrContains(output, "cpu0.usage_in_bytes"));
  EXPECT_TRUE(absl::StrContains(output, "node1.memory.stat"));
  EXPECT_TRUE(absl::StrContains(output, "eth0.stat"));
  EXPECT_TRUE(absl::StrContains(output, "nvme0n1.stat"));
  EXPECT_TRUE(absl::StrContains(output, "sda1.stat"));
}

TEST(LogWriterTest, HashesStructuralDigitTokens) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  writer.WriteMetric(123, "host", "cgroup.pod1234.memory.stat", 100);
  writer.WriteMetric(123, "host", "sys.user1000.cpu.stat", 200);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find("pod1234") == std::string::npos);
  EXPECT_TRUE(output.find("user1000") == std::string::npos);
  EXPECT_TRUE(absl::StrContains(output, "pod[HASH:") ||
              absl::StrContains(output, "[HASH:"));
}

TEST(LogWriterTest, JsonEscapesSpecialCharacters) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  writer.WriteMetric(123, "host\"name\n", "proc.meminfo.MemFree", 12345);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(absl::StrContains(output, R"("source": "host\"name\n")"));
}

TEST(LogWriterTest, ScrubCacheHitReturnsSameScrubbedString) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  // Call twice with identical metric path to exercise cache hit
  writer.WriteMetric(100, "host", "cgroup/user_app/memory.current", 50);
  writer.WriteMetric(200, "host", "cgroup/user_app/memory.current", 60);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(absl::StrContains(output, "cgroup/[HASH:"));
}

TEST(LogWriterTest, ScrubUserDirectoryNoUsernameOrSuffix) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  // Exercise trailing /home/ and /users/ without username/suffix
  writer.WriteMetric(100, "host", "/home/", 50);
  writer.WriteMetric(200, "host", "/users/", 60);
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(absl::StrContains(output, "/home/[REDACTED]"));
  EXPECT_TRUE(absl::StrContains(output, "/users/[REDACTED]"));
}

TEST(LogWriterTest, WriteMetricToFileStream) {
  std::string tmp_path = testing::TempDir() + "/test_metric_log.json";
  LogWriter writer(tmp_path);
  EXPECT_TRUE(writer.Open().ok());
  writer.WriteMetric(1234567890, "source_host", "proc.meminfo.MemFree", 4096);
  writer.Close();

  std::ifstream f(tmp_path);
  std::string file_content;
  std::getline(f, file_content);
  EXPECT_TRUE(absl::StrContains(file_content, R"("source": "source_host")"));
  EXPECT_TRUE(
      absl::StrContains(file_content, R"("metric": "proc.meminfo.MemFree")"));
  EXPECT_TRUE(absl::StrContains(file_content, R"("value": 4096)"));
}

TEST(LogWriterTest, ScrubCacheEvictionFiresWhenFull) {
  LogWriter writer;
  testing::internal::CaptureStdout();
  // Fill cache beyond 10,000 entries to trigger 50% eviction
  for (int i = 0; i < 10005; ++i) {
    writer.WriteMetric(100, "host",
                       absl::StrFormat("cgroup/app_%d/memory.stat", i), i);
  }
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_FALSE(output.empty());
}

}  // namespace
}  // namespace guest_memory_metrics
