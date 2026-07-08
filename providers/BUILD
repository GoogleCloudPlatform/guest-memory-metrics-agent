load("//third_party/bazel_rules/rules_cc/cc:cc_library.bzl", "cc_library")
load("//third_party/bazel_rules/rules_cc/cc:cc_test.bzl", "cc_test")

package(
    default_applicable_licenses = ["//third_party/guest_memory_metrics_agent:license"],
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
        "//third_party/absl/strings",
        "//third_party/absl/time",
    ],
)

cc_test(
    name = "providers_test",
    srcs = ["providers_test.cc"],
    deps = [
        ":metrics",
        "//testing/base/public:gunit_main",
    ],
)
