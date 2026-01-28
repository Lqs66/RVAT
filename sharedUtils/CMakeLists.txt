cmake_minimum_required(VERSION 3.5)
project(sharedUtils)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE
      STRING "Build type (default Release):" FORCE)
endif()

include(../cmake_deps/llvm.cmake)
include(../cmake_deps/yaml-cpp.cmake)

include_directories("include")

file(GLOB SOURCES "src/*.cpp")

add_library(sharedUtils SHARED ${SOURCES})
target_link_libraries(sharedUtils PRIVATE llvm16)
target_link_libraries(sharedUtils PUBLIC yaml-cpp)