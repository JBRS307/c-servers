cmake_minimum_required(VERSION 3.22)

project(select_server)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(main main.c utils.c)

set_target_properties(
    main
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)