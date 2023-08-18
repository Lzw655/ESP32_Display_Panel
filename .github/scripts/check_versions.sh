#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: $0 <version>"
    # exit 1
fi

# 获取第一个输入参数作为待比较版本号
latest_version="$1"

# 指定目录路径
target_directory="./"

echo "checking directory: ${target_directory}"

# 检查文件是否存在函数
check_file_exists() {
    if [ ! -f "$1" ]; then
        echo "File '$1' not found."
        return 1
    fi
    return 0
}

# 函数：比较版本号大小
# 输入参数：$1 第一个版本号，$2 第二个版本号
# 返回值：0 表示版本号相等，1 表示第一个版本号大，2 表示第二个版本号大，3 表示版本号格式不正确
compare_versions() {
    version_regex="^v[0-9]+\.[0-9]+\.[0-9]+$"

    if [[ ! $1 =~ $version_regex || ! $2 =~ $version_regex ]]; then
        return 3
    fi

    version1=$(echo "$1" | cut -c 2-)  # 去掉版本号前的 'v'
    version2=$(echo "$2" | cut -c 2-)

    IFS='.' read -ra v1_parts <<< "$version1"
    IFS='.' read -ra v2_parts <<< "$version2"

    for ((i=0; i<${#v1_parts[@]}; i++)); do
        if [ "${v1_parts[$i]}" -lt "${v2_parts[$i]}" ]; then
            return 2
        elif [ "${v1_parts[$i]}" -gt "${v2_parts[$i]}" ]; then
            return 1
        fi
    done

    return 0
}

echo "checking file: library.properties"
# 检查 "library.properties" 是否存在
check_file_exists "${target_directory}/library.properties"
if [ $? -ne 0 ]; then
    exit 1
fi
# 读取文件中的 version 信息
arduino_version=v$(grep -E '^version=' "${target_directory}/library.properties" | cut -d '=' -f 2)
echo "Get Arduino version: ${arduino_version}"
# 判断 Arduino Library 和 最新 Release 版本号大小
compare_versions "${arduino_version}" "${latest_version}"
result=$?
if [ ${result} -ne 1 ]; then
    if [ ${result} -eq 3 ]; then
        echo "Arduino version (${arduino_version}) is incorrect."
    else
        echo "Arduino version (${arduino_version}) is not greater than the latest release version (${latest_version})."
        # exit 1
    fi
fi

echo "checking file: idf_component.yml"
# 检查 "idf_component.yml" 是否存在
check_file_exists "${target_directory}/idf_component.yml"
if [ $? -eq 0 ]; then
    # 读取文件中的 version 信息
    idf_version=v$(grep -E '^version:' "${target_directory}/idf_component.yml" | awk -F'"' '{print $2}')
    echo "Get IDF component version: ${idf_version}"
    # 判断 IDF Component 和 Arduino Library 版本号大小
    compare_versions ${idf_version} ${arduino_version}
    result=$?
    if [ ${result} -ne 0 ]; then
        if [ ${result} -eq 3 ]; then
            echo "IDF component version (${idf_version}) is incorrect."
        else
        echo "IDF component version (${idf_version}) is not equal to the Arduino version (${arduino_version})."
            # exit 1
        fi
    fi
    # 判断 IDF Component 和 最新 Release 版本号大小
    compare_versions ${idf_version} ${latest_version}
    result=$?
    if [ ${result} -ne 1 ]; then
        if [ ${result} -eq 3 ]; then
            echo "IDF component version (${idf_version}) is incorrect."
        else
            echo "IDF component version (${idf_version}) is not greater than the latest release version (${latest_version})."
            # exit 1
        fi
    fi
fi
