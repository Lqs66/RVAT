#!/bin/bash

PROJECTHOME=$(pwd)

# Function to display usage information
usage() {
    echo "Usage: $0 [command]"
    echo "Commands:"
    echo "  all                           Build all projects, Default is 'all'"
    echo "  tools                         Build all projects except dependencies."
    echo "  dep                           Build dependencies."
    echo "  su [-d]                       Build sharedUtils shared lib. Use '-d' for debug build. Default is release build."
    echo "  llvm-pdg [-d]                 Build llvm-pdg. Use '-d' for debug build. Default is release build."
    echo "  pa [-d]                       Build pre analyzer. Use '-d' for debug build. Default is release build."
    echo "  mtg [-d]                      Build modelTempGen. Use '-d' for debug build. Default is release build."
    echo "  ist [-d]                      Build instrumenter. Use '-d' for debug build. Default is release build."
    echo "  klee [-d]                     Build KLEE entry point instrumenter. Use '-d' for debug build. Default is release build."
    echo "  ir [-d]                       Build InterpreterR. Use '-d' for debug build. Default is release build."
    echo "  clean                         Clean all projects."
    echo "  clean-tools                   Clean all projects except dependencies."
    echo "  clean-su                      Clean sharedUtils."
    echo "  clean-ist                     Clean instrumenter."
    echo "  clean-klee                    Clean KLEE entry point instrumenter."
    echo "  clean-llvm-pdg                Clean llvm-pdg."
    echo "  clean-mtg                     Clean modelTempGen."
    echo "  clean-pa                      Clean preAnalyzer."
    echo "  clean-ir                      Clean InterpreterR."
    echo "  help                          Display this help message."
    # exit 1
}

build_all() {
    echo "Building all projects..."
    build_dependencies
    build_pre_analyzer
    build_llvm_pdg
    build_modelTempGen
    build_instrumenter
    build_klee
    build_interpreterR
    # build_arducopter
}

build_tools() {
    echo "Building all projects except dependencies..."
    build_pre_analyzer
    build_llvm_pdg
    build_modelTempGen
    build_instrumenter
    build_klee
    build_interpreterR
    # build_arducopter
}

build_dependencies() {
    echo "Building dependencies..."
    cd $PROJECTHOME/dependencies
    bash lib_build.sh
    cd $PROJECTHOME
}

build_pre_analyzer() {
    build_sharedUtils $1
    local build_type=$1
    cd $PROJECTHOME/preAnalyzer
    if [ ! -d "build" ]; then
        mkdir build
    fi
    cd build
    if [ "$build_type" == "-d" ]; then
        echo "Building preAnalyzer with build type: DEBUG..."
        cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-O0 -g" ..
    else
        echo "Building preAnalyzer with build type: Release..."
        cmake -DCMAKE_BUILD_TYPE=Release ..
    fi
    make
    cd $PROJECTHOME
}

build_sharedUtils() {
    local build_type=$1
    echo "Building sharedUtils shared lib..."
    cd $PROJECTHOME/sharedUtils
    if [ ! -d "build" ]; then
        mkdir build
    fi
    cd build
    if [ "$build_type" == "-d" ]; then
        echo "Building sharedUtils with build type: DEBUG..."
        cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-O0 -g" ..
    else
        echo "Building sharedUtils with build type: Release..."
        cmake -DCMAKE_BUILD_TYPE=Release ..
    fi
    make
    cd $PROJECTHOME
}

build_llvm_pdg() {
    build_sharedUtils $1
    set -e
    local build_type=$1
    echo "Building llvm-pdg..."
    cd $PROJECTHOME/llvm-pdg
    if [ "$build_type" == "-d" ]; then
        echo "Building llvm-pdg with build type: DEBUG..."
        make debug
    else
        echo "Building llvm-pdg with build type: Release..."
        make
    fi
    cd $PROJECTHOME
}

build_modelTempGen() {
    build_sharedUtils $1
    local build_type=$1
    cd $PROJECTHOME/modelTempGen
    if [ ! -d "build" ]; then
        mkdir build
    fi
    cd build
    if [ "$build_type" == "-d" ]; then
        echo "Building modelTempGen with build type: DEBUG..."
        cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-O0 -g" ..
    else
        echo "Building modelTempGen with build type: Release..."
        cmake -DCMAKE_BUILD_TYPE=Release ..
    fi
    make
    cd $PROJECTHOME
}

build_instrumenter() {
    build_sharedUtils $1
    local build_type=$1
    cd $PROJECTHOME/instrumenter
    if [ ! -d "build" ]; then
        mkdir build
    fi
    cd build
    if [ "$build_type" == "-d" ]; then
        echo "Building instrumenter with build type: DEBUG..."
        cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-O0 -g" ..
    else
        echo "Building instrumenter with build type: Release..."
        cmake -DCMAKE_BUILD_TYPE=Release ..
    fi
    make
    cd $PROJECTHOME
}

