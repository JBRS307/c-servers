cmake_minimum_required(VERSION 3.22)

project(blocking_listener)

add_compile_options(-Wall)

add_executable(main main.c utils.c)

set_target_properties(
    main
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)