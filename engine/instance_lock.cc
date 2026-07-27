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

#include "engine/instance_lock.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace guest_memory_metrics {

InstanceLock::~InstanceLock() { Unlock(); }

InstanceLock::InstanceLock(InstanceLock&& other) noexcept : fd_(other.fd_) {
  other.fd_ = -1;
}

InstanceLock& InstanceLock::operator=(InstanceLock&& other) noexcept {
  if (this != &other) {
    Unlock();
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

void InstanceLock::Unlock() {
  if (fd_ >= 0) {
    flock(fd_, LOCK_UN);
    close(fd_);
    fd_ = -1;
  }
}

std::string InstanceLock::GetLockPath(const std::string& custom_path) {
  if (custom_path.empty()) {
    return absl::StrCat("/tmp/guest_memory_metrics_agent_", getuid(), ".lock");
  }
  return custom_path;
}

absl::StatusOr<InstanceLock> InstanceLock::TryAcquire(
    const std::string& lock_path) {
  int fd = open(lock_path.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) {
    if (errno == ELOOP) {
      return absl::PermissionDeniedError(
          "Lockfile is a symlink; aborting for security.");
    }
    return absl::InternalError(
        absl::StrFormat("Failed to open/create lock file '%s': %s", lock_path,
                        strerror(errno)));
  }

  if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
    int lock_errno = errno;
    close(fd);
    if (lock_errno == EWOULDBLOCK || lock_errno == EAGAIN) {
      return absl::AlreadyExistsError(absl::StrFormat(
          "Another instance of Guest Memory Metrics Agent is already running "
          "(lock file: '%s')",
          lock_path));
    }
    return absl::InternalError(absl::StrFormat(
        "Failed to lock file '%s': %s", lock_path, strerror(lock_errno)));
  }

  // Record process ID in lock file for administrative visibility
  if (ftruncate(fd, 0) == 0) {
    std::string pid_str = absl::StrCat(getpid(), "\n");
    ssize_t bytes_written = write(fd, pid_str.c_str(), pid_str.size());
    if (bytes_written != static_cast<ssize_t>(pid_str.size())) {
      // Writing PID failed partially or fully.
      // Since lock is acquired, we proceed but process inspection via lockfile may fail.
    }
  }

  return InstanceLock(fd);
}

}  // namespace guest_memory_metrics
