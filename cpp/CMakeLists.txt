cmake_minimum_required(VERSION 3.10)
project(TDengineHealpixAnalysis)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找必需的包
find_package(PkgConfig REQUIRED)

# 查找 HealPix C++
pkg_check_modules(HEALPIX_CXX REQUIRED healpix_cxx)
if(NOT HEALPIX_CXX_FOUND)
    message(FATAL_ERROR "HealPix C++ library not found. Please install libhealpix-cxx-dev")
endif()

# 查找 CFITSIO
pkg_check_modules(CFITSIO REQUIRED cfitsio)
if(NOT CFITSIO_FOUND)
    message(FATAL_ERROR "CFITSIO library not found. Please install libcfitsio-dev")
endif()

# 查找 TDengine
find_library(TAOS_LIB taos PATHS /usr/lib /usr/local/lib)
if(NOT TAOS_LIB)
    message(FATAL_ERROR "TDengine library not found. Please install TDengine client")
endif()

# 设置包含目录
include_directories(${HEALPIX_CXX_INCLUDE_DIRS})
include_directories(${CFITSIO_INCLUDE_DIRS})
include_directories(/usr/include/taos)

# 设置链接目录
link_directories(${HEALPIX_CXX_LIBRARY_DIRS})
link_directories(${CFITSIO_LIBRARY_DIRS})

# 编译选项
add_compile_options(${HEALPIX_CXX_CFLAGS_OTHER})
add_compile_options(${CFITSIO_CFLAGS_OTHER})

# 可执行文件：数据生成器
add_executable(generate_astronomical_data generate_astronomical_data.cpp)
target_link_libraries(generate_astronomical_data stdc++fs)

# 可执行文件：查询测试器
add_executable(query_test query_test.cpp)
target_link_libraries(query_test 
    ${TAOS_LIB}
    ${HEALPIX_CXX_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    stdc++fs
)

# 可执行文件：数据导入器
add_executable(quick_import quick_import.cpp)
target_link_libraries(quick_import 
    ${TAOS_LIB}
    ${HEALPIX_CXX_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    stdc++fs
)

# 设置编译标志
set_target_properties(generate_astronomical_data query_test quick_import
    PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 输出编译信息
message(STATUS "HealPix C++ include dirs: ${HEALPIX_CXX_INCLUDE_DIRS}")
message(STATUS "HealPix C++ libraries: ${HEALPIX_CXX_LIBRARIES}")
message(STATUS "CFITSIO include dirs: ${CFITSIO_INCLUDE_DIRS}")
message(STATUS "CFITSIO libraries: ${CFITSIO_LIBRARIES}")
message(STATUS "TDengine library: ${TAOS_LIB}")

# 安装目标
install(TARGETS generate_astronomical_data query_test quick_import
    DESTINATION bin
)