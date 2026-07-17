#include "third_party/guest_memory_metrics_agent/engine/log_writer.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <regex>  // NOLINT
#include <string>

#include "third_party/absl/base/no_destructor.h"
#include "third_party/absl/status/status.h"
#include "third_party/absl/strings/match.h"
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
  if (path.length() < 12 &&
      !absl::StrContains(path, "home") &&
      !absl::StrContains(path, "users") &&
      !absl::StrContains(path, "slice") &&
      !absl::StrContains(path, "scope") &&
      !absl::StrContains(path, "pod") &&
      !absl::StrContains(path, "run") &&
      !absl::StrContains(path, "proc") &&
      !absl::StrContains(path, "docker") &&
      !absl::StrContains(path, "containerd") &&
      !absl::StrContains(path, "crio") &&
      !absl::StrContains(path, "lxc")) {
    return path;
  }

  auto it = scrub_cache_.find(path);
  if (it != scrub_cache_.end()) {
    return it->second;
  }

  std::string scrubbed = path;

  // Standard cgroup/proc keywords that should not be redacted as usernames.
  static const char kReservedKeywords[] =
      "memory|proc|cpu|cpuacct|cpuset|cgroup|io|blkio|pids|net_cls|net_prio|"
      "devices|freezer|hugetlb|perf_event|rdma|misc|numa|stat|vmstat|meminfo|"
      "node|tasks|procs|notify_on_release|current|max|swap|events|high|low|"
      "min|oom|peak|pressure|weight|zswap|idle|failcnt|limit_in_bytes|usage_in_"
      "bytes";

  static const std::string kReservedSuffix = absl::StrFormat(
      R"((?:%s)(?:\.(?:%s))*)", kReservedKeywords, kReservedKeywords);

  static const std::string kNotPurelyReservedDot =
      absl::StrFormat(R"((?!%s$))", kReservedSuffix);

  static const std::string kNotPurelyReservedSlash =
      absl::StrFormat(R"((?!%s(?:/|$)))", kReservedSuffix);

  // Replace /home/username/ with /home/[REDACTED]/, swallowing subdirectories
  static const absl::NoDestructor<std::regex> home_slash_regex(absl::StrFormat(
      R"((/|\.|^)home/(%s(?:[^/]+/)*?[^/]+)(/?)(?=(?:/|\.)%s|$))",
      kNotPurelyReservedSlash, kReservedSuffix));
  scrubbed =
      std::regex_replace(scrubbed, *home_slash_regex, "$1home/[REDACTED]$3");

  // Replace .home.username. with .home.[REDACTED].
  static const absl::NoDestructor<std::regex> home_dot_regex(
      absl::StrFormat(R"((/|\.|^)home\.(%s(?:[^/.]+\.)*?[^/.]+)(?=\.%s$|$))",
                      kNotPurelyReservedDot, kReservedSuffix));
  scrubbed = std::regex_replace(scrubbed, *home_dot_regex, "$1home.[REDACTED]");

  // Replace /users/username/ with /users/[REDACTED]/
  static const absl::NoDestructor<std::regex> users_slash_regex(absl::StrFormat(
      R"((/|\.|^)users/(%s(?:[^/]+/)*?[^/]+)(/?)(?=(?:/|\.)%s|$))",
      kNotPurelyReservedSlash, kReservedSuffix));
  scrubbed =
      std::regex_replace(scrubbed, *users_slash_regex, "$1users/[REDACTED]$3");

  // Replace .users.username. with .users.[REDACTED].
  static const absl::NoDestructor<std::regex> users_dot_regex(
      absl::StrFormat(R"((/|\.|^)users\.(%s(?:[^/.]+\.)*?[^/.]+)(?=\.%s$|$))",
                      kNotPurelyReservedDot, kReservedSuffix));
  scrubbed =
      std::regex_replace(scrubbed, *users_dot_regex, "$1users.[REDACTED]");

  // Deterministic FNV-1a hash to preserve cross-restart metric cardinality
  // stably
  auto fnv1a = [](const std::string& text) -> uint64_t {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (char c : text) {
      hash ^= static_cast<uint64_t>(c);
      hash *= 0x100000001b3ULL;
    }
    return hash;
  };

  auto replace_and_hash = [&](std::string& input, const std::regex& pattern) {
    std::sregex_iterator it(input.begin(), input.end(), pattern);
    std::sregex_iterator end;

    std::string result;
    size_t last_pos = 0;

    for (; it != end; ++it) {
      if (it->size() > 1) {
        size_t group_pos = it->position(1);
        size_t group_len = it->length(1);
        result += input.substr(last_pos, group_pos - last_pos);
        uint64_t hash = fnv1a(it->str(1));
        result += absl::StrFormat("[HASH:%016x]", hash);
        last_pos = group_pos + group_len;
      }
    }
    result += input.substr(last_pos);
    input = result;
  };

  static const absl::NoDestructor<std::regex> docker_regex(
      R"((?:^|[/.])([0-9a-fA-F]{64})(?=[/.1]|$))");
  replace_and_hash(scrubbed, *docker_regex);
  static const absl::NoDestructor<std::regex> short_hash_regex(
      "(?:docker|containerd|crio|lxc)/([0-9a-fA-F]{12})(?:/|$)");
  replace_and_hash(scrubbed, *short_hash_regex);
  static const absl::NoDestructor<std::regex> k8s_pod_regex(
      "([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]"
      "{12})");
  replace_and_hash(scrubbed, *k8s_pod_regex);
  static const absl::NoDestructor<std::regex> systemd_pod_regex(
      "pod([^.]+)\\.slice");
  replace_and_hash(scrubbed, *systemd_pod_regex);
  static const absl::NoDestructor<std::regex> systemd_user_regex(
      "user-([0-9]+)\\.slice");
  replace_and_hash(scrubbed, *systemd_user_regex);
  static const absl::NoDestructor<std::regex> systemd_session_regex(
      "session-([0-9]+)\\.scope");
  replace_and_hash(scrubbed, *systemd_session_regex);
  static const absl::NoDestructor<std::regex> run_uid_regex(
      "/run/user/([0-9]+)/");
  replace_and_hash(scrubbed, *run_uid_regex);
  static const absl::NoDestructor<std::regex> qemu_machine_regex(
      "machine-([^.]+)\\.scope");
  replace_and_hash(scrubbed, *qemu_machine_regex);
  static const absl::NoDestructor<std::regex> proc_slash_regex(
      "/proc/([0-9]+)/");
  replace_and_hash(scrubbed, *proc_slash_regex);
  static const absl::NoDestructor<std::regex> proc_dot_regex(
      "proc\\.([0-9]+)\\.");
  replace_and_hash(scrubbed, *proc_dot_regex);

  if (scrub_cache_.size() >= kMaxCacheSize) {
    scrub_cache_.clear();
  }
  scrub_cache_.emplace(path, scrubbed);

  return scrubbed;
}

void LogWriter::WriteMetric(int64_t timestamp, const std::string& source,
                            const std::string& metric_name, uint64_t value) {
  std::string scrubbed_name = ScrubPath(metric_name);

  std::string line = absl::StrFormat(
      "{\"timestamp\": %v, \"source\": \"%s\", \"metric\": \"%s\", \"value\": "
      "%v}\n",
      timestamp, source, scrubbed_name, value);

  if (out_stream_.is_open()) {
    out_stream_ << line;
  } else {
    // Fallback to stdout
    std::cout << line;
  }
}

}  // namespace guest_memory_metrics
