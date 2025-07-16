# XKOMP
An OpenMP runtime-library implementation built on top of the XKaapi runtime system, with support for LLVM's Clang ABI.

# Prerequisities
Must have a patched version of LLVM - https://github.com/rpereira-dev/llvm-project
and compile test manually

# Example build
```
rm -rf CMakeCache.txt CMakeFiles/ && CC=icx CXX=icpx cmake -DCMAKE_BUILD_TYPE=Debug ../
```


