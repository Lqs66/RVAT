cmake_minimum_required(VERSION 3.5)
project(pdg)

set (CMAKE_C_COMPILER "gcc")
set (CMAKE_CXX_COMPILER "g++")

# Build type
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE
      STRING "Build type (default Release):" FORCE)
endif()

include(../cmake_deps/llvm.cmake)
include(../cmake_deps/svf.cmake)
include(../cmake_deps/sharedUtils.cmake)

include_directories("include")
include_directories("src")

file(GLOB SOURCES "src/*.cpp")
list(REMOVE_ITEM SOURCES "src/Main.cpp")
file(GLOB HEADERS "include/*.hh")

add_library(pdg SHARED ${HEADERS} ${SOURCES})
target_link_libraries(pdg sharedUtils llvm16 ${Z3_LIBRARIES} ${SVF_LIB})

add_executable(llvm-pdg src/Main.cpp)
target_link_libraries(llvm-pdg pdg)

