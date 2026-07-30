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

#include "engine/sampling_engine.h"

#include <algorithm>
#include <chrono>  // NOLINT
#include <functional>
#include <mutex>  // NOLINT(build/c++11)
#include <utility>

#include "absl/time/time.h"

namespace guest_memory_metrics {

SamplingEngine::SamplingEngine(absl::Duration interval, absl::Duration duration,
                               std::function<void()> callback)
    : interval_(interval),
      duration_(duration),
      callback_(std::move(callback)),
      running_(false) {}

SamplingEngine::~SamplingEngine() { Stop(); }

void SamplingEngine::Start() {
  {
    std::lock_guard<std::mutex> lock(mutex_);  // NOLINT(build/c++11)
    if (running_) return;
    running_ = true;
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  thread_ = std::thread(&SamplingEngine::Loop, this);
}

void SamplingEngine::Stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);  // NOLINT(build/c++11)
    running_ = false;
  }
  cv_.notify_one();
  if (thread_.joinable()) {
    thread_.join();
  }
}

void SamplingEngine::Loop() {
  auto start_time = std::chrono::steady_clock::now();
  auto duration_ns =
      std::chrono::nanoseconds(absl::ToInt64Nanoseconds(duration_));
  auto end_time = start_time + duration_ns;

  auto interval_ns =
      std::chrono::nanoseconds(absl::ToInt64Nanoseconds(interval_));
  if (interval_ns.count() <= 0) {
    interval_ns = std::chrono::seconds(1);
  }

  std::unique_lock<std::mutex> lock(mutex_);  // NOLINT(build/c++11)
  auto next_tick = start_time + interval_ns;

  while (running_) {
    auto now = std::chrono::steady_clock::now();
    if (now >= end_time) {
      break;
    }

    auto wait_until_time = std::min(next_tick, end_time);

    bool stopped =
        cv_.wait_until(lock, wait_until_time, [this] { return !running_; });
    if (stopped) {
      break;
    }

    now = std::chrono::steady_clock::now();
    if (now >= next_tick) {
      lock.unlock();
      callback_();
      lock.lock();
      // Calculate the next tick safely, preventing drift.
      next_tick += interval_ns;

      // If callback took too long and we missed multiple ticks, catch up.
      if (next_tick <= now) {
        next_tick = now + interval_ns;
      }
    }
  }
  running_ = false;
}

}  // namespace guest_memory_metrics
