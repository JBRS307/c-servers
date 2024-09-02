cmake_minimum_required(VERSION 3.22)

project(sequential_server)

add_executable(main main.c utils.c)

set_target_properties(
    main
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)