# XKOMP
An experimental OpenMP runtime-library implementation built on top of the XKaapi runtime system, and an extended LLVM's Clang ABI.

# Prerequisities
You must have an installation of
- XKRT (the XKaapi runtime system) - https://gitlab.inria.fr/xkaapi/dev-v2
- patched LLVM - https://github.com/anlsys/llvm-project and compile your openmp program with it

Example of LLVM build
```
cmake ../llvm -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_PROJECTS="clang" -DLLVM_ENABLE_RUNTIMES="openmp;offload" -DLLVM_TARGETS_TO_BUILD="NVPTX;X86" -DCMAKE_INSTALL_PREFIX=~/install/llvm/debug
```

Example of XKOMP build
```
cmake -DCMAKE_BUILD_TYPE=Debug ../
```

Example of application build
```
clang -fopenmp main.c -o main -lxkomp
```

# Bits of history
XKaapi is a dataflow programming model.
XKRT is an implementation of XKaapi with support for task dependencies over intersecting regions of memory - https://gitlab.inria.fr/xkaapi/dev-v2
This repo is an implementation of OpenMP on top of XKRT towards designing dataflow interfaces in OpenMP.

A previous implementation of OpenMP on top of XKaapi is available here:
- https://gitlab.inria.fr/openmp/kaapi
- https://gitlab.inria.fr/openmp/libkomp
