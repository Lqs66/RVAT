cmake_minimum_required(VERSION 3.5)
project(instrumenter)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE
      STRING "Build type (default Release):" FORCE)
endif()

include(../cmake_deps/llvm.cmake)
include(../cmake_deps/yaml-cpp.cmake)
include(../cmake_deps/sharedUtils.cmake)

include_directories("include")
include_directories("src")

add_executable(instrumenter "src/instrumenter.cpp" "src/inputgetter.cpp" "src/basicBlockTimer.cpp" "src/main.cpp")
target_link_libraries(instrumenter sharedUtils yaml-cpp llvm16)