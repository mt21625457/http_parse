#!/bin/bash


export LLVM_PATH=${HOME}/usr/llvm 

export CC=${LLVM_PATH}/bin/clang
export CXX=${LLVM_PATH}/bin/clang++

LIBBASE_BASE=${HOME}/usr/xbase-libcxx

cmake -B cmake-build-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=${CXX} \
    -DCMAKE_C_COMPILER=${CC} \
    -DCMAKE_PREFIX_PATH=${LIBBASE_BASE}/lib/cmake \
    -DLIBBASE_BASE=${LIBBASE_BASE} \
    -DCMAKE_CXX_FLAGS="-stdlib=libc++" 



ninja -C cmake-build-release