#ifndef GUEST_MEMORY_METRICS_AGENT_METRIC_SNAPSHOT_H_
#define GUEST_MEMORY_METRICS_AGENT_METRIC_SNAPSHOT_H_

#include <cstdint>
#include <map>
#include <string>

namespace guest_memory_metrics {

struct MetricSnapshot {
  int64_t timestamp_ms;
  std::map<std::string, uint64_t> metrics;
};

}  // namespace guest_memory_metrics

#endif  // GUEST_MEMORY_METRICS_AGENT_METRIC_SNAPSHOT_H_
