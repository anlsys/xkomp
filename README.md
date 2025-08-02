# XKOMP
An experimental OpenMP runtime-library implementation built on top of the XKaapi runtime system, and an extended LLVM's Clang ABI.

# Prerequisities
You must have an installation of XKRT (the XKaapi runtime system) - https://gitlab.inria.fr/xkaapi/dev-v2

# Example build
```
rm -rf CMakeCache.txt CMakeFiles/ && CC=icx CXX=icpx cmake -DCMAKE_BUILD_TYPE=Debug ../
```

# Running standard OpenMP programs
If you want to run OpenMP programs, you must have a patched version of LLVM
Compile your openmp program with it, and link against libomp.so with libxkomp.so

If you want to run OpenMP programs with targets, you need to enable the `offload` projects of LLVM.
The `offload` project depends on XKRT and XKOMP, so you must have a valid installation of both first.

https://github.com/anlsys/llvm-project

Example build of llvm
```
CC=clang CXX=clang++ cmake ../llvm -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_PROJECTS="clang" -DLLVM_ENABLE_RUNTIMES="openmp;offload" -DLLVM_TARGETS_TO_BUILD="NVPTX;X86" -DCMAKE_INSTALL_PREFIX=/vast/users/rpereira/install/llvm/Debug
```

Example of OpenMP program build
```
clang -fopenmp main.c -o main -lxkomp
```
