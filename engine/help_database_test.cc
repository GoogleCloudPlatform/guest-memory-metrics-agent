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

#include "engine/help_database.h"

#include <string>

#include "gtest/gtest.h"

namespace guest_memory_metrics {
namespace {

TEST(HelpDatabaseTest, KnownMetricReturnsHelpText) {
  // Test that calling it with a known metric returns a non-empty string
  // that isn't the default fallback. Using "pgmajfault" as the known metric.
  std::string result = GetHelpForMetric("pgmajfault");
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result, "No help available for this metric.");
}

TEST(HelpDatabaseTest, UnknownMetricReturnsFallbackText) {
  // Test that calling it with an unknown metric returns the fallback default
  // string.
  std::string result = GetHelpForMetric("UnknownMetric123");
  EXPECT_EQ(result, "No help available for this metric.");
}

}  // namespace
}  // namespace guest_memory_metrics
