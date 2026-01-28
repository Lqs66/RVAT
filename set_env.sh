#!/bin/bash

PROJECTHOME=$(pwd)
SVFHOME=$PROJECTHOME/"dependencies/SVF-3.1"
LLVM_DIR=$PROJECTHOME/"dependencies/llvm-16.0.4/build"
Z3Home="z3.obj"
DTMC=$PROJECTHOME

export Z3_DIR="$SVFHOME/$Z3Home"
# export YAML_CPP_DIR="$PROJECTHOME/dependencies/yaml-cpp"
# export RAPIDJSON_DIR="$PROJECTHOME/dependencies/rapidjson"
# export BOOST_DIR="$PROJECTHOME/dependencies/boost_1_74_0"
export SHARED_UTILS_DIR="$PROJECTHOME/sharedUtils"
export LLVM_SRC="$SVFHOME/../llvm-16.0.4"
export LLVM_OBJ="$LLVM_DIR"
export LLVM_DIR="$LLVM_DIR"
export PATH=$LLVM_DIR/bin:$PATH
export SVF_DIR=$SVFHOME/
export DTMC="$DTMC"

# for KLEE
export LLVM_ROOT="$LLVM_DIR/.."
export Z3_ROOT="$Z3_DIR"
export CVC5_DIR="$PROJECTHOME/dependencies/KLEE/cvc5-1.3.0/cvc5_install/lib/cmake/cvc5"
export KLEE_ROOT="$PROJECTHOME/dependencies/KLEE"

# PGFuzz config
export PGFUZZ_HOME="$PROJECTHOME/PGFuzz/"
export ARDUPILOT_HOME="$PROJECTHOME/flight-control/arducopter-4.4/"
export PX4_HOME="$PROJECTHOME/flight-control/PX4-1.15.2/"