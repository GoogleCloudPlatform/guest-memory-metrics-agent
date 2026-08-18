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

load("@rules_cc//cc:defs.bzl", "cc_binary")


package(
    default_visibility = ["//visibility:public"],
)

exports_files(["LICENSE"])


cc_binary(
    name = "kernel_metrics_agent",
    srcs = ["main.cc"],
    features = ["fully_static_link"],
    linkstatic = True,
    deps = [
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/time",
        "//engine:help_database",
        "//engine:instance_lock",
        "//engine:log_writer",
        "//engine:report_engine",
        "//engine:sampling_engine",
        "//providers:metrics",
    ],
)
