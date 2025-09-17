# QtDeploymentTools.cmake
# Qt应用程序部署和安装程序生成工具模块
#
# 使用方法:
#   include(QtDeploymentTools)
#   setup_qt_deployment(
#       TARGET_NAME your_target_name
#       PACKAGE_NAME "Your Package Name"
#       PACKAGE_DESCRIPTION "Your package description"
#       VERSION "1.0.0"
#       LICENSE_FILE "${CMAKE_SOURCE_DIR}/LICENSE.txt"
#       DEPENDS other_custom_target1 other_custom_target2
#       MINIMAL_DEPLOYMENT ON  # 启用最小化部署
#       EXCLUDE_MODULES network sql multimedia  # 排除特定模块
#       INSTALLED_EXE_NAME "CustomName"  # 安装后的exe名称
#   )
#
# 可选参数:
#   OUTPUT_DIR - 输出目录 (默认: ${CMAKE_BINARY_DIR})
#   DEPLOY_FLAGS - 额外的部署标志
#   INSTALLER_TITLE - 安装程序标题
#   PUBLISHER - 发布者名称
#   CREATE_SHORTCUTS - 是否创建快捷方式 (默认: ON)
#   REGISTER_FILE_TYPES - 要注册的文件类型列表
#   DEPENDS - 依赖的其他目标列表
#   MINIMAL_DEPLOYMENT - 启用最小化部署模式 (默认: OFF)
#   EXCLUDE_MODULES - 要排除的Qt模块列表
#   INCLUDE_MODULES - 强制包含的Qt模块列表
#   INSTALLED_EXE_NAME - 安装后的exe名称（默认使用PACKAGE_NAME）

include_guard(GLOBAL)

