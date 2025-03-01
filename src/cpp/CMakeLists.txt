cmake_minimum_required(VERSION 3.10)
project(VT)
set(CMAKE_BUILD_TYPE Debug)
# 编译优化选项
add_compile_options(-O3)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# 定义平台宏
add_definitions(-DRV1106_1103)

# 包含路径设置
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/allocator/dma)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/opencv/rv110x)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty)

# 添加子目录
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty 3rdparty.out)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../utils utils.out)

# 设置运行时库路径
set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")

# OpenCV 配置
set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# 添加可执行文件
add_executable(${PROJECT_NAME}
    main.cc
    gpio_ctrl.cpp

)

# 链接库
target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
    imageutils
    fileutils
    imagedrawing
    dl
)

# Linux 特定配置
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} Threads::Threads)
endif()

# 包含头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 安装可执行文件
install(TARGETS ${PROJECT_NAME} DESTINATION .)