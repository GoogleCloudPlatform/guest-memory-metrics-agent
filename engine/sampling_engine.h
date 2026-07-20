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

#ifndef GUEST_MEMORY_METRICS_AGENT_SAMPLING_ENGINE_H_
#define GUEST_MEMORY_METRICS_AGENT_SAMPLING_ENGINE_H_

#include <atomic>
#include <functional>
#include <thread>  // NOLINT

#include "absl/time/time.h"

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

#endif  // GUEST_MEMORY_METRICS_AGENT_SAMPLING_ENGINE_H_
