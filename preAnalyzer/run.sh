#!/bin/bash

# 检查 build 文件夹是否存在
if [ ! -d "build" ]; then
  echo "Error: 'build' directory does not exist in the current folder."
  exit 1
fi

# 进入 build 文件夹
cd build

echo "start executing inCallAnalyzer..."

# 执行 ./indCallAnalysis -a
if [ -x "./indCallAnalysis" ]; then
  ./indCallAnalysis -v
else
  echo "Error: './indCallAnalysis' does not exist or is not executable."
  exit 1
fi

if [ -z "$DTMC" ]; then
  echo "Error: DTMC environment variable is not set."
  exit 1
fi

targetIR=$DTMC/verifyDataBase/ir_and_elf/arducopter_sitl_pi64.ll

echo "start executing addTargetsMD..."

# 执行 ./addTargetsMD
if [ -x "./addTargetsMD" ]; then
  ./addTargetsMD $targetIR --config $DTMC/inCallAnalyzer/build/indirectCalls.yml
else
  echo "Error: './addTargetsMD' does not exist or is not executable."
  exit 1
fi