cmake_minimum_required(VERSION 3.10)

project(KLEEHelper)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g)
endif()

include(../cmake_deps/llvm.cmake)
include(../cmake_deps/sharedUtils.cmake)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Generate protobuf files
set(Protobuf_PROTOC_EXECUTABLE /usr/bin/protoc)
find_package(Protobuf 3.12 REQUIRED)
add_custom_command(
  OUTPUT modelInputs.pb.cc modelInputs.pb.h
  COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/.. --cpp_out=. modelInputs.proto
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../modelInputs.proto
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Add protobuf files to the target
add_executable(klee_helper main.cpp kleeEntryBuilder.cpp modelInputs.pb.cc)

target_link_libraries(klee_helper PRIVATE
    sharedUtils
    llvm16
    ${PROTOBUF_LIBRARIES}
    yaml-cpp
)