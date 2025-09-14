project(JarPackager VERSION 1.0.0 LANGUAGES CXX)

# 启用Qt自动化工具
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置UI文件搜索路径
set(CMAKE_AUTOUIC_SEARCH_PATHS ui)
# 设置Qt路径（可以根据需要修改）
set(QT_INSTALL_PATH "$ENV{QT_INSTALL_PATH}")

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

# ========== Qt部署工具配置 ==========

# 查找Qt部署工具的路径
if(QT_QMAKE_EXECUTABLE)
    get_filename_component(_qt_bin_dir "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
elseif(QT_INSTALL_PATH)
    set(_qt_bin_dir "${QT_INSTALL_PATH}/bin")
else()
    set(_qt_bin_dir "")
endif()

# 查找Qt Installer Framework工具
find_program(QT_BINARYCREATOR_EXECUTABLE
        NAMES binarycreator
        PATHS ${_qt_bin_dir}
        ${QT_INSTALL_PATH}/../../Tools/QtInstallerFramework/*/bin
        NO_DEFAULT_PATH
)

find_program(QT_ARCHIVEGEN_EXECUTABLE
        NAMES archivegen
        PATHS ${_qt_bin_dir}
        ${QT_INSTALL_PATH}/../../Tools/QtInstallerFramework/*/bin
        NO_DEFAULT_PATH
)

# Windows: windeployqt
if(WIN32)
    find_program(QT_DEPLOYQT_EXECUTABLE
            NAMES windeployqt
            PATHS ${_qt_bin_dir}
            NO_DEFAULT_PATH
    )

    set(DEPLOYQT_FLAGS
            --verbose 2
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
    )

    if(QT_DEPLOYQT_EXECUTABLE)
        message(STATUS "找到windeployqt: ${QT_DEPLOYQT_EXECUTABLE}")
    else()
        message(WARNING "未找到windeployqt工具")
    endif()

    # macOS: macdeployqt
elseif(APPLE)
    find_program(QT_DEPLOYQT_EXECUTABLE
            NAMES macdeployqt
            PATHS ${_qt_bin_dir}
            NO_DEFAULT_PATH
    )

    set(DEPLOYQT_FLAGS -verbose=2)

    if(QT_DEPLOYQT_EXECUTABLE)
        message(STATUS "找到macdeployqt: ${QT_DEPLOYQT_EXECUTABLE}")
    else()
        message(WARNING "未找到macdeployqt工具")
    endif()

    # Linux: linuxdeployqt (需要单独安装)
elseif(UNIX)
    find_program(QT_DEPLOYQT_EXECUTABLE
            NAMES linuxdeployqt
            PATHS ${_qt_bin_dir} /usr/local/bin /usr/bin
    )

    set(DEPLOYQT_FLAGS -verbose=2)

    if(QT_DEPLOYQT_EXECUTABLE)
        message(STATUS "找到linuxdeployqt: ${QT_DEPLOYQT_EXECUTABLE}")
    else()
        message(WARNING "未找到linuxdeployqt工具。请从 https://github.com/probonopd/linuxdeployqt 安装")
    endif()
endif()

# ========== Qt Installer Framework 配置 ==========

# 设置安装程序相关路径
set(INSTALLER_DIR "${CMAKE_BINARY_DIR}/installer")
set(INSTALLER_CONFIG_DIR "${INSTALLER_DIR}/config")
set(INSTALLER_PACKAGES_DIR "${INSTALLER_DIR}/packages")
set(INSTALLER_PACKAGE_DIR "${INSTALLER_PACKAGES_DIR}/club.xiaojiawei.${PROJECT_NAME}")
set(INSTALLER_PACKAGE_DATA_DIR "${INSTALLER_PACKAGE_DIR}/data")
set(INSTALLER_PACKAGE_META_DIR "${INSTALLER_PACKAGE_DIR}/meta")

# 创建许可证文件（如果不存在）
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/license.txt")
    file(WRITE "${CMAKE_SOURCE_DIR}/license.txt" "MIT License

Copyright (c) 2024 ${PROJECT_NAME} Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the \"Software\"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.")
endif()

# 创建安装程序目录结构
add_custom_command(
        OUTPUT "${INSTALLER_CONFIG_DIR}/.dir_created"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALLER_CONFIG_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALLER_PACKAGE_DATA_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALLER_PACKAGE_META_DIR}"
        COMMAND ${CMAKE_COMMAND} -E touch "${INSTALLER_CONFIG_DIR}/.dir_created"
        COMMENT "创建安装程序目录结构..."
)

