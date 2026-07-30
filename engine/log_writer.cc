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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"

namespace guest_memory_metrics {

LogWriter::LogWriter(const std::string& output_path)
    : output_path_(output_path) {}

LogWriter::~LogWriter() { Close(); }

absl::Status LogWriter::Open() {
  absl::MutexLock lock(io_mutex_);
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
  absl::MutexLock lock(io_mutex_);
  if (out_stream_.is_open()) {
    out_stream_.close();
  }
}

std::string LogWriter::ScrubPath(const std::string& path) {
  {
    absl::MutexLock lock(mutex_);
    auto it = scrub_cache_.find(path);
    if (it != scrub_cache_.end()) {
      return it->second;
    }
  }

  static const absl::NoDestructor<absl::flat_hash_set<std::string>>
      kMetricAllowList([] {
        return absl::flat_hash_set<std::string>{"memory",
                                                "proc",
                                                "cpu",
                                                "cpuacct",
                                                "cpuset",
                                                "cgroup",
                                                "io",
                                                "blkio",
                                                "pids",
                                                "net_cls",
                                                "net_prio",
                                                "devices",
                                                "freezer",
                                                "hugetlb",
                                                "perf_event",
                                                "rdma",
                                                "misc",
                                                "numa",
                                                "stat",
                                                "vmstat",
                                                "meminfo",
                                                "node",
                                                "tasks",
                                                "procs",
                                                "notify_on_release",
                                                "current",
                                                "max",
                                                "swap",
                                                "events",
                                                "high",
                                                "low",
                                                "min",
                                                "oom",
                                                "peak",
                                                "pressure",
                                                "weight",
                                                "zswap",
                                                "idle",
                                                "failcnt",
                                                "limit_in_bytes",
                                                "usage_in_bytes",
                                                "memfree",
                                                "memtotal",
                                                "memavailable",
                                                "buffers",
                                                "cached",
                                                "swapcached",
                                                "active",
                                                "inactive",
                                                "unevictable",
                                                "mlocked",
                                                "swaptotal",
                                                "swapfree",
                                                "dirty",
                                                "writeback",
                                                "anonpages",
                                                "mapped",
                                                "shmem",
                                                "kreclaimable",
                                                "slab",
                                                "sreclaimable",
                                                "sunreclaim",
                                                "kernelstack",
                                                "pagetables",
                                                "n_highmemory",
                                                "vmalloctotal",
                                                "vmallocused",
                                                "vmallocchunk",
                                                "percpu",
                                                "hardwarecorrupted",
                                                "anonhugepages",
                                                "shmemhugepages",
                                                "shmempmdmapped",
                                                "filehugepages",
                                                "filepmdmapped",
                                                "hugepages_total",
                                                "hugepages_free",
                                                "hugepages_rsvd",
                                                "hugepages_surp",
                                                "hugepagesize",
                                                "status",
                                                "cmdline",
                                                "statm",
                                                "eth",
                                                "nvme",
                                                "sd",
                                                "vd",
                                                "net",
                                                "disk",
                                                "block"};
      }());

  static const absl::NoDestructor<absl::flat_hash_set<std::string>> kAllowList(
      [] {
        absl::flat_hash_set<std::string> set = *kMetricAllowList;
        set.insert({"home",
                    "users",
                    "slice",
                    "scope",
                    "pod",
                    "run",
                    "docker",
                    "containerd",
                    "crio",
                    "lxc",
                    "machine",
                    "session",
                    "user",
                    "system",
                    "sys",
                    "fs",
                    "var",
                    "lib",
                    "mounts",
                    "shm",
                    "containers",
                    "kubelet",
                    "kubepods",
                    "kubepods-besteffort",
                    "kubepods-burstable",
                    "kubepods-guaranteed",
                    "systemd",
                    "dev",
                    "etc",
                    "tmp",
                    "opt",
                    "usr",
                    "bin",
                    "sbin",
                    "init",
                    "kernel",
                    "app",
                    "service",
                    "prefix"});
        return set;
      }());

  auto is_allowed_token =
      [&](const std::string& lower_tok,
          const absl::flat_hash_set<std::string>& allow_set) -> bool {
    if (allow_set.contains(lower_tok)) {
      return true;
    }
    size_t first_digit = lower_tok.find_first_of("0123456789");
    if (first_digit != std::string::npos && first_digit > 0) {
      std::string prefix = lower_tok.substr(0, first_digit);

      // FIX 1: Only allow dynamic trailing digits for pure metrics!
      // This forces PII like `user1000` or `pod1234` to fail and be hashed.
      if (kMetricAllowList->contains(prefix)) {
        std::string suffix = lower_tok.substr(first_digit);
        // Handle nvme0n1 format (digits + 'n' + digits)
        if (prefix == "nvme") {
          size_t n_pos = suffix.find('n');
          if (n_pos != std::string::npos && n_pos > 0) {
            bool valid_nvme = true;
            for (size_t i = 0; i < suffix.length(); ++i) {
              if (i == n_pos) continue;
              if (suffix[i] < '0' || suffix[i] > '9') {
                valid_nvme = false;
                break;
              }
            }
            if (valid_nvme) return true;
          }
        }
        bool all_digits = true;
        for (char d : suffix) {
          if (d < '0' || d > '9') {
            all_digits = false;
            break;
          }
        }
        if (all_digits) return true;
      }
    }
    // Handle block devices like sda, sda1, vda, vda1
    if ((absl::StartsWith(lower_tok, "sd") ||
         absl::StartsWith(lower_tok, "vd")) &&
        lower_tok.length() >= 3 && lower_tok.length() <= 5) {
      if (absl::ascii_isalpha(lower_tok[2])) {
        bool valid_blk = true;
        for (size_t i = 3; i < lower_tok.length(); ++i) {
          if (!absl::ascii_isdigit(lower_tok[i])) {
            valid_blk = false;
            break;
          }
        }
        if (valid_blk) return true;
      }
    }
    return false;
  };

  // FIX 3: Seed the hash salt using the host's machine-id.
  // This defeats pre-computed rainbow tables but remains stable across
  // agent restarts, protecting metric cardinality in time-series databases.
  static const uint64_t kHashSalt = []() -> uint64_t {
    std::ifstream f("/etc/machine-id");
    std::string id;
    if (f >> id && !id.empty()) {
      uint64_t hash = 0xcbf29ce484222325ULL;
      for (char c : id) {
        hash ^= static_cast<uint64_t>(static_cast<uint8_t>(c));
        hash *= 0x100000001b3ULL;
      }
      return hash;
    }
    // Fallback constant if machine-id is unreadable
    return 0xcbf29ce484222325ULL;
  }();

  auto fnv1a = [](const std::string& text) -> uint64_t {
    uint64_t hash = kHashSalt;
    for (char c : text) {
      hash ^= static_cast<uint64_t>(static_cast<uint8_t>(c));
      hash *= 0x100000001b3ULL;
    }
    return hash;
  };

  std::string working_path = path;

  // Step 1: User directory redaction (/home/ and /users/)
  auto handle_user_redaction = [&](const std::string& key) {
    size_t pos = 0;
    while ((pos = working_path.find(key, pos)) != std::string::npos) {
      bool valid_start = (pos == 0 || working_path[pos - 1] == '/' ||
                          working_path[pos - 1] == '.');
      size_t after_key = pos + key.length();
      bool valid_delim =
          (after_key < working_path.length() &&
           (working_path[after_key] == '/' || working_path[after_key] == '.'));

      if (valid_start && valid_delim) {
        size_t user_start = after_key + 1;

        size_t suffix_start = working_path.length();
        size_t curr = working_path.length();
        while (curr > user_start) {
          size_t prev_delim = working_path.find_last_of("/.", curr - 1);
          size_t tok_start =
              (prev_delim == std::string::npos || prev_delim < user_start)
                  ? user_start
                  : prev_delim + 1;
          std::string tok = working_path.substr(tok_start, curr - tok_start);
          if (!tok.empty() &&
              is_allowed_token(absl::AsciiStrToLower(tok), *kMetricAllowList)) {
            suffix_start =
                (prev_delim != std::string::npos && prev_delim >= user_start)
                    ? prev_delim
                    : user_start;
            if (prev_delim == std::string::npos || prev_delim < user_start)
              break;
            curr = prev_delim;
          } else {
            break;
          }
        }

        if (suffix_start > user_start) {
          std::string prefix_part = working_path.substr(0, user_start);
          std::string suffix_part = working_path.substr(suffix_start);
          working_path = prefix_part + "[REDACTED]" + suffix_part;
        } else if (suffix_start == user_start &&
                   suffix_start == working_path.length()) {
          std::string prefix_part = working_path.substr(0, user_start);
          working_path = prefix_part + "[REDACTED]";
        }
        break;
      }
      pos += key.length();
    }
  };

  handle_user_redaction("home");
  handle_user_redaction("users");

  // Step 2: Token Allowlist Pass for remaining tokens
  std::string scrubbed;
  scrubbed.reserve(working_path.size() * 2);
  std::string current_token;

  for (size_t i = 0; i <= working_path.size(); ++i) {
    char c = (i < working_path.size()) ? working_path[i] : '\0';
    if (c == '/' || c == '.' || c == '\0') {
      if (!current_token.empty()) {
        std::string lower_token = absl::AsciiStrToLower(current_token);
        if (current_token == "[REDACTED]" ||
            is_allowed_token(lower_token, *kAllowList)) {
          scrubbed += current_token;
        } else {
          bool prefix_handled = false;
          static constexpr const char* kPrefixes[] = {
              "pod",   "user-",       "session-", "machine-",       "docker-",
              "crio-", "containerd-", "lxc-",     "cri-containerd-"};
          for (const char* prefix : kPrefixes) {
            if (absl::StartsWith(lower_token, prefix)) {
              std::string orig_prefix = current_token.substr(0, strlen(prefix));
              std::string suffix = current_token.substr(strlen(prefix));
              scrubbed +=
                  absl::StrFormat("%s[HASH:%016x]", orig_prefix, fnv1a(suffix));
              prefix_handled = true;
              break;
            }
          }
          if (!prefix_handled) {
            scrubbed += absl::StrFormat("[HASH:%016x]", fnv1a(current_token));
          }
        }
        current_token.clear();
      }
      if (c != '\0') {
        scrubbed += c;
      }
    } else {
      current_token += c;
    }
  }

  {
    absl::MutexLock lock(mutex_);
    if (scrub_cache_.size() >= kMaxCacheSize) {
      scrub_cache_.clear();
    }
    scrub_cache_.emplace(path, scrubbed);
  }

  return scrubbed;
}

void LogWriter::WriteMetric(int64_t timestamp, const std::string& source,
                            const std::string& metric_name, uint64_t value) {
  std::string scrubbed_name = ScrubPath(metric_name);

  // FIX 2: Strict JSON RFC 8259 Compliant Escaping with Zero-Allocation
  // Fast-Path
  auto json_escape = [](std::string_view input,
                        std::string* scratch) -> std::string_view {
    bool needs_escape = false;
    for (unsigned char c : input) {
      if (c == '"' || c == '\\' || c < 0x20) {
        needs_escape = true;
        break;
      }
    }
    if (!needs_escape) {
      return input;
    }

    scratch->clear();
    scratch->reserve(input.length() + 16);
    for (unsigned char c : input) {
      switch (c) {
        case '"':
          *scratch += "\\\"";
          break;
        case '\\':
          *scratch += "\\\\";
          break;
        case '\b':
          *scratch += "\\b";
          break;
        case '\f':
          *scratch += "\\f";
          break;
        case '\n':
          *scratch += "\\n";
          break;
        case '\r':
          *scratch += "\\r";
          break;
        case '\t':
          *scratch += "\\t";
          break;
        default:
          if (c < 0x20) {
            *scratch += absl::StrFormat("\\u%04x", c);
          } else {
            *scratch += c;
          }
      }
    }
    return *scratch;
  };

  std::string source_scratch;
  std::string metric_scratch;
  std::string_view escaped_source = json_escape(source, &source_scratch);
  std::string_view escaped_metric = json_escape(scrubbed_name, &metric_scratch);

  std::string line = absl::StrFormat(
      "{\"timestamp\": %v, \"source\": \"%s\", \"metric\": \"%s\", \"value\": "
      "%v}\n",
      timestamp, escaped_source, escaped_metric, value);

  // FIX 5: Use a separate io_mutex_ here so disk I/O doesn't freeze the
  // scrubbing cache
  absl::MutexLock lock(io_mutex_);
  if (out_stream_.is_open()) {
    out_stream_ << line;
  } else {
    std::cout << line;
  }
}

}  // namespace guest_memory_metrics
