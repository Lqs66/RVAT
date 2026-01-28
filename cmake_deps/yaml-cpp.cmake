find_package(yaml-cpp REQUIRED)

if(YAML_CPP_FOUND)
    message(STATUS "Found yaml-cpp: ${YAML_CPP_VERSION}")
    message(STATUS "yaml-cpp include directory: ${YAML_CPP_INCLUDE_DIRS}")
    message(STATUS "yaml-cpp library directory: ${YAML_CPP_LIBRARY_DIRS}")
    message(STATUS "yaml-cpp libraries: ${YAML_CPP_LIBRARIES}")
endif()