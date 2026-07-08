load("//third_party/bazel_rules/rules_cc/cc:cc_library.bzl", "cc_library")
load("//third_party/bazel_rules/rules_cc/cc:cc_test.bzl", "cc_test")

package(
    default_applicable_licenses = ["//third_party/guest_memory_metrics_agent:license"],
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "log_writer",
    srcs = ["log_writer.cc"],
    hdrs = ["log_writer.h"],
    deps = [
        "//third_party/absl/status",
        "//third_party/absl/strings:str_format",
    ],
)

cc_test(
    name = "log_writer_test",
    srcs = ["log_writer_test.cc"],
    deps = [
        ":log_writer",
        "//testing/base/public:gunit_main",
        "//third_party/absl/strings",
    ],
)

cc_library(
    name = "sampling_engine",
    srcs = ["sampling_engine.cc"],
    hdrs = ["sampling_engine.h"],
    linkopts = ["-lrt"],
    deps = [
        "//third_party/absl/time",
    ],
)

cc_library(
    name = "help_database",
    srcs = ["help_database.cc"],
    hdrs = ["help_database.h"],
    deps = [
        "//third_party/absl/base:no_destructor",
    ],
)

cc_test(
    name = "help_database_test",
    srcs = ["help_database_test.cc"],
    deps = [
        ":help_database",
        "//testing/base/public:gunit_main",
    ],
)

cc_library(
    name = "report_engine",
    srcs = ["report_engine.cc"],
    hdrs = ["report_engine.h"],
    deps = [
        "//third_party/absl/strings",
    ],
)

cc_test(
    name = "report_engine_test",
    srcs = ["report_engine_test.cc"],
    deps = [
        ":report_engine",
        "//testing/base/public:gunit_main",
    ],
)
