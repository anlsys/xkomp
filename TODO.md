# TODO list
1. access clause: add missing front-end in Clang and unit-tests for each clause/modifier.
1. taskgraph construct: implement front-end in Clang.
1. taskgraph conditional tasks: design and implement conditional tasks; so that they can be mapped to CUDA conditional nodes.
1. implement a coding agent to rebase weekly on llvm-project/llvm.
1. ala OmpSs - taskiter construct (a "taskgraph" executed 'n' times" -- so that it can be mapped to CUDA conditional nodes).
1. ala OmpSs - depend on taskloop
1. transparent tasks: implement in XKRT
1. large team of threads: currently XKRT uses 1 futex per team of thread. Waking up a team leads to a O(n) syscall -- with "n" the number of threads. While it is fine for small teams (1-16 threads) it is not optimal for large teams (>16 threads). Instead: use multiple futexes and wakeup in parallel
1. KMP ABI: verify and fix compatibility
1. GOMP ABI: implement it
1. OMPT: add support for it. Likely, add a tooling interface in XKRT, and implement OMPT on top of it
