project(packager VERSION 1.0.0 LANGUAGES CXX)

# 启用Qt自动化工具
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置UI文件搜索路径
set(CMAKE_AUTOUIC_SEARCH_PATHS ui)
# 设置Qt路径（可以根据需要修改）

# 查找Qt组件 - 支持Qt5/Qt6兼容性
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

# 收集源文件 - 适配我们的目录结构
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
        src/*.cpp
        include/*.h
        ui/*.ui
        app.rc
)

# 创建可执行文件 - 使用现代Qt方式，qt_add_executable和模块库冲突
#if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
#    qt_add_executable(${PROJECT_NAME}
#            MANUAL_FINALIZATION
#            ${PROJECT_SOURCES}
#    )
#else()
#    add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
#endif()

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# 链接Qt库
target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        std_lib
        common
)

target_include_directories(${PROJECT_NAME} PRIVATE
        "include"
        ${CMAKE_CURRENT_BINARY_DIR}  # 用于生成的ui_*.h文件
)

# 设置目标属性
set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
        VERSION ${PROJECT_VERSION}
)

if(WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if(MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif()

    if(NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if(NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif()
    endif()

    # 复制平台插件
    if(EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif()

    # 复制样式插件
    if(EXISTS "${QT_INSTALL_PATH}/plugins/styles")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/styles/")
        file(GLOB STYLE_DLLS "${QT_INSTALL_PATH}/plugins/styles/*${DEBUG_SUFFIX}.dll")
        foreach(STYLE_DLL ${STYLE_DLLS})
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy
                    "${STYLE_DLL}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/styles/")
        endforeach()
    endif()

    # 复制图片插件
    if(EXISTS "${QT_INSTALL_PATH}/plugins/imageformats")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/imageformats/")
        file(GLOB IMAGE_DLLS "${QT_INSTALL_PATH}/plugins/imageformats/*${DEBUG_SUFFIX}.dll")
        foreach(IMAGE_DLL ${IMAGE_DLLS})
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy
                    "${IMAGE_DLL}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/imageformats/")
        endforeach()
    endif()

    # 复制Qt核心库
    foreach(QT_LIB Core Gui Widgets)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_INSTALL_PATH}/bin/Qt${QT_VERSION_MAJOR}${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach()
endif()