add_custom_command(
        OUTPUT "${INSTALLER_PACKAGE_META_DIR}/license.txt"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/license.txt" "${INSTALLER_PACKAGE_META_DIR}/license.txt"
        DEPENDS "${INSTALLER_CONFIG_DIR}/.dir_created"
        COMMENT "复制许可证文件..."
)

file(WRITE "${CMAKE_BINARY_DIR}/config.xml.in" [=[
<?xml version="1.0" encoding="UTF-8"?>
<Installer>
    <Name>JarPackager</Name>
    <Version>@PROJECT_VERSION@</Version>
    <Title>JarPackager 安装程序</Title>
    <Publisher>JarPackager Team</Publisher>
    <StartMenuDir>JarPackager</StartMenuDir>
    <TargetDir>@ApplicationsDir@/JarPackager</TargetDir>
    <AdminTargetDir>@ApplicationsDir@/JarPackager</AdminTargetDir>
    <WizardStyle>Modern</WizardStyle>
    <TitleColor>#2c3e50</TitleColor>
    <RunProgram>@TargetDir@/@PROJECT_NAME@@EXECUTABLE_SUFFIX@</RunProgram>
    <RunProgramDescription>启动 JarPackager</RunProgramDescription>
    <AllowSpaceInPath>true</AllowSpaceInPath>
    <AllowNonAsciiCharacters>true</AllowNonAsciiCharacters>
    <RemoteRepositories>
        <!-- 可以添加在线仓库 -->
    </RemoteRepositories>
</Installer>
]=])

file(WRITE "${CMAKE_BINARY_DIR}/package.xml.in" [=[
<?xml version="1.0" encoding="UTF-8"?>
<Package>
    <DisplayName>JarPackager 应用程序</DisplayName>
    <Description>一个用于将JAR文件打包到EXE文件中的工具。支持配置启动参数、JVM选项，并提供友好的图形界面。</Description>
    <Version>@PROJECT_VERSION@</Version>
    <ReleaseDate>@BUILD_DATE@</ReleaseDate>
    <Licenses>
        <License name="MIT 软件许可协议" file="license.txt"/>
    </Licenses>
    <Default>true</Default>
    <Essential>true</Essential>
    <ForcedInstallation>true</ForcedInstallation>
    <Script>installscript.qs</Script>
</Package>
]=])

file(WRITE "${CMAKE_BINARY_DIR}/installscript.qs.in" [=[
function Component()
{
    // JarPackager 安装组件构造函数
}

Component.prototype.createOperations = function()
{
    // 调用默认实现来实际安装文件
    component.createOperations();

    if (systemInfo.productType === "windows") {
        // Windows特定操作
        component.addOperation("CreateShortcut",
            "@TargetDir@/@PROJECT_NAME@.exe",
            "@StartMenuDir@/JarPackager.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/@PROJECT_NAME@.exe",
            "description=JAR文件打包工具 - 将JAR文件嵌入到EXE中");

        component.addOperation("CreateShortcut",
            "@TargetDir@/@PROJECT_NAME@.exe",
            "@DesktopDir@/JarPackager.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/@PROJECT_NAME@.exe",
            "description=JarPackager - JAR文件打包工具");

        // 注册文件关联（可选）
        component.addOperation("RegisterFileType",
            "jar",
            "@TargetDir@/@PROJECT_NAME@.exe %1",
            "JarPackager - JAR File",
            "text/plain",
            "@TargetDir@/@PROJECT_NAME@.exe,0");
    }

    if (systemInfo.productType === "osx") {
        // macOS特定操作
        component.addOperation("CreateLink",
            "@TargetDir@/JarPackager.app",
            "@ApplicationsDir@/JarPackager.app");
    }
}

Component.prototype.beginInstallation = function()
{
    // 安装开始时的操作
    installer.setValue(component.name, "Virtual", "false");
    component.beginInstallation();
}
]=])

# 设置可执行文件后缀
if(WIN32)
    set(EXECUTABLE_SUFFIX ".exe")
else()
    set(EXECUTABLE_SUFFIX "")
endif()

