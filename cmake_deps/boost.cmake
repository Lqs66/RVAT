# Try to find Boost
find_package(Boost REQUIRED COMPONENTS program_options CONFIG)

if(Boost_FOUND)
    message(STATUS "Find Boost: ${Boost_VERSION}")
    message(STATUS "Boost include directory: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost library directory: ${Boost_LIBRARY_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
endif()