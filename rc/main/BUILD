load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "rdma_handler",
    srcs = ["rdma_handler.cpp"],
    hdrs = ["rdma_handler.h"],
    deps = [
        "@usr//:ibverbs"
    ],
)


cc_library(
    name = "udp_server",
    srcs = ["udp_server.cpp"],
    hdrs = ["udp_server.h"],
    deps = ["rdma_handler"]
)


cc_binary(
    name = "app",
    srcs = ["app.cpp"],
    deps = [":rdma_handler", 
            ":udp_server"]
    
)
