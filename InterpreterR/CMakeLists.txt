cmake_minimum_required(VERSION 3.5)
project(InterpreterR)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Set compile options based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Configuring for Debug build")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -DDEBUG")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Configuring for Release build")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

include (../cmake_deps/llvm.cmake)
include(../cmake_deps/sharedUtils.cmake)

# Add include directories FIRST (before LLVM includes)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ExecutionEngine)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Interpreter)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/model)

include_directories (${INST_SCHEDULER_DIR})
include_directories (${Boost_INCLUDE_DIRS})
include_directories (${LLVM_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Set protobuf source path
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../modelInputs.proto")

message("Protobuf source file: ${PROTO_FILE}")

# Generate protobuf files
set(Protobuf_PROTOC_EXECUTABLE /usr/bin/protoc)
find_package(Protobuf 3.12 REQUIRED)

# Conditionally find libffi (required for ExternalFunctions.cpp)
option(LLVM_ENABLE_FFI "Enable libffi for external function calls" OFF)
if(LLVM_ENABLE_FFI)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFI REQUIRED libffi)
    message(STATUS "libffi enabled: ${FFI_LIBRARIES}")
endif()

add_custom_command(
  OUTPUT modelInputs.pb.cc modelInputs.pb.h
  COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/.. --cpp_out=. ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Generating protobuf files from ${PROTO_FILE}"
)

# Create a separate library for CFA modeling components
add_library(cfaModel STATIC
    model/CFABuilder.cpp
    model/CFAModelFactory.cpp
)

# Set compile options for cfaModel library
target_compile_options(cfaModel PRIVATE
    $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra>
    $<$<CONFIG:Release>:-O3>
)
target_compile_definitions(cfaModel PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

# Link LLVM libraries to cfaModel (needed for CFABuilder.cpp and CFAModelFactory.cpp)
target_link_libraries(cfaModel PUBLIC sharedUtils llvm16)
target_include_directories(cfaModel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/model
)

# Enable Position Independent Code for static library
set_target_properties(cfaModel PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(interpreter STATIC
    ExecutionEngine/ExecutionEngine.cpp
    # ExecutionEngine/GenericValue.h
    Interpreter/ExternalFunctions.cpp
    Interpreter/Interpreter.cpp
    Interpreter/Execution.cpp
    Interpreter/InstrsInterpreter.cpp
    Interpreter/CFAExecution.cpp
)

# Set debug/release specific options for interpreter library
target_compile_options(interpreter PRIVATE
    $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra>
    $<$<CONFIG:Release>:-O3>
)

# Link cfaModel library to interpreter (needed by CFAExecution.cpp)
target_link_libraries(interpreter PUBLIC cfaModel)

# Conditionally link libffi to interpreter library
if(LLVM_ENABLE_FFI)
    target_link_libraries(interpreter PUBLIC ${FFI_LIBRARIES})
    target_include_directories(interpreter PUBLIC ${FFI_INCLUDE_DIRS})
    # Define macros that ExternalFunctions.cpp expects
    target_compile_definitions(interpreter PUBLIC 
        LLVM_ENABLE_FFI
        HAVE_FFI_CALL
        HAVE_FFI_H
    )
endif()

# Enable Position Independent Code for static library so it can be linked into shared library
set_target_properties(interpreter PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Build a separate shared library from base.cpp and link it with the interpreter and cfaModel
add_library(base SHARED
  base.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/modelInputs.pb.cc
)
target_link_libraries(base PUBLIC interpreter cfaModel sharedUtils llvm16 ${PROTOBUF_LIBRARIES})
target_compile_options(base PRIVATE
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)
target_compile_definitions(base PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

add_executable(InterpreterR InterpreterR.cpp)
target_link_libraries(InterpreterR PUBLIC interpreter sharedUtils llvm16)
target_compile_options(InterpreterR PRIVATE
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)
target_compile_definitions(InterpreterR PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

# Compile base.cpp separately for the test with a special flag to skip initInterpreter
# Only build testMain in Debug mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(testMain 
        testMain.cpp
    )
    target_compile_definitions(testMain PRIVATE TEST_MODE DEBUG)
    target_link_libraries(testMain PUBLIC base)
    target_compile_options(testMain PRIVATE
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3>
    )
endif()