# 获取当前日期
string(TIMESTAMP BUILD_DATE "%Y-%m-%d")

# 配置文件生成
add_custom_command(
        OUTPUT "${INSTALLER_CONFIG_DIR}/config.xml"
        COMMAND ${CMAKE_COMMAND}
        -DPROJECT_NAME=${PROJECT_NAME}
        -DPROJECT_VERSION=${PROJECT_VERSION}
        -DEXECUTABLE_SUFFIX=${EXECUTABLE_SUFFIX}
        -DINPUT_FILE="${CMAKE_BINARY_DIR}/config.xml.in"
        -DOUTPUT_FILE="${INSTALLER_CONFIG_DIR}/config.xml"
        -P "${CMAKE_SOURCE_DIR}/configure_file.cmake"
        DEPENDS "${CMAKE_BINARY_DIR}/config.xml.in"
        COMMENT "配置${PROJECT_NAME}安装程序config.xml"
)

add_custom_command(
        OUTPUT "${INSTALLER_PACKAGE_META_DIR}/package.xml"
        COMMAND ${CMAKE_COMMAND}
        -DPROJECT_NAME=${PROJECT_NAME}
        -DPROJECT_VERSION=${PROJECT_VERSION}
        -DBUILD_DATE=${BUILD_DATE}
        -DINPUT_FILE="${CMAKE_BINARY_DIR}/package.xml.in"
        -DOUTPUT_FILE="${INSTALLER_PACKAGE_META_DIR}/package.xml"
        -P "${CMAKE_SOURCE_DIR}/configure_file.cmake"
        DEPENDS "${CMAKE_BINARY_DIR}/package.xml.in"
        COMMENT "配置${PROJECT_NAME}包package.xml"
)

add_custom_command(
        OUTPUT "${INSTALLER_PACKAGE_META_DIR}/installscript.qs"
        COMMAND ${CMAKE_COMMAND}
        -DPROJECT_NAME=${PROJECT_NAME}
        -DINPUT_FILE="${CMAKE_BINARY_DIR}/installscript.qs.in"
        -DOUTPUT_FILE="${INSTALLER_PACKAGE_META_DIR}/installscript.qs"
        -P "${CMAKE_SOURCE_DIR}/configure_file.cmake"
        DEPENDS "${INSTALLER_PACKAGE_META_DIR}/license.txt"
        COMMENT "配置${PROJECT_NAME}安装脚本"
)

# 创建configure_file.cmake脚本
file(WRITE "${CMAKE_SOURCE_DIR}/configure_file.cmake" [=[
# 简单的文件配置脚本
file(READ ${INPUT_FILE} CONTENT)
string(REPLACE "@PROJECT_NAME@" "${PROJECT_NAME}" CONTENT "${CONTENT}")
string(REPLACE "@PROJECT_VERSION@" "${PROJECT_VERSION}" CONTENT "${CONTENT}")
string(REPLACE "@BUILD_DATE@" "${BUILD_DATE}" CONTENT "${CONTENT}")
string(REPLACE "@EXECUTABLE_SUFFIX@" "${EXECUTABLE_SUFFIX}" CONTENT "${CONTENT}")
file(WRITE ${OUTPUT_FILE} "${CONTENT}")
]=])

# 部署应用程序到安装程序数据目录
add_custom_target(prepare-installer-data
        DEPENDS ${PROJECT_NAME}
        "${INSTALLER_CONFIG_DIR}/config.xml"
        "${INSTALLER_PACKAGE_META_DIR}/package.xml"
        "${INSTALLER_PACKAGE_META_DIR}/installscript.qs"
        COMMENT "准备${PROJECT_NAME}安装程序数据..."
)

if(QT_DEPLOYQT_EXECUTABLE)
    if(WIN32)
        add_custom_command(TARGET prepare-installer-data POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${INSTALLER_PACKAGE_DATA_DIR}/"
                COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "${INSTALLER_PACKAGE_DATA_DIR}/${PROJECT_NAME}${EXECUTABLE_SUFFIX}"
                COMMENT "使用windeployqt部署${PROJECT_NAME}到安装程序数据目录"
        )
    elseif(APPLE)
        add_custom_command(TARGET prepare-installer-data POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>" "${INSTALLER_PACKAGE_DATA_DIR}/${PROJECT_NAME}.app"
                COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "${INSTALLER_PACKAGE_DATA_DIR}/${PROJECT_NAME}.app"
                COMMENT "使用macdeployqt部署${PROJECT_NAME}到安装程序数据目录"
        )
    else()
        add_custom_command(TARGET prepare-installer-data POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${INSTALLER_PACKAGE_DATA_DIR}/"
                COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "${INSTALLER_PACKAGE_DATA_DIR}/${PROJECT_NAME}"
                COMMENT "使用linuxdeployqt部署${PROJECT_NAME}到安装程序数据目录"
        )
    endif()
