load("//third_party/bazel_rules/rules_cc/cc:cc_binary.bzl", "cc_binary")
load("//tools/build_defs/license:license.bzl", "license")

licenses(["notice"])

package(
    default_applicable_licenses = ["//third_party/guest_memory_metrics_agent:license"],
    default_visibility = ["//visibility:public"],
)

license(
    name = "license",
    package_name = "guest_memory_metrics_agent",
)

exports_files(["LICENSE"])

cc_binary(
    name = "kernel_metrics_agent",
    srcs = ["main.cc"],
    deps = [
        "//third_party/absl/flags:flag",
        "//third_party/absl/flags:parse",
        "//third_party/absl/time",
        "//third_party/guest_memory_metrics_agent/engine:help_database",
        "//third_party/guest_memory_metrics_agent/engine:log_writer",
        "//third_party/guest_memory_metrics_agent/engine:report_engine",
        "//third_party/guest_memory_metrics_agent/engine:sampling_engine",
        "//third_party/guest_memory_metrics_agent/providers:metrics",
    ],
)
