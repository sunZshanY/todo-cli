cmake_minimum_required(VERSION 3.20)
project(todo-cli VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(TODO_BUILD_TESTS "构建测试" ON)

find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    message(STATUS "使用系统 SQLite: ${SQLite3_VERSION}")
    set(TODO_SQLITE_LIB SQLite::SQLite3)
else()
    message(STATUS "未找到系统 SQLite，下载官方 amalgamation 静态编译")
    include(FetchContent)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Declare(sqlite
        URL https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
        URL_HASH SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_GetProperties(sqlite)
    if(NOT sqlite_POPULATED)
        FetchContent_Populate(sqlite)
    endif()
    add_library(sqlite3 STATIC ${sqlite_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${sqlite_SOURCE_DIR})
    if(MSVC)
        target_compile_definitions(sqlite3 PRIVATE
            _CRT_SECURE_NO_WARNINGS SQLITE_ENABLE_COLUMN_METADATA)
    endif()
    set(TODO_SQLITE_LIB sqlite3)
endif()

set(TODO_SOURCES
    src/util.c
    src/config.c
    src/db.c
    src/todo.c
    src/cli.c
    src/commands.c
    src/ext.c)

add_library(todo_core STATIC ${TODO_SOURCES})
target_include_directories(todo_core PUBLIC
    include
    ${CMAKE_BINARY_DIR}/gen)
target_link_libraries(todo_core PUBLIC ${TODO_SQLITE_LIB})

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/gen/todo)
configure_file(cmake/todo_version.h.in
    ${CMAKE_BINARY_DIR}/gen/todo/todo_version.h @ONLY)

if(MSVC)
    target_compile_definitions(todo_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(todo_core PRIVATE -Wall -Wextra)
endif()

add_executable(todo-cli src/main.c)
set_target_properties(todo-cli PROPERTIES OUTPUT_NAME todo)
target_link_libraries(todo-cli PRIVATE todo_core)
target_compile_definitions(todo-cli PRIVATE
    TODO_DEFAULT_SCRIPT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/python")
if(MSVC)
    target_compile_definitions(todo-cli PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if(MINGW)
    target_link_options(todo-cli PRIVATE -municode)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(todo-cli PRIVATE -Wall -Wextra)
endif()

install(TARGETS todo-cli RUNTIME DESTINATION bin)
install(DIRECTORY python/ DESTINATION share/todo-cli/python)
install(FILES docs/config.ini.example DESTINATION share/todo-cli)
install(FILES docs/todo.1 DESTINATION share/man/man1)
install(FILES include/todo.h DESTINATION include)
install(DIRECTORY include/todo/ DESTINATION include/todo)
install(FILES ${CMAKE_BINARY_DIR}/gen/todo/todo_version.h DESTINATION include/todo)

if(TODO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
