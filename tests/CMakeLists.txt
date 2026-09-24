add_executable(test_core test_core.c)
target_link_libraries(test_core PRIVATE todo_core)
add_test(NAME test_core COMMAND test_core)
