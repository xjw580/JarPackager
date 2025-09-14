cmake_minimum_required(VERSION 3.30.5)
project(JarPackagerParent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#设置upx路径
set(UPX_EXECUTABLE ${PROJECT_SOURCE_DIR}/upx.exe)
set(OUTPUT_DIR ${PROJECT_SOURCE_DIR}/out)

#设置cpp标准库模块路径
get_filename_component(VCToolsInstallDir "$ENV{VCToolsInstallDir}" ABSOLUTE)
set(STD_IXX_PATH "${VCToolsInstallDir}/modules/std.ixx")

#构建cpp标准库依赖
add_library(std_lib)
get_filename_component(STD_IXX_DIR "${STD_IXX_PATH}" DIRECTORY)
target_sources(std_lib
        PUBLIC
        FILE_SET std_modules TYPE CXX_MODULES
        BASE_DIRS "${STD_IXX_DIR}"
        FILES "${STD_IXX_PATH}"
)

include_directories("includes")
link_directories("libs")

#设置脚本文件
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(build)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building in Debug mode!!!")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Building in Release mode")
endif ()

#包含子项目
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "64-bit environment detected")
    add_subdirectory(common)
    add_subdirectory(packager)
    if(MSVC)
        add_subdirectory(launcher)
    else()
        message(WARNING "非 MSVC 编译器，跳过launcher")
    endif()
else ()
    message(STATUS "32-bit environment detected")
endif ()