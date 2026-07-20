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

#ifndef GUEST_MEMORY_METRICS_AGENT_LOG_WRITER_H_
#define GUEST_MEMORY_METRICS_AGENT_LOG_WRITER_H_

#include <fstream>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"

namespace guest_memory_metrics {

class LogWriter {
 public:
  explicit LogWriter(const std::string& output_path = "");
  ~LogWriter();

  absl::Status Open();
  void Close();

  // Write a metric to the log, scrubbing PII like container ID and path.
  void WriteMetric(int64_t timestamp, const std::string& source,
                   const std::string& metric_name, uint64_t value);

 private:
  std::string ScrubPath(const std::string& path);

  std::string output_path_;
  std::ofstream out_stream_ ABSL_GUARDED_BY(io_mutex_);
  mutable absl::Mutex mutex_;
  mutable absl::Mutex io_mutex_;
  absl::flat_hash_map<std::string, std::string> scrub_cache_
      ABSL_GUARDED_BY(mutex_);
  static constexpr size_t kMaxCacheSize = 10000;
};

}  // namespace guest_memory_metrics

#endif  // GUEST_MEMORY_METRICS_AGENT_LOG_WRITER_H_
