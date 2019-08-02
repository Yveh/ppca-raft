cmake_minimum_required(VERSION 3.10)
project(raft)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(protos)

find_package (Threads)

add_executable(main main.cpp)

target_link_libraries(main proto_svr proto_cli grpc++ gpr Threads::Threads)

target_include_directories(main PUBLIC ${CMAKE_CURRENT_BINARY_DIR} protos)

