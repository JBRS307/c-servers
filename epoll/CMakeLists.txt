cmake_minimum_required(VERSION 3.22)

project(epoll_server)

add_compile_options(-Wall -g -std=gnu17)

add_executable(server server.c utils.c)

set_target_properties(
    server
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)