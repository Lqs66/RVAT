#!/usr/bin/env bash
# type './build.sh'       for release build
# type './build.sh debug' for debug build
# if the LLVM_DIR variable is not set, LLVM will be downloaded.
#
# Dependencies include: build-essential libncurses5 libncurses-dev cmake zlib1g-dev
set -e # exit on first error

GENERATOR=""
if command -v ninja &> /dev/null
then
    echo "Ninja is installed, using Ninja generator"
    GENERATOR="Ninja"
else
    echo "Ninja is not installed, using default generator"
    GENERATOR="Unix Makefiles"
fi


#########
# VARs and Links
########
LIBS_HOME=$(pwd)
echo "LIBS_HOME:" $LIBS_HOME

LLVM_BIN_DIR=$LIBS_HOME/"../llvm-16.0.4.obj"
echo "LLVM_BIN_DIR:" $LLVM_BIN_DIR

# #############
# # Build binutils
# #############
# echo "Building binutils..."
# cd $LIBS_HOME/binutils
# if [ -d "build" ] && [ -f "build/.binutils" ]; then
#     echo "binutils has been built. Skipping build."
# else
#     rm -rf ./'build'
#     mkdir ./'build'
#     cd ./'build'
#     ../configure --enable-gold --enable-plugins --disable-werror
#     make all-gold
#     touch .binutils
# fi

#######
# Build llvm
#######
echo "Building llvm for x64 and aarch64..."
cd $LIBS_HOME/llvm-16.0.4
if [ -d "build" ] && [ -f "build/.llvm16" ]; then
    echo "llvm aarch64 has been built. Skipping build."
else
    rm -rf ./'build'
    mkdir ./'build'
    cd ./'build'
    cmake -G "$GENERATOR" \
      -DLLVM_ENABLE_PROJECTS="clang;lld;compiler-rt" \
      -DLLVM_TARGETS_TO_BUILD="X86;AArch64" \
      -DLLVM_DEFAULT_TARGET_TRIPLE="x86_64-unknown-linux-gnu" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DLLVM_ENABLE_RTTI=ON \
      -DLLVM_ENABLE_FFI=ON \
      -DLLVM_ENABLE_LIBXML2=OFF \
      -DLLVM_ENABLE_ASSERTIONS=OFF \
      ../llvm
    cmake --build . -- -j4
    touch .llvm16
fi

#############
# SVF denpends Z3
#############
echo "Building Z3..."
cd $LIBS_HOME/SVF-3.1
Z3_URL="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-ubuntu-16.04.zip"
CHECKSUM="6534f26427ee4f02835d17c3472f5ce750f34b4898c35cdd4223459b3589664e"
Z3_zip="Z3.zip"
Z3Home="z3.obj"
# download and extract Z3
if [ -f "$Z3_zip" ]; then
    echo "Checksumming $Z3_zip..."
    if [ "$(sha256sum $Z3_zip | awk '{print $1}')" != "$CHECKSUM" ]; then
        echo "Checksum failed. Downloading $Z3_zip from $Z3_URL..."
        wget "$Z3_URL" -O "$Z3_zip"
    else
        echo "$Z3_zip is up to date. Skipping download."
    fi
else
    echo "Downloading $Z3_zip from $Z3_URL..."
    wget "$Z3_URL" -O "$Z3_zip"
fi

if [ $? -ne 0 ]; then
    echo "Download Z3 failed. Exiting."
    exit 1
fi
if [ -d "$Z3Home" ] && [ -f "Z3Home/.z3" ]; then
    echo "Extraction directory $Z3Home already exists. Skipping extraction."
else
    rm -rf "$Z3Home"
    echo "Extracting files from $Z3_zip to $Z3Home..."
    unzip -j "$Z3_zip" -d "$Z3Home"

    if [ $? -ne 0 ]; then
        echo "Extraction failed. Exiting."
        exit 1
    fi
    touch $Z3Home/.z3
fi
export Z3_DIR="$LIBS_HOME/SVF-3.1/$Z3Home"
echo "Z3_DIR =" $Z3_DIR

export LLVM_SRC="$LIBS_HOME/llvm-16.0.4"
# export LLVM_OBJ="$LLVM_DIR"
export LLVM_DIR="$LLVM_DIR"
export PATH=$LLVM_DIR/bin:$PATH

########
# Build SVF
########
echo "Building SVF..."
cd $LIBS_HOME/SVF-3.1
if [[ $1 =~ ^[Dd]ebug$ ]]; then
    BUILD_TYPE='Debug'
else
    BUILD_TYPE='Release'
fi
BUILD_DIR="./${BUILD_TYPE}-build"
if [ -d "${BUILD_DIR}" ]&& [ -f "${BUILD_DIR}/.svf" ]; then
    echo "SVF has been built. Skipping build."
