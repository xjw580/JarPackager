# 寻找jni
macro(find_jni)

    find_path(JNI_INCLUDE_DIR jni.h
            PATHS
            $ENV{JAVA_HOME}/include
            /usr/lib/jvm/default/include
            /usr/local/include/java
            /usr/include/java
            DOC "Path to JNI header files"
    )

    if(JNI_INCLUDE_DIR)
        include_directories(${JNI_INCLUDE_DIR})
        # 查找平台特定目录
        if(WIN32)
            find_path(JNI_INCLUDE_DIR2 jni_md.h
                    PATHS ${JNI_INCLUDE_DIR}/win32
                    DOC "Path to JNI platform-specific header files"
            )
        elseif(APPLE)
            find_path(JNI_INCLUDE_DIR2 jni_md.h
                    PATHS ${JNI_INCLUDE_DIR}/darwin
                    DOC "Path to JNI platform-specific header files"
            )
        else()
            find_path(JNI_INCLUDE_DIR2 jni_md.h
                    PATHS ${JNI_INCLUDE_DIR}/linux
                    DOC "Path to JNI platform-specific header files"
            )
        endif()

        if(JNI_INCLUDE_DIR2)
            include_directories(${JNI_INCLUDE_DIR2})
        endif()
    else()
        message(FATAL_ERROR "JNI headers (jni.h) not found")
    endif()

endmacro()

# 链接jni
macro(link_jni)

    if(DEFINED ENV{JAVA_HOME})
        message(STATUS "Found JAVA_HOME: $ENV{JAVA_HOME}")
        set(JAVA_HOME $ENV{JAVA_HOME})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${JAVA_HOME}\\lib\\jvm.lib)
    else()
        message(FATAL_ERROR "Not setting JAVA_HOME ENV!")
    endif()

endmacro()