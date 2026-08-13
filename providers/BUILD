# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_cc//cc:defs.bzl", "cc_test")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "metrics",
    srcs = [
        "cgroup_provider.cc",
        "numa_provider.cc",
        "proc_provider.cc",
    ],
    hdrs = [
        "cgroup_provider.h",
        "metric_provider.h",
        "metric_snapshot.h",
        "numa_provider.h",
        "proc_provider.h",
    ],
    deps = [
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "providers_test",
    srcs = ["providers_test.cc"],
    deps = [
        ":metrics",
        "@com_google_googletest//:gtest_main",
    ],
)
