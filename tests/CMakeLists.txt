add_executable(test_core test_core.c)
target_link_libraries(test_core PRIVATE todo_core)
add_test(NAME test_core COMMAND test_core)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME test_cli
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_cli.py
            $<TARGET_FILE:todo-cli>)
