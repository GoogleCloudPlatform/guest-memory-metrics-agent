#include "third_party/guest_memory_metrics_agent/engine/log_writer.h"

#include <cstdint>
#include <iostream>
#include <regex>  // NOLINT
#include <string>

#include "third_party/absl/status/status.h"
#include "third_party/absl/strings/str_format.h"

namespace guest_memory_metrics {

// ScrubContainerId removed

LogWriter::LogWriter(const std::string& output_path)
    : output_path_(output_path) {}

LogWriter::~LogWriter() { Close(); }

absl::Status LogWriter::Open() {
  if (output_path_.empty()) {
    // If no path is provided, we'll write to stdout
    return absl::OkStatus();
  }

  out_stream_.open(output_path_, std::ios::app);
  if (!out_stream_.is_open()) {
    return absl::InternalError("Failed to open output log file: " +
                               output_path_);
  }
  return absl::OkStatus();
}

void LogWriter::Close() {
  if (out_stream_.is_open()) {
    out_stream_.close();
  }
}

std::string LogWriter::ScrubPath(const std::string& path) {
  // Path anonymization: redact user identifiers or common PII paths
  std::string scrubbed = path;

  // Standard cgroup/proc keywords that should not be redacted as usernames.
  static const char kReservedKeywords[] =
      "memory|proc|cpu|cpuacct|cpuset|cgroup|io|blkio|pids|net_cls|net_prio|"
      "devices|freezer|hugetlb|perf_event|rdma|misc|numa|stat|vmstat|meminfo|"
      "node|tasks|procs|notify_on_release";

  // Negative lookahead to prevent matching standard cgroup/proc keywords.
  static const std::string kNotReserved =
      absl::StrFormat(R"((?!(?:%s)(?:/|\.|$)))", kReservedKeywords);

  // Segment pattern for dot-separated paths (non-slash, non-dot, non-reserved).
  static const std::string kDotSeg =
      absl::StrFormat(R"((?!(?:%s)(?:/|\.|$))[^/.]+)", kReservedKeywords);

  // Replace /home/username/ with /home/[REDACTED]/
  static const std::regex home_slash_regex(
      absl::StrFormat(R"((/|\.|^)home/%s([^/]+)(/|$))", kNotReserved));
  scrubbed =
      std::regex_replace(scrubbed, home_slash_regex, "$1home/[REDACTED]$3");

  // Replace .home.username. with .home.[REDACTED].
  static const std::regex home_dot_regex(
      absl::StrFormat(R"((/|\.|^)home\.(%s(?:\.%s)*)\.)", kDotSeg, kDotSeg));
  scrubbed =
      std::regex_replace(scrubbed, home_dot_regex, "$1home.[REDACTED].");

  // Replace /users/username/ with /users/[REDACTED]/
  static const std::regex users_slash_regex(
      absl::StrFormat(R"((/|\.|^)users/%s([^/]+)(/|$))", kNotReserved));
  scrubbed =
      std::regex_replace(scrubbed, users_slash_regex, "$1users/[REDACTED]$3");

  // Replace .users.username. with .users.[REDACTED].
  static const std::regex users_dot_regex(
      absl::StrFormat(R"((/|\.|^)users\.(%s(?:\.%s)*)\.)", kDotSeg, kDotSeg));
  scrubbed =
      std::regex_replace(scrubbed, users_dot_regex, "$1users.[REDACTED].");

  return scrubbed;
}

void LogWriter::WriteMetric(int64_t timestamp, const std::string& source,
                            const std::string& metric_name, uint64_t value) {
  std::string scrubbed_name = ScrubPath(metric_name);

  std::string line = absl::StrFormat(
      "{\"timestamp\": %d, \"source\": \"%s\", \"metric\": \"%s\", \"value\": "
      "%u}\n",
      timestamp, source, scrubbed_name, value);

  if (out_stream_.is_open()) {
    out_stream_ << line;
    out_stream_.flush();
  } else {
    // Fallback to stdout
    std::cout << line;
  }
}

}  // namespace guest_memory_metrics
