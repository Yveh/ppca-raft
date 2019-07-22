cmake_minimum_required(VERSION 3.10)
project(protos)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

find_package(Protobuf REQUIRED)
find_package(grpc REQUIRED)
FILE(GLOB PROTOFILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTOFILES})
MESSAGE(STATUS "CLI SRC " ${PROTO_SRCS} ", CLI HEAD " ${PROTO_HDRS})
add_library(proto_cli STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(proto_cli protobuf)
target_include_directories(proto_cli PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

protobuf_generate_grpc_cpp(PROTO_GRPC_SRCS PROTO_GRPC_HDRS ${PROTOFILES})
MESSAGE(STATUS "SVR SRC " ${PROTO_GRPC_SRCS} ", SVR HEAD " ${PROTO_GRPC_HDRS})
add_library(proto_svr STATIC ${PROTO_GRPC_SRCS} ${PROTO_GRPC_HDRS} ${PROTO_HDRS})
target_link_libraries(proto_svr protobuf)
target_include_directories(proto_svr PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

