# external dependencies

# android ndk
# currently only ndk version r21 or below is supported from bazel. any ndks newer than version r21 doesn't have `platforms` folder and that cause the following error.
# "build aborted: Expected directory at <NDKPATH>/platforms but it is not a directory or it does not exist."

android_ndk_repository(
    name="androidndk",
    #path = "<NDKPATH>", #Uncomment this line to use specific ndk. Otherwise `ANDROID_NDK_HOME` is used.
    api_level = 21, #Target api level. Not the same thing with ndk version.
)