project(common)

find_jni()

my_add_target(${PROJECT_NAME} STATIC false)

link_jni()