else()
    # 如果没有deployqt工具，至少复制可执行文件
    if(APPLE)
        add_custom_command(TARGET prepare-installer-data POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>" "${INSTALLER_PACKAGE_DATA_DIR}/${PROJECT_NAME}.app"
                COMMENT "复制${PROJECT_NAME}到安装程序数据目录"
        )
    else()
        add_custom_command(TARGET prepare-installer-data POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${INSTALLER_PACKAGE_DATA_DIR}/"
                COMMENT "复制${PROJECT_NAME}到安装程序数据目录"
        )
    endif()
endif()

# 创建安装程序
if(QT_BINARYCREATOR_EXECUTABLE)
    add_custom_target(installer
            DEPENDS prepare-installer-data
            COMMAND ${QT_BINARYCREATOR_EXECUTABLE}
            --offline-only
            -c "${INSTALLER_CONFIG_DIR}/config.xml"
            -p "${INSTALLER_PACKAGES_DIR}"
            "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_installer${EXECUTABLE_SUFFIX}"
            COMMENT "创建${PROJECT_NAME}安装程序"
    )

    # 在线安装程序（可选）
    add_custom_target(online-installer
            DEPENDS prepare-installer-data
            COMMAND ${QT_BINARYCREATOR_EXECUTABLE}
            --online-only
            -c "${INSTALLER_CONFIG_DIR}/config.xml"
            -p "${INSTALLER_PACKAGES_DIR}"
            "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_online_installer${EXECUTABLE_SUFFIX}"
            COMMENT "创建${PROJECT_NAME}在线安装程序"
    )

    message(STATUS "找到Qt Installer Framework: ${QT_BINARYCREATOR_EXECUTABLE}")
    message(STATUS "${PROJECT_NAME} 安装程序构建目标:")
    message(STATUS "  cmake --build . --target installer        # 离线安装程序")
    message(STATUS "  cmake --build . --target online-installer # 在线安装程序")
else()
    message(WARNING "未找到Qt Installer Framework工具 (binarycreator)")
    message(STATUS "请安装Qt Installer Framework或设置正确的路径")
endif()

# 简单的部署目标（不使用QtIFW）
if(QT_DEPLOYQT_EXECUTABLE)
    add_custom_target(deploy
            DEPENDS ${PROJECT_NAME}
            COMMENT "部署${PROJECT_NAME}应用程序..."
    )

    if(WIN32)
        add_custom_command(TARGET deploy POST_BUILD
                COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "$<TARGET_FILE:${PROJECT_NAME}>"
        )
    elseif(APPLE)
        add_custom_command(TARGET deploy POST_BUILD
                COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
        )
    else()
        add_custom_command(TARGET deploy POST_BUILD
                COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "$<TARGET_FILE:${PROJECT_NAME}>"
        )
    endif()

    message(STATUS "使用 'cmake --build . --target deploy' 来部署${PROJECT_NAME}")
endif()

# 清理目标
add_custom_target(clean-installer
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${INSTALLER_DIR}"
        COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_installer${EXECUTABLE_SUFFIX}"
        COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_online_installer${EXECUTABLE_SUFFIX}"
        COMMENT "清理${PROJECT_NAME}安装程序文件..."
)

# ========== 安装配置 ==========
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION ..
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Qt6终结化
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()

# 打印项目信息
message(STATUS "========== ${PROJECT_NAME} 项目配置 ==========")
message(STATUS "项目名称: ${PROJECT_NAME}")
message(STATUS "项目版本: ${PROJECT_VERSION}")
message(STATUS "Qt版本: Qt${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}")
message(STATUS "C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "QT_INSTALL_PATH: ${QT_INSTALL_PATH}")
message(STATUS "==========================================")