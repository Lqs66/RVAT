#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "====================================="
echo "Installing all module dependencies"
echo "====================================="

# Create necessary directories
mkdir -p flight-control
mkdir -p dependencies/KLEE

# ===================================
# 1. Install ArduCopter-4.4
# ===================================
echo ""
echo "[1/8] Installing ArduCopter-4.4..."
if [ -d "flight-control/arducopter-4.4" ]; then
    echo "ArduCopter-4.4 already exists, skipping..."
else
    echo "Cloning ArduPilot Copter-4.4..."
    git clone https://github.com/ArduPilot/ardupilot.git -b Copter-4.4 --depth 1 flight-control/arducopter-4.4
    cd flight-control/arducopter-4.4
    echo "Initializing submodules for ArduPilot..."
    git submodule update --init --recursive --depth 1
    echo "Applying ArduPilot patch..."
    git apply ../../git_patchs/ardupilot.patch
    cd "$SCRIPT_DIR"
    echo "ArduCopter-4.4 installed successfully"
fi

# ===================================
# 2. Install PX4-1.15.2
# ===================================
echo ""
echo "[2/8] Installing PX4-1.15.2..."
if [ -d "flight-control/PX4-1.15.2" ]; then
    echo "PX4-1.15.2 already exists, skipping..."
else
    echo "Cloning PX4-Autopilot v1.15.2..."
    git clone https://github.com/PX4/PX4-Autopilot.git -b v1.15.2 --depth 1 flight-control/PX4-1.15.2
    cd flight-control/PX4-1.15.2
    echo "Initializing submodules for PX4..."
    git submodule update --init --recursive --depth 1
    echo "Applying PX4 patch..."
    git apply ../../git_patchs/px4.patch
    cd "$SCRIPT_DIR"
    echo "PX4-1.15.2 installed successfully"
fi

# ===================================
# 3. Install CVC5-1.3.0
# ===================================
echo ""
echo "[3/8] Installing CVC5-1.3.0..."
if [ -d "dependencies/KLEE/cvc5-1.3.0" ]; then
    echo "CVC5-1.3.0 already exists, skipping..."
else
    echo "Cloning CVC5..."
    git clone https://github.com/cvc5/cvc5.git -b cvc5-1.3.0 --depth 1 dependencies/KLEE/cvc5-1.3.0
    cd "$SCRIPT_DIR"
    echo "CVC5-1.3.0 installed successfully"
fi

# ===================================
# 4. Install STP
# ===================================
echo ""
echo "[4/8] Installing STP..."
if [ -d "dependencies/KLEE/stp" ]; then
    echo "STP already exists, skipping..."
else
    echo "Cloning STP..."
    git clone https://github.com/stp/stp.git -b 2.3.3 --depth 1 dependencies/KLEE/stp
    cd "$SCRIPT_DIR"
    echo "STP installed successfully"
fi

# ===================================
# 5. Install KLEE-uClibc
# ===================================
echo ""
echo "[5/8] Installing KLEE-uClibc..."
if [ -d "dependencies/KLEE/klee-uclibc" ]; then
    echo "KLEE-uClibc already exists, skipping..."
else
    echo "Cloning KLEE-uClibc..."
    git clone https://github.com/klee/klee-uclibc.git -b klee_0_9_29 --depth 1 dependencies/KLEE/klee-uclibc
    cd "$SCRIPT_DIR"
    echo "KLEE-uClibc installed successfully"
fi

# ===================================
# 6. Install KLEE 3.1
# ===================================
echo ""
echo "[6/8] Installing KLEE 3.1..."
if [ -d "dependencies/KLEE/klee" ]; then
    echo "KLEE 3.1 already exists, skipping..."
else
    echo "Cloning KLEE 3.1..."
    git clone https://github.com/klee/klee.git -b v3.1 --depth 1 dependencies/KLEE/klee
    cd dependencies/KLEE/klee
    echo "Applying KLEE patch..."
    git apply ../../../git_patchs/klee.patch
    cd "$SCRIPT_DIR"
    echo "KLEE 3.1 installed successfully"
fi

# ===================================
# 7. Install LLVM 16.0.4
# ===================================
echo ""
echo "[7/8] Installing LLVM 16.0.4..."
if [ -d "dependencies/llvm-16.0.4" ]; then
    echo "LLVM 16.0.4 already exists, skipping..."
else
    echo "Cloning LLVM 16.0.4..."
    git clone https://github.com/llvm/llvm-project.git -b llvmorg-16.0.4 --depth 1 dependencies/llvm-16.0.4
    cd dependencies/llvm-16.0.4
    echo "Applying LLVM patch..."
    git apply ../../git_patchs/llvm.patch
    cd "$SCRIPT_DIR"
    echo "LLVM 16.0.4 installed successfully"
fi

# ===================================
# 8. Install PGFuzz
# ===================================
echo ""
echo "[8/8] Installing PGFuzz..."
if [ -d "PGFuzz" ]; then
    echo "PGFuzz already exists, skipping..."
else
    echo "Cloning PGFuzz..."
    git clone https://github.com/purseclab/PGFuzz.git
    cd PGFuzz
    echo "Checking out commit 7eaebf2..."
    git checkout 7eaebf2
    echo "Applying PGFuzz patch..."
    git apply ../../git_patchs/pgfuzz.patch
    cd "$SCRIPT_DIR"
    echo "PGFuzz installed successfully"
fi

echo ""
echo "====================================="
echo "All modules installed successfully!"
echo "====================================="
echo ""
echo "Installed modules:"
echo "  - flight-control/arducopter-4.4 (ArduPilot Copter-4.4 with patches)"
echo "  - flight-control/PX4-1.15.2 (PX4-Autopilot v1.15.2 with patches)"
echo "  - dependencies/KLEE/cvc5-1.3.0 (CVC5 1.3.0)"
echo "  - dependencies/KLEE/stp (STP 2.3.3)"
echo "  - dependencies/KLEE/klee-uclibc (klee_0_9_29)"
echo "  - dependencies/KLEE/klee (KLEE 3.1 with patches)"
echo "  - dependencies/llvm-16.0.4 (LLVM 16.0.4 with patches)"
echo "  - PGFuzz (commit 7eaebf2)"
echo ""
