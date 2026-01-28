cmake_minimum_required(VERSION 3.5)
project(MTGen)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE
      STRING "Build type (default Release):" FORCE)
endif()

include(../cmake_deps/llvm.cmake)
include(../cmake_deps/sharedUtils.cmake)


include_directories("include")
include_directories("src")
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

add_executable(MTGen "src/Main.cpp" "src/ModelInputTempGen.cpp" modelInputs.pb.cc)
target_link_libraries(MTGen sharedUtils llvm16 ${PROTOBUF_LIBRARIES})

add_executable(globalVarsInfo "src/globalVarsInfo.cpp")
target_link_libraries(globalVarsInfo sharedUtils llvm16)

add_executable(structExpander "src/structExpander.cpp")
target_link_libraries(structExpander sharedUtils llvm16)


