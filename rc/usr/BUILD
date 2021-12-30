load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library (
    name = "ibverbs",
    hdrs = glob(["include/infiniband/**"]),
    srcs = ["lib64/libibverbs.so"],
    visibility = ["//visibility:public"],
)