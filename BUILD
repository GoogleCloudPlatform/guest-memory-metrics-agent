load("//third_party/bazel_rules/rules_cc/cc:cc_binary.bzl", "cc_binary")
load("//tools/build_defs/license:license.bzl", "license")

licenses(["notice"])

package(
    default_applicable_licenses = ["//third_party/guest_memory_metrics_agent:license"],
    default_visibility = ["//visibility:public"],
)

exports_files(["LICENSE"])

license(
    name = "license",
    license_text = "LICENSE",
)

cc_binary(
    name = "kernel_metrics_agent",
    srcs = ["main.cc"],
    linkstatic = True,
    deps = [
        "//third_party/absl/flags:flag",
        "//third_party/absl/flags:parse",
        "//third_party/absl/time",
        "//engine:help_database",
        "//engine:log_writer",
        "//engine:report_engine",
        "//engine:sampling_engine",
        "//providers:metrics",
    ],
)
