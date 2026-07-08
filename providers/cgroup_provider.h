#ifndef GUEST_MEMORY_METRICS_AGENT_CGROUP_PROVIDER_H_
#define GUEST_MEMORY_METRICS_AGENT_CGROUP_PROVIDER_H_

#include <string>

#include "third_party/guest_memory_metrics_agent/providers/metric_provider.h"

namespace guest_memory_metrics {

class CgroupProvider : public MetricProvider {
 public:
  explicit CgroupProvider(const std::string& base_path = "/sys/fs/cgroup");
  MetricSnapshot GetSnapshot() const override;

 private:
  std::string base_path_;
};

}  // namespace guest_memory_metrics

#endif  // GUEST_MEMORY_METRICS_AGENT_CGROUP_PROVIDER_H_
