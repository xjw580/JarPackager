project(attacher)

my_add_target(${PROJECT_NAME} EXECUTABLE false)

target_link_libraries(
        ${PROJECT_NAME} PRIVATE
        common
)