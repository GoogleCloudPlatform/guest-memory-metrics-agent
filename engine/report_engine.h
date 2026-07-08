#ifndef THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_REPORT_ENGINE_H_
#define THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_REPORT_ENGINE_H_

#include <string>

namespace guest_memory_metrics {

class ReportEngine {
 public:
  ReportEngine() = default;
  ~ReportEngine() = default;

  void GenerateReport(const std::string& input_path,
                      const std::string& start_str, const std::string& end_str);
};

}  // namespace guest_memory_metrics

#endif  // THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_REPORT_ENGINE_H_
