# Shared Utils
if (EXISTS "${SHARED_UTILS_DIR}")
else()
    set(SHARED_UTILS_DIR $ENV{SHARED_UTILS_DIR})
    if(EXISTS "${SHARED_UTILS_DIR}")
    else()
        message(FATAL_ERROR "\
            WARNING: The SHARED_UTILS_DIR var was not set (required for an out-of-source build)!\n
            Please set this to environment variable to point to the SHARED_UTILS_DIR directory or set this variable to cmake configuration\n
            (e.g. on linux: export SHARED_UTILS_DIR=/path/to/dir) \n or (make the project via: cmake -DSHARED_UTILS_DIR=your_path_to_sharedUtils)")
    endif()
endif()

set(SHARED_UTILS_BIN "${SHARED_UTILS_DIR}/build")
set(SHARED_UTILS_INCLUDE_DIR "${SHARED_UTILS_DIR}/include")
set(SHARED_UTILS_SOURCE_DIR "${SHARED_UTILS_DIR}/src")
include_directories(${SHARED_UTILS_INCLUDE_DIR})
include_directories(${SHARED_UTILS_SOURCE_DIR})
message(STATUS "sharedUtils include directory: ${SHARED_UTILS_INCLUDE_DIR}")
message(STATUS "sharedUtils source directory: ${SHARED_UTILS_SOURCE_DIR}")

set(SHARED_UTILS_BIN_LIB ${SHARED_UTILS_BIN}/libsharedUtils.so)

add_library (sharedUtils INTERFACE)
target_include_directories(sharedUtils INTERFACE ${SHARED_UTILS_INCLUDE_DIR})
target_link_libraries(sharedUtils INTERFACE ${SHARED_UTILS_BIN_LIB})