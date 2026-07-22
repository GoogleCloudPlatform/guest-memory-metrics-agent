load("@rules_cc//cc:defs.bzl", "cc_binary")
load("//tools/build_defs/license:license.bzl", "license")

licenses(["notice"])

package(
    default_applicable_licenses = ["//:license"],
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
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/time",
        "//engine:help_database",
        "//engine:log_writer",
        "//engine:report_engine",
        "//engine:sampling_engine",
        "//providers:metrics",
    ],
)
