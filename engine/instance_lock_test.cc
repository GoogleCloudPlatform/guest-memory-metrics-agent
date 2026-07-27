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

#include <unistd.h>

#include <fstream>
#include <string>

#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace guest_memory_metrics {
namespace {

TEST(InstanceLockTest, GetLockPathDefault) {
  EXPECT_EQ(InstanceLock::GetLockPath(""),
            absl::StrCat("/tmp/guest_memory_metrics_agent_", getuid(),
                         ".lock"));
}

TEST(InstanceLockTest, GetLockPathCustom) {
  EXPECT_EQ(InstanceLock::GetLockPath("/tmp/custom.lock"),
            "/tmp/custom.lock");
}

TEST(InstanceLockTest, AcquireAndReleaseLock) {
  std::string lock_path =
      absl::StrCat(testing::TempDir(), "/test_instance_lock_1.lock");
  unlink(lock_path.c_str());

  {
    auto lock_or = InstanceLock::TryAcquire(lock_path);
    ASSERT_TRUE(lock_or.ok()) << lock_or.status().message();
    EXPECT_TRUE(lock_or->IsLocked());

    // Verify PID was recorded correctly
    std::ifstream ifs(lock_path);
    std::string stored_pid;
    std::getline(ifs, stored_pid);
    EXPECT_EQ(stored_pid, std::to_string(getpid()));
    ifs.close();

    // Trying to acquire the lock again while held should fail with
    // AlreadyExists
    auto second_lock_or = InstanceLock::TryAcquire(lock_path);
    EXPECT_FALSE(second_lock_or.ok());
    EXPECT_TRUE(absl::IsAlreadyExists(second_lock_or.status()));
  }

  // After scope exits, lock should be released and can be acquired again
  auto third_lock_or = InstanceLock::TryAcquire(lock_path);
  EXPECT_TRUE(third_lock_or.ok()) << third_lock_or.status().message();

  unlink(lock_path.c_str());
}

TEST(InstanceLockTest, ManualUnlock) {
  std::string lock_path =
      absl::StrCat(testing::TempDir(), "/test_instance_lock_2.lock");
  unlink(lock_path.c_str());

  auto lock_or = InstanceLock::TryAcquire(lock_path);
  ASSERT_TRUE(lock_or.ok());
  EXPECT_TRUE(lock_or->IsLocked());

  lock_or->Unlock();
  EXPECT_FALSE(lock_or->IsLocked());

  auto second_lock_or = InstanceLock::TryAcquire(lock_path);
  EXPECT_TRUE(second_lock_or.ok());

  unlink(lock_path.c_str());
}

}  // namespace
}  // namespace guest_memory_metrics
