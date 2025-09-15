project(common)

find_jni()

my_add_target(${PROJECT_NAME} STATIC false)

link_jni()

target_link_libraries(${PROJECT_NAME} PUBLIC
        imagehlp
        shlwapi
)