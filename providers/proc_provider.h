#ifndef GUEST_MEMORY_METRICS_AGENT_PROC_PROVIDER_H_
#define GUEST_MEMORY_METRICS_AGENT_PROC_PROVIDER_H_

#include <string>

#include "third_party/guest_memory_metrics_agent/providers/metric_provider.h"

namespace guest_memory_metrics {

class ProcProvider : public MetricProvider {
 public:
  explicit ProcProvider(const std::string& base_path = "/proc");
  MetricSnapshot GetSnapshot() const override;

 private:
  std::string base_path_;
};

}  // namespace guest_memory_metrics

#endif  // GUEST_MEMORY_METRICS_AGENT_PROC_PROVIDER_H_
