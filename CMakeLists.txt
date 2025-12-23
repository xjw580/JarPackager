cmake_minimum_required(VERSION 3.30.5)
project(JarPackager VERSION 1.3.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC 编译器需要此选项以正确报告 C++ 标准版本
if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

#设置upx路径
set(UPX_EXECUTABLE ${PROJECT_SOURCE_DIR}/upx.exe)
set(OUTPUT_DIR ${PROJECT_SOURCE_DIR}/out)

#设置qt目录
if(DEFINED ENV{QT_INSTALL_PATH})
    set(CMAKE_PREFIX_PATH "$ENV{QT_INSTALL_PATH}")
    set(Qt6_DIR "$ENV{QT_INSTALL_PATH}")
    set(QT_INSTALL_PATH "${Qt6_DIR}")
    message(STATUS "Using Qt from: $ENV{QT_INSTALL_PATH}")
else()
    message(FATAL_ERROR "请设置 QT_INSTALL_PATH 环境变量，例如: S:/Qt/6.7.3/msvc2022_64")
endif()

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
include(jni)
include(deploy)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building in Debug mode!!!")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Building in Release mode")
endif ()

#包含子项目
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "64-bit environment detected")
    add_subdirectory(common)
    add_subdirectory(packager) #包装器
    add_subdirectory(launcher) #启动器
    add_subdirectory(attacher) #附加器

    add_custom_target(execute_attacher_new
            COMMAND echo "执行附加动作"
            COMMAND $<TARGET_FILE:attacher> $<TARGET_FILE:packager> $<TARGET_FILE:launcher> "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.exe"
            DEPENDS packager launcher attacher
            COMMENT "将启动器附加到包装器中"
            VERBATIM
    )

    add_custom_target(execute_attacher
            COMMAND echo "执行附加动作"
            COMMAND $<TARGET_FILE:attacher> $<TARGET_FILE:packager> $<TARGET_FILE:launcher>
            DEPENDS packager launcher attacher
            COMMENT "将启动器附加到包装器中"
            VERBATIM
    )
    #    add_dependencies(prepare-installer-data execute_attacher)
    # 设置部署，依赖于自定义目标
    setup_qt_deployment(
            TARGET_NAME packager
            PACKAGE_NAME ${PROJECT_NAME}
            INSTALLED_EXE_NAME ${PROJECT_NAME}
            VERSION ${PROJECT_VERSION}
            EXCLUDE_MODULES network pdf svg
            LICENSE_FILE "${CMAKE_SOURCE_DIR}/license.txt"
            PACKAGE_DESCRIPTION "包含资源文件的Qt应用程序"
            DEPENDS execute_attacher  # 添加依赖
    )
else ()
    message(STATUS "32-bit environment detected")
endif ()