#!/bin/bash
## 
## Release script for 'blockevent for Android' application. 
## Builds the application executables for multiple platforms into 'releases' directory.
## Can take version number as an argument. It's optional.
## Valid version numbers: 1.0, v1.0, 1.0.0, v1.0.0, 2.9.9, v99.9.9
##

out_dir="releases"
bin_dir="bazel-bin"
apps=("src:blockevent")
arch_attr="--config"
archs=("android_arm" "android_arm64" "android_x86" "android_x86_64")
version=""
declare -a output_files
declare -a default_bazel_flags=(build -c opt)
declare -a bazel_flags

if [ $# -gt 1 ]; then
    echo "Too many arguments. Aborted." >&2
    exit 1
elif [ $# -eq 1 ]; then
    [[ "$1" =~ ^v?(([0-9]{1,2})\.([0-9])(\.([0-9]))?)$ ]] && version="_${1#v}" || \
        >&2 echo "Invalid Version: '$1'. Build starting versionless." 
else
    echo "Build starting versionless." 
fi

for app in ${apps[@]}; do
    bazel_flags=("${default_bazel_flags[@]}")
    bazel_flags+=("//"${app})
    bazel_flags+=(${arch_attr}=)

    for arch in ${archs[@]}; do
        output_filename=${app##*:}${version}_${arch#android_}

        bazel "${bazel_flags[@]}${arch}"
        [[ $? -ne 0 ]] && continue
        mkdir -p "${out_dir}" && cp -f "${bin_dir}/${app%%:*}/${app##*:}" "${out_dir}/${output_filename}"
        [[ $? -eq 0 ]] || >&2 echo "Build complete successfully. Error occured when copying: '${output_filename}'" 
        echo "`sha256sum ${out_dir}/${output_filename} | cut -d " " -f1` ${output_filename}" >> \
            "${out_dir}/checksums${version}.txt"
        
        output_files+=(${out_dir}/${output_filename})
    done
done

echo "${#output_files[@]} build completed successfully (Total: `expr ${#apps[@]} \* ${#archs[@]}`)"
for out in ${output_files[@]}; do
    file $out
done