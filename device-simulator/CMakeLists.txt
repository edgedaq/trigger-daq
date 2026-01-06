# CMakeLists.txt for Device Simulator
cmake_minimum_required(VERSION 3.10)
project(device_simulator C)

# 设置C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 源文件
set(SOURCES
    main.c
    app.c
    transport_test.c
    transport_tcp_client.c
    protocol/protocol.c
    protocol/io_buffer.c
)

# 创建可执行文件
add_executable(device_simulator ${SOURCES})

# 链接库
if(WIN32)
    target_link_libraries(device_simulator ws2_32)
endif()

# 数学库
target_link_libraries(device_simulator m)

# 安装目标
install(TARGETS device_simulator DESTINATION bin)

# 调试配置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
    if(MSVC)
        add_compile_options(/Zi)
    else()
        add_compile_options(-g)
    endif()
endif()

# 输出信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")