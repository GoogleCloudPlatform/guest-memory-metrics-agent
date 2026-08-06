load("@rules_cc//cc:defs.bzl", "cc_binary")




package(

    default_visibility = ["//visibility:public"],
)

exports_files(["LICENSE"])



cc_binary(
    name = "kernel_metrics_agent",
    srcs = ["main.cc"],
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
