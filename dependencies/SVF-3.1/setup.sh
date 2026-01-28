#!/usr/bin/env bash

echo "Setting up environment for SVF"

SVFHOME=$(pwd)
LLVM_DIR=$SVFHOME/"../llvm-16.0.4.obj"

export Z3_DIR="$SVFHOME/$Z3Home"
export CTIR_DIR="$SVFHOME/$CTIRHome/bin"
export LLVM_SRC="$SVFHOME/../llvm-16.0.4"
export LLVM_OBJ="$LLVM_DIR"
export LLVM_DIR="$LLVM_DIR"
export PATH=$LLVM_DIR/bin:$PATH