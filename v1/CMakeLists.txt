cmake_minimum_required(VERSION 3.10)
project(ServerClient VERSION 1.0 LANGUAGES C)

# 設定 C 編譯器選項
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 設定編譯選項（對應 Makefile 中的 CFLAGS）
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")

# 設定輸出目錄
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 設定包含目錄
include_directories(include)

# Debug log control option (compile-time)
option(BUILD_DEBUG "Enable debug log support at compile time" OFF)

# 查找 libcurl（多個目標共用）
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

# Utility shared library: 包含 client 和 server 共用的功能
add_library(utility SHARED src/debug.c)
set_target_properties(utility PROPERTIES
    OUTPUT_NAME "utility"
    POSITION_INDEPENDENT_CODE ON
)

# 根據 BUILD_DEBUG 選項設定 utility 庫的編譯定義
if(BUILD_DEBUG)
    target_compile_definitions(utility PRIVATE DEBUG)
    message(STATUS "Debug log support: ENABLED (compile-time)")
else()
    message(STATUS "Debug log support: DISABLED (compile-time)")
endif()

# Server 可執行文件（需要鏈接 utility 庫、sysinfo.c、smtp.c、env.c 和 libcurl）
add_executable(server src/server.c src/sysinfo.c src/smtp.c src/env.c)
target_link_libraries(server utility ${CURL_LIBRARIES})
target_include_directories(server PRIVATE ${CURL_INCLUDE_DIRS})

# 根據 BUILD_DEBUG 選項設定編譯定義
if(BUILD_DEBUG)
    target_compile_definitions(server PRIVATE DEBUG)
endif()

# 設定 server 的 rpath，讓它可以找到動態庫
set_target_properties(server PROPERTIES
    INSTALL_RPATH "${CMAKE_BINARY_DIR}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Client 可執行文件（需要連結 utility 庫）
add_executable(client src/client.c)
target_link_libraries(client utility)

# 根據 BUILD_DEBUG 選項設定編譯定義
if(BUILD_DEBUG)
    target_compile_definitions(client PRIVATE DEBUG)
endif()

# 設定 client 的 rpath，讓它可以找到動態庫
set_target_properties(client PROPERTIES
    INSTALL_RPATH "${CMAKE_BINARY_DIR}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# 可選：安裝規則
install(TARGETS server client
    RUNTIME DESTINATION bin
)

install(TARGETS utility
    LIBRARY DESTINATION lib
)

