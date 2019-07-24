cmake_minimum_required(VERSION 3.10)
project(helloworld)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(protos)

find_package (Threads)

add_executable(server server.cpp)
add_executable(client client.cpp)
target_link_libraries(server proto_svr proto_cli grpc++ gpr Threads::Threads)
target_link_libraries(client proto_svr proto_cli grpc++ gpr Threads::Threads)

target_include_directories(server PUBLIC ${CMAKE_CURRENT_BINARY_DIR} protos)
target_include_directories(client PUBLIC ${CMAKE_CURRENT_BINARY_DIR} protos)

