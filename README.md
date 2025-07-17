# XKOMP
An experimental OpenMP runtime-library implementation built on top of the XKaapi runtime system, relying on an extended LLVM's Clang ABI.

# Prerequisities
Must have a patched version of LLVM - https://github.com/anlsys/llvm-project and compile your openmp program with it
Example build of llvm
```
CC=clang CXX=clang++ cmake ../llvm -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_PROJECTS="clang" -DLLVM_ENABLE_RUNTIMES="openmp;offload" -DLLVM_TARGETS_TO_BUILD="NVPTX;X86" -DCMAKE_INSTALL_PREFIX=/vast/users/rpereira/install/llvm/Debug
```

# Example build
```
rm -rf CMakeCache.txt CMakeFiles/ && CC=icx CXX=icpx cmake -DCMAKE_BUILD_TYPE=Debug ../
```
