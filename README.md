# XKOMP
An experimental OpenMP runtime-library implementation built on top of the XKaapi runtime system, and an extended LLVM's Clang ABI.

# Prerequisities
You must have an installation of
- [XKRT](https://github.com/rpereira-dev/xkrt)
- [patched LLVM](https://github.com/anlsys/llvm-project) - and compile your openmp program with it

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
XKRT is a fork of XKaapi, that it extended with support for task dependencies over intersecting regions of memory (https://gitlab.inria.fr/xkaapi/dev-v2)
This repo is an abstraction layer to OpenMP through LLVM and GCC ABIs.
It partial support of OpenMP 6.0 on taskgraphs (6.0) and original extensions for multi-devices dataflow.

If you are looking for the old version of kaapi/komp, refer to:
- https://gitlab.inria.fr/openmp/kaapi
- https://gitlab.inria.fr/openmp/libkomp

# References
[1] XKRT: a Runtime System for Macro-Dataflow Programming on Multi-Devices Architectures. DOI: 10.2139/ssrn.6460634
