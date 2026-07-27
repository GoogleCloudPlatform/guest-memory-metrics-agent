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

#ifndef GUEST_MEMORY_METRICS_AGENT_ENGINE_INSTANCE_LOCK_H_
#define GUEST_MEMORY_METRICS_AGENT_ENGINE_INSTANCE_LOCK_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace guest_memory_metrics {

class InstanceLock {
 public:
  InstanceLock() : fd_(-1) {}
  ~InstanceLock();

  // Non-copyable
  InstanceLock(const InstanceLock&) = delete;
  InstanceLock& operator=(const InstanceLock&) = delete;

  // Moveable
  InstanceLock(InstanceLock&& other) noexcept;
  InstanceLock& operator=(InstanceLock&& other) noexcept;

  // Attempts to acquire an exclusive flock on lock_path.
  // Returns InstanceLock object if successful.
  // Returns absl::AlreadyExistsError if another instance already holds the
  // lock. Returns absl::InternalError if file cannot be opened/created.
  static absl::StatusOr<InstanceLock> TryAcquire(const std::string& lock_path);

  // Returns the global lock file path for the agent.
  // Defaults to "/tmp/guest_memory_metrics_agent.lock".
  // If custom_path is non-empty, custom_path is returned (useful for testing).
  static std::string GetLockPath(const std::string& custom_path = "");

  void Unlock();
  bool IsLocked() const { return fd_ >= 0; }

 private:
  explicit InstanceLock(int fd) : fd_(fd) {}
  int fd_;
};

}  // namespace guest_memory_metrics

#endif  // GUEST_MEMORY_METRICS_AGENT_ENGINE_INSTANCE_LOCK_H_
