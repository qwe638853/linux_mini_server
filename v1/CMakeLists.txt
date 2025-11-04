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

# 查找 libcurl（多個目標共用）
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

# 共享庫：libsysinfo
add_library(sysinfo SHARED src/sysinfo.c)
set_target_properties(sysinfo PROPERTIES
    OUTPUT_NAME "sysinfo"
    POSITION_INDEPENDENT_CODE ON
)

# Server 可執行文件（需要鏈接 sysinfo 庫、smtp.c、env.c 和 libcurl）
add_executable(server src/server.c src/smtp.c src/env.c)
target_link_libraries(server sysinfo ${CURL_LIBRARIES})
target_include_directories(server PRIVATE ${CURL_INCLUDE_DIRS})

# 設定 server 的 rpath，讓它可以找到動態庫
set_target_properties(server PROPERTIES
    INSTALL_RPATH "${CMAKE_BINARY_DIR}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Client 可執行文件（不需要額外的庫）
add_executable(client src/client.c)

# 可選：安裝規則
install(TARGETS server client
    RUNTIME DESTINATION bin
)

install(TARGETS sysinfo
    LIBRARY DESTINATION lib
)

