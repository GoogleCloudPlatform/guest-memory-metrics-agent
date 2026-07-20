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

#ifndef GUEST_MEMORY_METRICS_AGENT_ENGINE_HELP_DATABASE_H_
#define GUEST_MEMORY_METRICS_AGENT_ENGINE_HELP_DATABASE_H_

#include <string>

namespace guest_memory_metrics {

// Returns help text or details for a given metric_name.
std::string GetHelpForMetric(const std::string& metric_name);

}  // namespace guest_memory_metrics

#endif  // GUEST_MEMORY_METRICS_AGENT_ENGINE_HELP_DATABASE_H_
