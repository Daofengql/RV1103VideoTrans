#!/bin/bash
set -e

# 固定设置
TARGET_SOC="rv1106"
TARGET_ARCH="arm"
GCC_COMPILER="arm-rockchip830-linux-uclibcgnueabihf"
BUILD_TYPE="Release"
BUILD_DEMO_NAME="VT"

# 设置编译器
export CC=${GCC_COMPILER}-gcc
export CXX=${GCC_COMPILER}-g++

# 检查编译器是否可用
if ! command -v "${CC}" >/dev/null 2>&1; then
    echo "${CC} is not available"
    exit
fi

# 设置路径
ROOT_PWD=$( cd "$( dirname "$0" )" && pwd )
BUILD_DEMO_PATH="${ROOT_PWD}/src/cpp"

# 检查源码目录
if [[ ! -d "${BUILD_DEMO_PATH}" ]]; then
    echo "Error: cpp directory not found in ${BUILD_DEMO_PATH}"
    exit
fi

# 设置安装和构建目录
INSTALL_DIR="${ROOT_PWD}/release"
BUILD_DIR="${ROOT_PWD}/build/build_${BUILD_DEMO_NAME}_${TARGET_SOC}_${TARGET_ARCH}_${BUILD_TYPE}"

# 创建目录
mkdir -p "${BUILD_DIR}"
rm -rf "${INSTALL_DIR}" 2>/dev/null
mkdir -p "${INSTALL_DIR}"

# 执行构建
cd "${BUILD_DIR}"
cmake "${BUILD_DEMO_PATH}" \
    -DTARGET_SOC=${TARGET_SOC} \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=${TARGET_ARCH} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"

make -j4
make install

echo "Install done! Check ${INSTALL_DIR}"