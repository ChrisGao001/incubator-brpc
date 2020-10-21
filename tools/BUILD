COPTS = [
    "-D__STDC_FORMAT_MACROS",
    "-DBTHREAD_USE_FAST_PTHREAD_MUTEX",
    "-D__const__=",
    "-D_GNU_SOURCE",
    "-DUSE_SYMBOLIZE",
    "-DNO_TCMALLOC",
    "-D__STDC_LIMIT_MACROS",
    "-D__STDC_CONSTANT_MACROS",
    "-fPIC",
    "-Wno-unused-parameter",
    "-fno-omit-frame-pointer",
    "-DGFLAGS_NS=google",
] + select({
    "//:with_glog": ["-DBRPC_WITH_GLOG=1"],
    "//conditions:default": ["-DBRPC_WITH_GLOG=0"],
})

proto_library(
    name = "view_proto",
    srcs = [
        "rpc_view/view.proto",
    ],
)

cc_proto_library(
    name = "cc_view_proto",
    deps = [
        ":view_proto",
    ],
)

cc_binary(
    name = "rpc_view",
    srcs = [
        "rpc_view/rpc_view.cpp",
    ],
    deps = [
        ":cc_view_proto",
        "//:brpc",
    ],
    copts = COPTS,
)

cc_binary(
    name = "rpc_replay",
    srcs = [
        "rpc_replay/rpc_replay.cpp",
        "rpc_replay/info_thread.cpp",
        "rpc_replay/info_thread.h",
    ],
    deps = [
        "//:brpc",
    ],
    copts = COPTS,
)


cc_binary(
    name = "rpc_press",
    srcs = glob([
        "rpc_press/*.cpp",
        "rpc_press/*.h",
    ]),
    deps = [
        "@com_google_protobuf//:protobuf",
        "@com_github_gflags_gflags//:gflags",
        "//:brpc",
        "//:butil",
    ],
    copts = COPTS,
    visibility = ["//visibility:public"],
)

