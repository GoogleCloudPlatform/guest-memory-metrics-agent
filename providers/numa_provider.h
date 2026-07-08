#ifndef GUEST_MEMORY_METRICS_AGENT_NUMA_PROVIDER_H_
#define GUEST_MEMORY_METRICS_AGENT_NUMA_PROVIDER_H_

#include <string>

#include "third_party/guest_memory_metrics_agent/providers/metric_provider.h"

namespace guest_memory_metrics {

class NumaProvider : public MetricProvider {
 public:
  explicit NumaProvider(
      const std::string& base_path = "/sys/devices/system/node");
  ~NumaProvider() override = default;

  MetricSnapshot GetSnapshot() const override;

 private:
  std::string base_path_;
};

}  // namespace guest_memory_metrics

#endif  // GUEST_MEMORY_METRICS_AGENT_NUMA_PROVIDER_H_
