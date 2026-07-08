#ifndef GUEST_MEMORY_METRICS_AGENT_METRIC_PROVIDER_H_
#define GUEST_MEMORY_METRICS_AGENT_METRIC_PROVIDER_H_

#include "third_party/guest_memory_metrics_agent/providers/metric_snapshot.h"

namespace guest_memory_metrics {

class MetricProvider {
 public:
  virtual ~MetricProvider() = default;

  // Implementations must be thread-safe so they can be queried concurrently.
  virtual MetricSnapshot GetSnapshot() const = 0;
};

}  // namespace guest_memory_metrics

#endif  // GUEST_MEMORY_METRICS_AGENT_METRIC_PROVIDER_H_
