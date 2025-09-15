#压缩二进制产物
macro(compress name)
    if (UPX_EXECUTABLE AND EXISTS "${UPX_EXECUTABLE}")
        add_custom_command(TARGET ${name} POST_BUILD
                COMMAND ${UPX_EXECUTABLE} --best --lzma "$<TARGET_FILE:${name}>"
                COMMENT "Compressing ${name} with UPX..."
                VERBATIM
        )
    else ()
        message(WARNING "UPX_EXECUTABLE not found or invalid, skipping compression for ${name}")
    endif ()
endmacro()

#构建命令
#name：输出名，type：构建类型(SHARED,STATIC,EXECUTABLE,EXECUTABLE_WIN32,MODULE)，compress：是否压缩
macro(my_add_target name type compress)
    if(NOT compress)
        set(compress false)
    endif()
    message(STATUS "name=${name}, type=${type}, compress=${compress}")

    # 设置默认输出目录（如果未定义）
    if (NOT DEFINED OUTPUT_DIR)
        set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/out")
    endif ()

    # 确保输出目录存在
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")

    # 收集普通源文件（不包括模块接口文件）
    file(GLOB_RECURSE srcs CONFIGURE_DEPENDS
            src/*.cpp
            src/*.h
            src/*.c
            include/*.h
            *.rc
            *.manifest
    )

    # 构建产物类型
    if ("${type}" MATCHES "MODULE")
        add_library(${name} ${srcs})
        target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    elseif ("${type}" MATCHES "EXECUTABLE_WIN32")
        add_executable(${name} WIN32 ${srcs})
        target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    elseif ("${type}" MATCHES "EXECUTABLE")
        add_executable(${name} ${srcs})
        target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    elseif ("${type}" MATCHES "SHARED" OR "${type}" MATCHES "STATIC")
        add_library(${name} ${type} ${srcs})
        target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    else ()
        message(FATAL_ERROR "Unknown target type: ${type}. Use EXECUTABLE, EXECUTABLE_WIN32, SHARED or STATIC")
    endif ()

    # 定义自定义模块文件集
    file(GLOB_RECURSE MODULE_FILES CONFIGURE_DEPENDS src/*.cppm)
    target_sources(${name}
            PUBLIC
            FILE_SET modules TYPE CXX_MODULES FILES
            ${MODULE_FILES}
    )

    # 链接标准库模块
    target_link_libraries(${name} PRIVATE std_lib)

    # 在构建后复制文件
    add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:${name}>"
            "${OUTPUT_DIR}/$<TARGET_FILE_NAME:${name}>"
            COMMENT "Copying $<TARGET_FILE_NAME:${name}> to ${OUTPUT_DIR}"
            VERBATIM
    )

    # 压缩构建产物，只在发布模式下压缩
    if (CMAKE_BUILD_TYPE STREQUAL "Release" AND ${compress})
        compress(${name})
    endif ()
endmacro()