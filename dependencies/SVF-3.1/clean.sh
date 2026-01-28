#!/bin/bash

SVFHOME=$(pwd)

echo delete Z3
rm -rf z3.obj
rm Z3.zip

for i in `find . -name 'Debug*'`
do
echo delete $i
rm -rf $i
done
for i in `find . -name 'Release*'`
do
echo delete $i
rm -rf $i
done

cd $SVFHOME/../
rm -rf llvm-16.0.4.obj
cd binutils
rm -rf build