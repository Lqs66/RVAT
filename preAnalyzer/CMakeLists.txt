cmake_minimum_required(VERSION 3.5)
project(indirectCallAnalysis)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE
      STRING "Build type (default Release):" FORCE)
endif()

include(../cmake_deps/boost.cmake)
include(../cmake_deps/llvm.cmake)
include(../cmake_deps/yaml-cpp.cmake)
include(../cmake_deps/sharedUtils.cmake)
include(../cmake_deps/svf.cmake)

include_directories("include")
include_directories("src")

set(DeepType
    "src/Common.cc"
    "src/Analyzer.cc"
    "src/CallGraph.cc"
)

add_executable(inCallAnalysis "src/inCallAnalysis.cpp" "src/findVirtualtargets.cpp" "src/inCallSVF.cpp" ${DeepType})
target_link_libraries(inCallAnalysis ${SVF_LIB} sharedUtils llvm16 ${Boost_LIBRARIES})
target_link_libraries(inCallAnalysis ${Z3_LIBRARIES})

add_executable(addTargetsMD "src/addTargetsMD.cpp")
target_link_libraries(addTargetsMD yaml-cpp llvm16 ${Boost_LIBRARIES})
