load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_cc//cc:defs.bzl", "cc_test")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "log_writer",
    srcs = ["log_writer.cc"],
    hdrs = ["log_writer.h"],
    deps = [
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "log_writer_test",
    srcs = ["log_writer_test.cc"],
    deps = [
        ":log_writer",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "sampling_engine",
    srcs = ["sampling_engine.cc"],
    hdrs = ["sampling_engine.h"],
    deps = [
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "help_database",
    srcs = ["help_database.cc"],
    hdrs = ["help_database.h"],
)

cc_test(
    name = "help_database_test",
    srcs = ["help_database_test.cc"],
    deps = [
        ":help_database",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "report_engine",
    srcs = ["report_engine.cc"],
    hdrs = ["report_engine.h"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "report_engine_test",
    srcs = ["report_engine_test.cc"],
    deps = [
        ":report_engine",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "instance_lock",
    srcs = ["instance_lock.cc"],
    hdrs = ["instance_lock.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "instance_lock_test",
    srcs = ["instance_lock_test.cc"],
    deps = [
        ":instance_lock",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)
