load("@rules_cc//cc:defs.bzl", "cc_binary")

# "-fPIE", "-pie" opts used to get position-independent executables. 
# AOSP and some distributions requires this opts.
cc_binary(
    name = "blockevent",
    srcs = ["blockevent.c"],
    copts = ["-fPIE"],
    linkopts = ["-fPIE", "-pie"],
)