# ========== 查找Qt部署工具 ==========
function(find_qt_deployment_tools)
    # 查找Qt部署工具的路径
    if(QT_QMAKE_EXECUTABLE)
        get_filename_component(_qt_bin_dir "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    elseif(Qt6_DIR)
        get_filename_component(_qt_bin_dir "${Qt6_DIR}/bin" ABSOLUTE)
    elseif(Qt5_DIR)
        get_filename_component(_qt_bin_dir "${Qt5_DIR}/bin" ABSOLUTE)
    else()
        set(_qt_bin_dir "")
    endif()

    # Windows: windeployqt
    if(WIN32)
        find_program(QT_DEPLOYQT_EXECUTABLE
                NAMES windeployqt windeployqt.exe
                PATHS ${_qt_bin_dir}
                NO_DEFAULT_PATH
        )

        if(QT_DEPLOYQT_EXECUTABLE)
            set(QT_DEPLOYQT_EXECUTABLE "${QT_DEPLOYQT_EXECUTABLE}" PARENT_SCOPE)
            set(DEPLOYQT_DEFAULT_FLAGS
                    --verbose 2
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --no-compiler-runtime
                    --no-quick-import
                    --no-virtualkeyboard
                    PARENT_SCOPE
            )
            message(STATUS "找到windeployqt: ${QT_DEPLOYQT_EXECUTABLE}")
        endif()

        # macOS: macdeployqt
    elseif(APPLE)
        find_program(QT_DEPLOYQT_EXECUTABLE
                NAMES macdeployqt
                PATHS ${_qt_bin_dir}
                NO_DEFAULT_PATH
        )

        if(QT_DEPLOYQT_EXECUTABLE)
            set(QT_DEPLOYQT_EXECUTABLE "${QT_DEPLOYQT_EXECUTABLE}" PARENT_SCOPE)
            set(DEPLOYQT_DEFAULT_FLAGS -verbose=2 PARENT_SCOPE)
            message(STATUS "找到macdeployqt: ${QT_DEPLOYQT_EXECUTABLE}")
        endif()

        # Linux: linuxdeployqt
    elseif(UNIX)
        find_program(QT_DEPLOYQT_EXECUTABLE
                NAMES linuxdeployqt linuxdeployqt-continuous-x86_64.AppImage
                PATHS ${_qt_bin_dir} /usr/local/bin /usr/bin ${CMAKE_CURRENT_BINARY_DIR}
        )

        if(QT_DEPLOYQT_EXECUTABLE)
            set(QT_DEPLOYQT_EXECUTABLE "${QT_DEPLOYQT_EXECUTABLE}" PARENT_SCOPE)
            set(DEPLOYQT_DEFAULT_FLAGS -verbose=2 PARENT_SCOPE)
            message(STATUS "找到linuxdeployqt: ${QT_DEPLOYQT_EXECUTABLE}")
        else()
            message(WARNING "未找到linuxdeployqt工具。请从 https://github.com/probonopd/linuxdeployqt 安装")
        endif()
    endif()

    # 查找Qt Installer Framework工具
    find_program(QT_BINARYCREATOR_EXECUTABLE
            NAMES binarycreator binarycreator.exe
            PATHS
            ${_qt_bin_dir}
            ${_qt_bin_dir}/../../../Tools/QtInstallerFramework/*/bin
            NO_DEFAULT_PATH
    )

    if(QT_BINARYCREATOR_EXECUTABLE)
        set(QT_BINARYCREATOR_EXECUTABLE "${QT_BINARYCREATOR_EXECUTABLE}" PARENT_SCOPE)
        message(STATUS "找到Qt Installer Framework: ${QT_BINARYCREATOR_EXECUTABLE}")
    endif()
endfunction()

function(update_file_if_newer)
    cmake_parse_arguments(
            ARG
            "ALWAYS_CREATE"
            "SOURCE;DESTINATION"
            ""
            ${ARGN}
    )

    if(ARG_SOURCE AND EXISTS "${ARG_SOURCE}")
        set(NEED_UPDATE FALSE)

        if(NOT EXISTS "${ARG_DESTINATION}")
            set(NEED_UPDATE TRUE)
        else()
            # 比较修改时间
            file(TIMESTAMP "${ARG_SOURCE}" SOURCE_TIME "%Y%m%d%H%M%S")
            file(TIMESTAMP "${ARG_DESTINATION}" DEST_TIME "%Y%m%d%H%M%S")

            if(SOURCE_TIME GREATER DEST_TIME)
                set(NEED_UPDATE TRUE)
                message(STATUS "源文件较新，需要更新")
            endif()
        endif()

        if(NEED_UPDATE)
            get_filename_component(DEST_DIR "${ARG_DESTINATION}" DIRECTORY)
            file(MAKE_DIRECTORY "${DEST_DIR}")
            configure_file("${ARG_SOURCE}" "${ARG_DESTINATION}" COPYONLY)
            message(STATUS "已更新（基于时间）: ${ARG_DESTINATION}")
        endif()

    elseif(ARG_ALWAYS_CREATE AND NOT EXISTS "${ARG_DESTINATION}")
        get_filename_component(DEST_DIR "${ARG_DESTINATION}" DIRECTORY)
        file(MAKE_DIRECTORY "${DEST_DIR}")
        file(WRITE "${ARG_DESTINATION}" "")
        message(STATUS "创建新文件: ${ARG_DESTINATION}")
    endif()
endfunction()

# ========== 创建安装程序配置文件 ==========
function(create_installer_configs)
    cmake_parse_arguments(
            ARG
            ""
            "TARGET_NAME;PACKAGE_NAME;PACKAGE_DESCRIPTION;VERSION;LICENSE_FILE;INSTALLER_DIR;PUBLISHER;INSTALLER_TITLE;INSTALLED_EXE_NAME"
            "REGISTER_FILE_TYPES"
            ${ARGN}
    )

    # 默认值设置
    if(NOT ARG_PUBLISHER)
        set(ARG_PUBLISHER "Your Company")
    endif()

    if(NOT ARG_INSTALLER_TITLE)
        set(ARG_INSTALLER_TITLE "${ARG_PACKAGE_NAME} 安装程序")
    endif()

    # 设置安装后的exe名称
    if(NOT ARG_INSTALLED_EXE_NAME)
        # 将PACKAGE_NAME转换为合法的文件名（移除特殊字符）
        string(REPLACE " " "_" SAFE_EXE_NAME "${ARG_PACKAGE_NAME}")
        string(REPLACE "." "_" SAFE_EXE_NAME "${SAFE_EXE_NAME}")
        string(REPLACE "/" "_" SAFE_EXE_NAME "${SAFE_EXE_NAME}")
        string(REPLACE "\\" "_" SAFE_EXE_NAME "${SAFE_EXE_NAME}")
        set(ARG_INSTALLED_EXE_NAME "${SAFE_EXE_NAME}")
    endif()

    # 设置文件名
    if(WIN32)
        set(ORIGINAL_EXE "${ARG_TARGET_NAME}.exe")
        set(INSTALLED_EXE "${ARG_INSTALLED_EXE_NAME}.exe")
    elseif(APPLE)
        set(ORIGINAL_EXE "${ARG_TARGET_NAME}.app")
        set(INSTALLED_EXE "${ARG_INSTALLED_EXE_NAME}.app")
    else()
        set(ORIGINAL_EXE "${ARG_TARGET_NAME}")
        set(INSTALLED_EXE "${ARG_INSTALLED_EXE_NAME}")
    endif()

    # 创建config.xml
    file(WRITE "${ARG_INSTALLER_DIR}/config/config.xml" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<Installer>
    <Name>${ARG_PACKAGE_NAME}</Name>
    <Version>${ARG_VERSION}</Version>
    <Title>${ARG_INSTALLER_TITLE}</Title>
    <Publisher>${ARG_PUBLISHER}</Publisher>
    <StartMenuDir>${ARG_PACKAGE_NAME}</StartMenuDir>
    <TargetDir>@ApplicationsDir@/${ARG_PACKAGE_NAME}</TargetDir>
    <AdminTargetDir>@ApplicationsDir@/${ARG_PACKAGE_NAME}</AdminTargetDir>
    <WizardStyle>Modern</WizardStyle>
    <TitleColor>#2c3e50</TitleColor>
    <RunProgram>@TargetDir@/${INSTALLED_EXE}</RunProgram>
    <RunProgramDescription>启动 ${ARG_PACKAGE_NAME}</RunProgramDescription>
    <AllowSpaceInPath>true</AllowSpaceInPath>
    <AllowNonAsciiCharacters>true</AllowNonAsciiCharacters>
</Installer>
")

    # 创建package.xml
    string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
    file(WRITE "${ARG_INSTALLER_DIR}/packages/${ARG_TARGET_NAME}/meta/package.xml" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<Package>
    <DisplayName>${ARG_PACKAGE_NAME}</DisplayName>
    <Description>${ARG_PACKAGE_DESCRIPTION}</Description>
    <Version>${ARG_VERSION}</Version>
    <ReleaseDate>${BUILD_DATE}</ReleaseDate>
    <Licenses>
        <License name=\"软件许可协议\" file=\"license.txt\"/>
    </Licenses>
    <Default>true</Default>
    <Essential>true</Essential>
    <ForcedInstallation>true</ForcedInstallation>
    <Script>installscript.qs</Script>
</Package>
")

    # 生成文件关联注册代码
    set(FILE_ASSOCIATION_CODE "")
    foreach(file_type ${ARG_REGISTER_FILE_TYPES})
        string(APPEND FILE_ASSOCIATION_CODE "
        component.addOperation(\"RegisterFileType\",
            \"${file_type}\",
            \"@TargetDir@/${INSTALLED_EXE} %1\",
            \"${ARG_PACKAGE_NAME} - ${file_type} File\",
            \"application/${file_type}\",
            \"@TargetDir@/${INSTALLED_EXE},0\"
        );")
    endforeach()

    # 创建installscript.qs - 包含重命名操作
    file(WRITE "${ARG_INSTALLER_DIR}/packages/${ARG_TARGET_NAME}/meta/installscript.qs" "function Component()
{
    // 安装组件构造函数
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    // 如果exe名称不同，重命名文件
    if (\"${ORIGINAL_EXE}\" != \"${INSTALLED_EXE}\") {
        // 重命名exe文件
        if (systemInfo.productType === \"windows\") {
            // Windows下重命名
            component.addOperation(\"Move\",
                \"@TargetDir@/${ORIGINAL_EXE}\",
                \"@TargetDir@/${INSTALLED_EXE}\"
            );
        }
    }

    if (systemInfo.productType === \"windows\") {
        // 创建开始菜单快捷方式
        component.addOperation(\"CreateShortcut\",
            \"@TargetDir@/${INSTALLED_EXE}\",
            \"@StartMenuDir@/${ARG_PACKAGE_NAME}.lnk\",
            \"workingDirectory=@TargetDir@\",
            \"iconPath=@TargetDir@/${INSTALLED_EXE}\",
            \"description=${ARG_PACKAGE_DESCRIPTION}\");

        // 创建桌面快捷方式
        component.addOperation(\"CreateShortcut\",
            \"@TargetDir@/${INSTALLED_EXE}\",
            \"@DesktopDir@/${ARG_PACKAGE_NAME}.lnk\",
            \"workingDirectory=@TargetDir@\",
            \"iconPath=@TargetDir@/${INSTALLED_EXE}\",
            \"description=${ARG_PACKAGE_DESCRIPTION}\");
        ${FILE_ASSOCIATION_CODE}
    }

    if (systemInfo.productType === \"osx\") {
        component.addOperation(\"CreateLink\",
            \"@TargetDir@/${INSTALLED_EXE}\",
            \"@ApplicationsDir@/${INSTALLED_EXE}\");
    }
}

Component.prototype.beginInstallation = function()
{
    installer.setValue(component.name, \"Virtual\", \"false\");
    component.beginInstallation();
}
")


    # 复制许可证文件
    if(ARG_LICENSE_FILE AND EXISTS "${ARG_LICENSE_FILE}")
        update_file_if_newer(
                SOURCE ${ARG_LICENSE_FILE}
                DESTINATION "${ARG_INSTALLER_DIR}/packages/${ARG_TARGET_NAME}/meta/license.txt"
        )
    else()
        # 创建空的许可证文件
        file(WRITE "${ARG_INSTALLER_DIR}/packages/${ARG_TARGET_NAME}/meta/license.txt" "")
        message(WARNING "使用空白许可证")
    endif()

    # 将信息传递给父作用域
    set(INSTALLED_EXE_NAME "${ARG_INSTALLED_EXE_NAME}" PARENT_SCOPE)
    set(ORIGINAL_EXE_NAME "${ARG_TARGET_NAME}" PARENT_SCOPE)
endfunction()

# ========== 主函数：设置Qt部署 ==========
function(setup_qt_deployment)
    cmake_parse_arguments(
            ARG
            "CREATE_SHORTCUTS;MINIMAL_DEPLOYMENT"
            "TARGET_NAME;PACKAGE_NAME;PACKAGE_DESCRIPTION;VERSION;LICENSE_FILE;OUTPUT_DIR;INSTALLER_TITLE;PUBLISHER;INSTALLED_EXE_NAME"
            "DEPLOY_FLAGS;REGISTER_FILE_TYPES;DEPENDS;EXCLUDE_MODULES;INCLUDE_MODULES"
            ${ARGN}
    )

    # 参数验证
    if(NOT ARG_TARGET_NAME)
        message(FATAL_ERROR "setup_qt_deployment: TARGET_NAME 是必需的")
    endif()

    # 默认值设置
    if(NOT ARG_PACKAGE_NAME)
        set(ARG_PACKAGE_NAME "${ARG_TARGET_NAME}")
    endif()

    if(NOT ARG_VERSION)
        if(PROJECT_VERSION)
            set(ARG_VERSION "${PROJECT_VERSION}")
        else()
            set(ARG_VERSION "1.0.0")
        endif()
    endif()

    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CMAKE_BINARY_DIR}")
    endif()

    if(NOT ARG_PACKAGE_DESCRIPTION)
        set(ARG_PACKAGE_DESCRIPTION "${ARG_PACKAGE_NAME} Application")
    endif()

    if(NOT DEFINED ARG_CREATE_SHORTCUTS)
        set(ARG_CREATE_SHORTCUTS ON)
    endif()

    # 查找Qt部署工具
    find_qt_deployment_tools()

    # 设置安装程序相关路径
    set(INSTALLER_DIR "${ARG_OUTPUT_DIR}/installer")
    set(INSTALLER_CONFIG_DIR "${INSTALLER_DIR}/config")
    set(INSTALLER_PACKAGES_DIR "${INSTALLER_DIR}/packages")
    set(INSTALLER_PACKAGE_DIR "${INSTALLER_PACKAGES_DIR}/${ARG_TARGET_NAME}")
    set(INSTALLER_PACKAGE_DATA_DIR "${INSTALLER_PACKAGE_DIR}/data")
    set(INSTALLER_PACKAGE_META_DIR "${INSTALLER_PACKAGE_DIR}/meta")

    # 创建目录结构
    file(MAKE_DIRECTORY "${INSTALLER_CONFIG_DIR}")
    file(MAKE_DIRECTORY "${INSTALLER_PACKAGE_DATA_DIR}")
    file(MAKE_DIRECTORY "${INSTALLER_PACKAGE_META_DIR}")

    # 创建配置文件
    create_installer_configs(
            TARGET_NAME "${ARG_TARGET_NAME}"
            PACKAGE_NAME "${ARG_PACKAGE_NAME}"
            PACKAGE_DESCRIPTION "${ARG_PACKAGE_DESCRIPTION}"
            VERSION "${ARG_VERSION}"
            LICENSE_FILE "${ARG_LICENSE_FILE}"
            INSTALLER_DIR "${INSTALLER_DIR}"
            PUBLISHER "${ARG_PUBLISHER}"
            INSTALLER_TITLE "${ARG_INSTALLER_TITLE}"
            INSTALLED_EXE_NAME "${ARG_INSTALLED_EXE_NAME}"
            REGISTER_FILE_TYPES ${ARG_REGISTER_FILE_TYPES}
    )

    # 合并部署标志
    set(DEPLOYQT_FLAGS ${DEPLOYQT_DEFAULT_FLAGS})

    # 处理最小化部署选项
    if(ARG_MINIMAL_DEPLOYMENT)
        if(WIN32)
            list(APPEND DEPLOYQT_FLAGS
                    --no-network
                    --no-sql
                    --no-multimedia
                    --no-multimediawidgets
                    --no-multimediaquick
                    --no-3dcore
                    --no-3drender
                    --no-3dinput
                    --no-3dlogic
                    --no-3dextras
                    --no-3danimation
                    --no-charts
                    --no-datavisualization
                    --no-serialport
                    --no-serialbus
                    --no-nfc
                    --no-websockets
                    --no-webenginecore
                    --no-webengine
                    --no-webview
                    --no-remoteobjects
                    --no-sensors
                    --no-bluetooth
                    --no-gamepad
                    --no-location
                    --no-waylandcompositor
                    --no-concurrent
                    --no-xml
                    --no-printsupport
                    --no-help
                    --no-pdf
                    --no-quickcontrols
                    --no-quickcontrols2
                    --no-quicktemplates2
                    --no-quickwidgets
                    --no-scxml
                    --no-test
            )
        endif()
    endif()

    # 处理排除的模块
    foreach(module ${ARG_EXCLUDE_MODULES})
        if(WIN32)
            list(APPEND DEPLOYQT_FLAGS "--no-${module}")
        endif()
    endforeach()

    # 处理包含的模块（仅限Windows）
    foreach(module ${ARG_INCLUDE_MODULES})
        if(WIN32)
            # 移除对应的 --no-xxx 标志
            list(REMOVE_ITEM DEPLOYQT_FLAGS "--no-${module}")
        endif()
    endforeach()

    # 添加用户指定的额外标志
    list(APPEND DEPLOYQT_FLAGS ${ARG_DEPLOY_FLAGS})

    # ========== 创建部署目标 ==========
    if(QT_DEPLOYQT_EXECUTABLE)
        # 简单部署目标
        add_custom_target(deploy-${ARG_TARGET_NAME}
                DEPENDS ${ARG_TARGET_NAME} ${ARG_DEPENDS}
                COMMENT "部署 ${ARG_TARGET_NAME} 应用程序..."
        )

        if(WIN32)
            add_custom_command(TARGET deploy-${ARG_TARGET_NAME} POST_BUILD
                    COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "$<TARGET_FILE:${ARG_TARGET_NAME}>"
            )
        elseif(APPLE)
            add_custom_command(TARGET deploy-${ARG_TARGET_NAME} POST_BUILD
                    COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "$<TARGET_BUNDLE_DIR:${ARG_TARGET_NAME}>"
            )
        else()
            add_custom_command(TARGET deploy-${ARG_TARGET_NAME} POST_BUILD
                    COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "$<TARGET_FILE:${ARG_TARGET_NAME}>"
            )
        endif()

        message(STATUS "使用 'cmake --build . --target deploy-${ARG_TARGET_NAME}' 来部署应用")
    endif()

    # ========== 准备安装程序数据 ==========
    add_custom_target(prepare-installer-${ARG_TARGET_NAME}
            DEPENDS ${ARG_TARGET_NAME} ${ARG_DEPENDS}
            COMMENT "准备 ${ARG_PACKAGE_NAME} 安装程序数据..."
    )

    if(QT_DEPLOYQT_EXECUTABLE)
        if(WIN32)
            add_custom_command(TARGET prepare-installer-${ARG_TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${ARG_TARGET_NAME}>" "${INSTALLER_PACKAGE_DATA_DIR}/"
                    COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "${INSTALLER_PACKAGE_DATA_DIR}/$<TARGET_FILE_NAME:${ARG_TARGET_NAME}>"
                    COMMENT "使用windeployqt部署到安装程序数据目录"
            )
        elseif(APPLE)
            add_custom_command(TARGET prepare-installer-${ARG_TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${ARG_TARGET_NAME}>" "${INSTALLER_PACKAGE_DATA_DIR}/${ARG_TARGET_NAME}.app"
                    COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "${INSTALLER_PACKAGE_DATA_DIR}/${ARG_TARGET_NAME}.app"
                    COMMENT "使用macdeployqt部署到安装程序数据目录"
            )
        else()
            add_custom_command(TARGET prepare-installer-${ARG_TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${ARG_TARGET_NAME}>" "${INSTALLER_PACKAGE_DATA_DIR}/"
                    COMMAND ${QT_DEPLOYQT_EXECUTABLE} ${DEPLOYQT_FLAGS} "${INSTALLER_PACKAGE_DATA_DIR}/$<TARGET_FILE_NAME:${ARG_TARGET_NAME}>"
                    COMMENT "使用linuxdeployqt部署到安装程序数据目录"
            )
        endif()
    else()
        # 如果没有deployqt工具，至少复制可执行文件
        if(APPLE)
            add_custom_command(TARGET prepare-installer-${ARG_TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${ARG_TARGET_NAME}>" "${INSTALLER_PACKAGE_DATA_DIR}/${ARG_TARGET_NAME}.app"
                    COMMENT "复制应用程序到安装程序数据目录"
            )
        else()
            add_custom_command(TARGET prepare-installer-${ARG_TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${ARG_TARGET_NAME}>" "${INSTALLER_PACKAGE_DATA_DIR}/"
                    COMMENT "复制应用程序到安装程序数据目录"
            )
        endif()
    endif()

    # ========== 创建安装程序 ==========
    if(QT_BINARYCREATOR_EXECUTABLE)
        # 离线安装程序
        add_custom_target(installer-${ARG_TARGET_NAME}
                DEPENDS prepare-installer-${ARG_TARGET_NAME}
                COMMAND ${QT_BINARYCREATOR_EXECUTABLE}
                --offline-only
                -c "${INSTALLER_CONFIG_DIR}/config.xml"
                -p "${INSTALLER_PACKAGES_DIR}"
                "${INSTALLER_DIR}/${ARG_PACKAGE_NAME}_installer${CMAKE_EXECUTABLE_SUFFIX}"
                COMMENT "创建 ${ARG_PACKAGE_NAME} 离线安装程序, "${INSTALLER_DIR}/${ARG_PACKAGE_NAME}_installer${CMAKE_EXECUTABLE_SUFFIX}""
        )

        # 在线安装程序
        add_custom_target(online-installer-${ARG_TARGET_NAME}
                DEPENDS prepare-installer-${ARG_TARGET_NAME}
                COMMAND ${QT_BINARYCREATOR_EXECUTABLE}
                --online-only
                -c "${INSTALLER_CONFIG_DIR}/config.xml"
                -p "${INSTALLER_PACKAGES_DIR}"
                "${INSTALLER_DIR}/${ARG_PACKAGE_NAME}_online_installer${CMAKE_EXECUTABLE_SUFFIX}"
                COMMENT "创建 ${ARG_PACKAGE_NAME} 在线安装程序, "${INSTALLER_DIR}/${ARG_PACKAGE_NAME}_online_installer${CMAKE_EXECUTABLE_SUFFIX}""
        )

        message(STATUS "${ARG_PACKAGE_NAME} 安装程序构建目标:")
        message(STATUS "  cmake --build . --target installer-${ARG_TARGET_NAME}        # 离线安装程序")
        message(STATUS "  cmake --build . --target online-installer-${ARG_TARGET_NAME} # 在线安装程序")
        if(ARG_INSTALLED_EXE_NAME)
            message(STATUS "  安装后exe名称: ${ARG_INSTALLED_EXE_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
        else()
            string(REPLACE " " "_" SAFE_NAME "${ARG_PACKAGE_NAME}")
            message(STATUS "  安装后exe名称: ${SAFE_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
        endif()
    else()
        message(WARNING "未找到Qt Installer Framework工具 (binarycreator)")
        message(STATUS "请安装Qt Installer Framework或设置正确的路径")
    endif()

    # ========== 清理目标 ==========
    add_custom_target(clean-installer-${ARG_TARGET_NAME}
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${INSTALLER_DIR}"
            COMMENT "清理 ${ARG_PACKAGE_NAME} 安装程序文件..."
    )

    # ========== 打印部署配置信息 ==========
    if(ARG_MINIMAL_DEPLOYMENT OR ARG_EXCLUDE_MODULES OR ARG_INSTALLED_EXE_NAME)
        message(STATUS "")
        message(STATUS "========== ${ARG_PACKAGE_NAME} 部署配置 ==========")
        if(ARG_MINIMAL_DEPLOYMENT)
            message(STATUS "最小化部署: 已启用")
        endif()
        if(ARG_EXCLUDE_MODULES)
            message(STATUS "排除的模块: ${ARG_EXCLUDE_MODULES}")
        endif()
        if(ARG_INCLUDE_MODULES)
            message(STATUS "包含的模块: ${ARG_INCLUDE_MODULES}")
        endif()
        if(ARG_INSTALLED_EXE_NAME)
            message(STATUS "安装后exe名称: ${ARG_INSTALLED_EXE_NAME}")
        endif()
        message(STATUS "==========================================")
    endif()

    # ========== 安装配置 ==========
    #    include(GNUInstallDirs)
    #    install(TARGETS ${ARG_TARGET_NAME}
    #            BUNDLE DESTINATION .
    #            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    #            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    #    )
endfunction()

# ========== 便捷宏 ==========
macro(add_qt_installer target_name)
    setup_qt_deployment(
            TARGET_NAME ${target_name}
            ${ARGN}
    )
endmacro()

# ========== 自动分析并最小化部署 ==========
function(auto_minimize_deployment target_name)
    # 获取目标链接的库
    get_target_property(LINKED_LIBS ${target_name} LINK_LIBRARIES)

    set(USED_MODULES "")
    set(EXCLUDE_MODULES "")

    # 分析实际使用的Qt模块
    foreach(lib ${LINKED_LIBS})
        if(lib MATCHES "Qt.*::Core")
            list(APPEND USED_MODULES "core")
        elseif(lib MATCHES "Qt.*::Widgets")
            list(APPEND USED_MODULES "widgets" "gui")
        elseif(lib MATCHES "Qt.*::Network")
            list(APPEND USED_MODULES "network")
        elseif(lib MATCHES "Qt.*::Sql")
            list(APPEND USED_MODULES "sql")
        elseif(lib MATCHES "Qt.*::Multimedia")
            list(APPEND USED_MODULES "multimedia")
        elseif(lib MATCHES "Qt.*::Quick")
            list(APPEND USED_MODULES "quick" "qml")
        elseif(lib MATCHES "Qt.*::PrintSupport")
            list(APPEND USED_MODULES "printsupport")
        elseif(lib MATCHES "Qt.*::Svg")
            list(APPEND USED_MODULES "svg")
        elseif(lib MATCHES "Qt.*::Xml")
            list(APPEND USED_MODULES "xml")
        elseif(lib MATCHES "Qt.*::Concurrent")
            list(APPEND USED_MODULES "concurrent")
        endif()
    endforeach()

    # 所有可能的Qt模块
    set(ALL_MODULES
            network sql multimedia multimediawidgets quick qml
            printsupport svg concurrent xml test dbus help
            designer positioning 3dcore 3drender 3dinput 3dlogic
            3dextras 3danimation charts datavisualization
            serialport serialbus webenginecore webengine websockets
            bluetooth gamepad location nfc remoteobjects sensors
            waylandcompositor webview virtualkeyboard pdf
            quickcontrols quickcontrols2 quicktemplates2 quickwidgets
            scxml
    )

    # 确定要排除的模块
    foreach(module ${ALL_MODULES})
        if(NOT module IN_LIST USED_MODULES)
            list(APPEND EXCLUDE_MODULES ${module})
        endif()
    endforeach()

    message(STATUS "自动最小化部署分析:")
    message(STATUS "  使用的模块: ${USED_MODULES}")
    message(STATUS "  将排除: ${EXCLUDE_MODULES}")

    # 设置部署
    setup_qt_deployment(
            TARGET_NAME ${target_name}
            EXCLUDE_MODULES ${EXCLUDE_MODULES}
            ${ARGN}
    )
endfunction()