#ifndef THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_ENGINE_HELP_DATABASE_H_
#define THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_ENGINE_HELP_DATABASE_H_

#include <string>

namespace guest_memory_metrics {

// Returns help text or details for a given metric_name.
std::string GetHelpForMetric(const std::string& metric_name);

}  // namespace guest_memory_metrics

#endif  // THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_ENGINE_HELP_DATABASE_H_