else
    rm -rf "${BUILD_DIR}"
    mkdir "${BUILD_DIR}"
    # If you need shared libs, turn BUILD_SHARED_LIBS on
    cmake -G "$GENERATOR"                            \
        -D CMAKE_BUILD_TYPE:STRING="${BUILD_TYPE}"   \
        -DSVF_ENABLE_ASSERTIONS:BOOL=true            \
        -DSVF_SANITIZE="${SVF_SANITIZER}"            \
        -DBUILD_SHARED_LIBS=off                      \
        -S "${LIBS_HOME}/SVF-3.1" -B "${BUILD_DIR}"
    cmake --build "${BUILD_DIR}" -j4
    touch "${BUILD_DIR}/.svf"
fi

# echo $LIBS_HOME/SVF-3.1/${BUILD_TYPE}-build/bin
export PATH=$LIBS_HOME/SVF-3.1/${BUILD_TYPE}-build/bin:$PATH

# ########
# # Build KLEE
# ########
echo "Building KLEE..."
cd $LIBS_HOME/KLEE
# first build stp
cd stp
if [ -d "build" ] && [ -f "build/.stp" ]; then
    echo "STP has been built. Skipping build."
    cd ..
else
    rm -rf ./'build'
    mkdir ./'build'
    cd ./'build'
    cmake -G "$GENERATOR" ..
    cmake --build . -- -j4
    touch .stp
    cd ../..
fi

# second build cvc5
cd cvc5-1.3.0
if [ -d "cvc5_install" ] && [ -f "cvc5_install/.cvc5" ]; then
    echo "CVC5 has been built. Skipping build."
    cd ..
else
    ./configure.sh --prefix=./cvc5_install/ --auto-download
    cd build
    make -j4
    make install
    touch .cvc5
    cd ../..
fi
# third build klee-uclibc
cd klee-uclibc
./configure --make-llvm-lib --with-cc=${LLVM_ROOT}/build/bin/clang --with-llvm-config=${LLVM_ROOT}/build/bin/llvm-config
make -j4
cd ..
# final build klee
cd klee
LLVM_VERSION=16 BASE=${KLEE_ROOT} ENABLE_OPTIMIZED=1 DISABLE_ASSERTIONS=1 ENABLE_DEBUG=0 REQUIRES_RTTI=1 scripts/build/build.sh libcxx
mkdir build
cd build
cmake -DENABLE_SOLVER_STP=ON \
      -DENABLE_SOLVER_Z3=ON \
      -DENABLE_SOLVER_CVC5=ON \
      -DZ3_ROOT=${Z3_ROOT} \
      -Dcvc5_DIR=${CVC5_DIR} \
      -DENABLE_POSIX_RUNTIME=ON \
      -DKLEE_UCLIBC_PATH=${KLEE_ROOT}/klee-uclibc \
      -DKLEE_LIBCXX_DIR=${KLEE_ROOT}/libc++-install-160/ \
      -DKLEE_LIBCXX_INCLUDE_DIR=${KLEE_ROOT}/libc++-install-160/include/c++/v1/ \
      -DLLVM_DIR=${KLEE_ROOT}/llvm-160-install_O_ND_NA_RTTI \
      -DENABLE_KLEE_LIBCXX=ON \
      -DENABLE_FLOATING_POINT=ON \
      -DENABLE_FP_RUNTIME=ON \
      ..
make -j4

# ########
# # Build yaml-cpp
# ########
# echo "Building yaml-cpp..."
# cd $LIBS_HOME/yaml-cpp
# if [ -d "build" ] && [ -f "build/.yamlcpp" ]; then
#     echo "Build directory already exists. Skipping build."
# else
#     rm -rf ./'build'
#     mkdir ./'build'
#     cd ./'build'
#     cmake -G "$GENERATOR" -DYAML_BUILD_SHARED_LIBS=ON ..
#     cmake --build . -- -j4
#     touch .yamlcpp
# fi

########
# install UPPAAL
########
echo "Installing UPPAAL..."
cd $LIBS_HOME
if [ ! -f uppaal-5.0.0-linux64.zip ]; then
    wget https://download.uppaal.org/uppaal-5.0/uppaal-5.0.0/uppaal-5.0.0-linux64.zip
    unzip uppaal-5.0.0-linux64.zip
fi

# ########
# # Build boost_1_74_0
# ########
# echo "Building boost_1_74_0..."
# cd $LIBS_HOME/boost_1_74_0
# if [ -d "boost_install" ]; then
#     echo "Boost has been built. Skipping build."
# else
#     rm -rf ./'boost_install'
#     ./bootstrap.sh --prefix=./boost_install
#     ./b2 install
# fi
