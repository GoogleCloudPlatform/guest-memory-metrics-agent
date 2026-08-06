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
