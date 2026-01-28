#!/bin/bash

PROJECTHOME=$(pwd)
SVFHOME=$PROJECTHOME/"../SVF-3.1"
LLVM_DIR=$SVFHOME/"../llvm-16.0.4.obj"

Z3Home="z3.obj"

export Z3_DIR="$SVFHOME/$Z3Home"

export LLVM_SRC="$SVFHOME/../llvm-16.0.4"
export LLVM_OBJ="$LLVM_DIR"
export LLVM_DIR="$LLVM_DIR"
export PATH=$LLVM_DIR/bin:$PATH

export SVF_DIR=$SVFHOME/