build_klee() {
    build_sharedUtils $1
    local build_type=$1
    cd $PROJECTHOME/KLEE
        if [ ! -d "build" ]; then
        mkdir build
    fi
    cd build
    if [ "$build_type" == "-d" ]; then
        echo "Building KLEE entry point instrumenter with build type: DEBUG..."
        cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-O0 -g" ..
    else
        echo "Building KLEE entry point instrumenter with build type: Release..."
        cmake -DCMAKE_BUILD_TYPE=Release ..
    fi
    make
    cd $PROJECTHOME
}

# build_uppllvm() {
#     build_sharedUtils $1
#     local build_type=$1
#     cd $PROJECTHOME/uppllvm
#     if [ ! -d "build" ]; then
#         mkdir build
#     fi
#     cd build
#     if [ "$build_type" == "-d" ]; then
#         echo "Building uppllvm with build type: DEBUG..."
#         cmake -DTEST=ON -DCMAKE_CXX_COMPILER=g++ -DCMAKE_CXX_FLAGS="-O0 -g" ..
#     else
#         echo "Building uppllvm with build type: Release..."
#         cmake -DCMAKE_CXX_COMPILER=g++ ..
#     fi
#     make -j4
#     cd $PROJECTHOME
# }

build_interpreterR() {
    build_sharedUtils $1
    local build_type=$1
    cd $PROJECTHOME/InterpreterR
    if [ ! -d "build" ]; then
        mkdir build
    fi
    cd build
    if [ "$build_type" == "-d" ]; then
        echo "Building InterpreterR with build type: DEBUG..."
        cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-O0 -g" ..
    else
        echo "Building InterpreterR with build type: Release..."
        cmake -DCMAKE_BUILD_TYPE=Release ..
    fi
    make
    cd $PROJECTHOME
}

# build_arducopter() {
#     cd $PROJECTHOME/flight-control
#     make
#     cd $PROJECTHOME
# }

clean_all() {
    clean_sharedUtils
    clean_incall_analyzer
    clean_modelInputTempGen
    clean_instrumenter
    clean_klee
    clean_interpreterR
}

clean_tools() {
    echo "Cleaning all projects except dependencies..."
    clean_sharedUtils
    clean_incall_analyzer
    clean_llvm_pdg
    clean_modelTempGen
    clean_instrumenter
    clean_klee
    clean_interpreterR
}

clean_sharedUtils() {
    echo "Cleaning sharedUtils..."
    cd $PROJECTHOME/sharedUtils
    rm -rf build
    cd $PROJECTHOME
}

clean_incall_analyzer() {
    echo "Cleaning incall_analyzer..."
    cd $PROJECTHOME/preAnalyzer
    rm -rf build
    cd $PROJECTHOME
}

clean_llvm_pdg(){
    cd $PROJECTHOME/llvm-pdg
    make clean
}

clean_instrumenter() {
    # clean_sharedUtils
    echo "Cleaning instrumenter..."
    cd $PROJECTHOME/instrumenter
    rm -rf build
    cd $PROJECTHOME
}

clean_klee() {
    # clean_sharedUtils
    echo "Cleaning KLEE entry point instrumenter..."
    cd $PROJECTHOME/KLEE
    rm -rf build
    cd $PROJECTHOME
}

clean_modelTempGen() {
    # clean_sharedUtils
    echo "Cleaning modelTempGen..."
    cd $PROJECTHOME/modelTempGen
    rm -rf build
    cd $PROJECTHOME
}

# clean_uppllvm() {
#     # clean_sharedUtils
#     echo "Cleaning uppllvm..."
#     cd $PROJECTHOME/uppllvm
#     rm -rf build
#     cd $PROJECTHOME
# }

clean_interpreterR() {
    # clean_sharedUtils
    echo "Cleaning InterpreterR..."
    cd $PROJECTHOME/InterpreterR
    rm -rf build
    cd $PROJECTHOME
}

# Main program entry point
if [ -z "$1" ]; then
    COMMAND="all"
else
    COMMAND=$1
fi

case $COMMAND in
    all)
        build_all
        ;;
    tools)
        build_tools
        ;;
    dep)
        build_dependencies
        ;;
    su)
        build_sharedUtils $2
        ;;
    llvm-pdg)
        build_llvm_pdg $2
        ;;
    pa)
        build_pre_analyzer $2
        ;;
    mtg)
        build_modelTempGen $2
        ;;
    ist)
        build_instrumenter  $2
        ;;
    klee)
        build_klee $2
        ;;
    ir)
        build_interpreterR $2
        ;;
    clean)
        clean_all
        ;;
    clean-tools)
        clean_tools
        ;;
    clean-su)
        clean_sharedUtils
        ;;
    clean-llvm-pdg)
        clean_llvm_pdg
        ;;
    clean-pa)
        clean_incall_analyzer
        ;;
    clean-ist)
        clean_instrumenter
        ;;
    clean-klee)
        clean_klee
        ;;
    clean-mtg)
        clean_modelTempGen
        ;;
    clean-ir)
        clean_interpreterR
        ;;
    help)
        usage
        ;;
    *)
        usage
        ;;
esac