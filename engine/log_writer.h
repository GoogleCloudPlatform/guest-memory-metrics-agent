#ifndef THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_LOG_WRITER_H_
#define THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_LOG_WRITER_H_

#include <fstream>
#include <string>

#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/status/status.h"

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
  std::ofstream out_stream_;
  absl::flat_hash_map<std::string, std::string> scrub_cache_;
  static constexpr size_t kMaxCacheSize = 10000;
};

}  // namespace guest_memory_metrics

#endif  // THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_LOG_WRITER_H_
