#ifndef THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_SAMPLING_ENGINE_H_
#define THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_SAMPLING_ENGINE_H_

#include <atomic>
#include <functional>
#include <thread>  // NOLINT

#include "third_party/absl/time/time.h"

namespace guest_memory_metrics {

class SamplingEngine {
 public:
  SamplingEngine(absl::Duration interval, absl::Duration duration,
                 std::function<void()> callback);
  ~SamplingEngine();

  void Start();
  void Stop();

 private:
  void Loop();

  absl::Duration interval_;
  absl::Duration duration_;
  std::function<void()> callback_;
  std::atomic<bool> running_;
  std::thread thread_;
};

}  // namespace guest_memory_metrics

#endif  // THIRD_PARTY_GUEST_MEMORY_METRICS_AGENT_SAMPLING_ENGINE_H_
