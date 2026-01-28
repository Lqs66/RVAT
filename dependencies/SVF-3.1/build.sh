#!/usr/bin/env bash
# type './build.sh'       for release build
# type './build.sh debug' for debug build
# if the LLVM_DIR variable is not set, LLVM will be downloaded.
#
# Dependencies include: build-essential libncurses5 libncurses-dev cmake zlib1g-dev
set -e # exit on first error

#########
# VARs and Links
########
SVFHOME=$(pwd)
echo "SVFHOME:" $SVFHOME

LLVM_DIR=$SVFHOME/"../llvm-16.0.4.obj"
echo "LLVM_DIR:" $LLVM_DIR

Z3_URL="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-ubuntu-16.04.zip"

Z3_zip="Z3.zip"
Z3Home="z3.obj"

if [ -e "$Z3_zip" ]; then
    echo "File $Z3_zip already exists. Skipping download."
else
    echo "Downloading $Z3_zip from $Z3_URL..."
    wget "$Z3_URL" -O "$Z3_zip"

    if [ $? -ne 0 ]; then
        echo "Download Z3 failed. Exiting."
        exit 1
    fi
fi

if [ -d "$Z3Home" ]; then
    echo "Extraction directory $Z3Home already exists. Skipping extraction."
else
    echo "Extracting files from $Z3_zip to $Z3Home..."
    unzip -j "$Z3_zip" -d "$Z3Home"

    if [ $? -ne 0 ]; then
        echo "Extraction failed. Exiting."
        exit 1
    fi
fi

export Z3_DIR="$SVFHOME/$Z3Home"

echo "Z3_DIR =" $Z3_DIR

#############
# Build binutils
#############
cd ../binutils
if [ -d "build" ]; then
    echo "Build directory build already exists. Skipping build."
    cd ../
else
    mkdir ./'build'
    cd ./'build'
    ../configure --enable-gold --enable-plugins --disable-werror
    make all-gold
    cd ../../
fi

#######
# Build llvm
#######
if [ -d "llvm-16.0.4.obj" ]; then
    echo "Build directory llvm-16.0.4.obj already exists. Skipping build."
else
    mkdir ./'llvm-16.0.4.obj'
    cd ./'llvm-16.0.4.obj'
    cmake -G "Ninja" -DLLVM_ENABLE_PROJECTS="llvm;clang" -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD="host" -DLLVM_BINUTILS_INCDIR=../binutils/include ../llvm-16.0.4/llvm
    ninja -j4
    if [ "$1" = "--install" ]; then
        sudo ninja install
    else
        echo "invalid options please input --install or not"
    fi

fi

cd $SVFHOME
export LLVM_SRC="$SVFHOME/../llvm-16.0.4"
export LLVM_OBJ="$LLVM_DIR"
export LLVM_DIR="$LLVM_DIR"
export PATH=$LLVM_DIR/bin:$PATH

########
# Build SVF
########
if [[ $1 =~ ^[Dd]ebug$ ]]; then
    BUILD_TYPE='Debug'
else
    BUILD_TYPE='Release'
fi
BUILD_DIR="./${BUILD_TYPE}-build"

rm -rf "${BUILD_DIR}"
mkdir "${BUILD_DIR}"
# If you need shared libs, turn BUILD_SHARED_LIBS on
cmake -D CMAKE_BUILD_TYPE:STRING="${BUILD_TYPE}" \
    -DSVF_ENABLE_ASSERTIONS:BOOL=true            \
    -DSVF_SANITIZE="${SVF_SANITIZER}"            \
    -DBUILD_SHARED_LIBS=off                      \
    -S "${SVFHOME}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j4

echo $SVFHOME/${BUILD_TYPE}-build/bin
export PATH=$SVFHOME/${BUILD_TYPE}-build/bin:$PATH


#########
# Optionally, you can also specify a CXX_COMPILER and your $LLVM_HOME for your build
# cmake -DCMAKE_CXX_COMPILER=$LLVM_DIR/bin/clang++ -DLLVM_DIR=$LLVM_DIR
#########
