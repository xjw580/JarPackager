project(launcher)

find_jni()

my_add_target(${PROJECT_NAME} EXECUTABLE false)

link_jni()
target_link_libraries(${PROJECT_NAME} PRIVATE
        crypt32
        common
        gdiplus
        msimg32
)