# 编译脚本，在使用前需要先编译SVF
MAKEFLAGS += --no-print-directory

ODIR := ./build
CONFIG := Release
output_folder := $(shell mkdir -p $(ODIR))

# set SVF env
PROJECTHOME := $(shell pwd)
SVFHOME := $(PROJECTHOME)/../dependencies/SVF-3.1
LLVM_DIR := $(SVFHOME)/../llvm-16.0.4/build
Z3Home := z3.obj

export Z3_DIR := $(SVFHOME)/$(Z3Home)
export LLVM_SRC := $(SVFHOME)/../llvm-16.0.4
export LLVM_OBJ := $(LLVM_DIR)
export LLVM_DIR := $(LLVM_DIR)
export PATH := $(LLVM_DIR)/bin:$(PATH)
export SVF_DIR := $(SVFHOME)


all: libplugin.so

libplugin.so: configure
	@echo Building...
	@$(MAKE) -C $(ODIR)

configure:
ifeq ($(CONFIG), Debug)
	@echo Configuring for Debug...
	@cd $(ODIR) && cmake -DCMAKE_BUILD_TYPE=Debug ..
else
	@echo Configuring for Release...
	@cd $(ODIR) && cmake ..
endif

debug:
	$(MAKE) CONFIG=Debug all

clean:
	@echo Cleaning up...
	@rm -rf $(ODIR)

