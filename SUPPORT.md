# Support List

Note that XKOMP is experimental, and features claimed 'supported' may actually break.
This list provides an overview on features support: It is **non-exhaustive** and likely **not up-to-date**.
One should refer to the implementation itself to determine actual support of a specific feature.

✅ Full Support | 🧪 Experimental | ⚠️ Partial | ❌ Not Supported | ❓ Unknown

| Status | Feature | Notes / Limitations |
|:------:|:-------:|:-------------------:|
| ✅ | `target` construct | Including `nowait` and `map` clauses |
| ✅ | `taskwait` construct | Including nowait and depend clauses |
| ✅ | Data-sharing (`firstprivate`, `private`, `shared`, etc.) | Mostly managed by base compilers |
| ✅ | `barrier` construct | |
| ✅ | `omp_get_thread_num`, `omp_get_num_threads`, `omp_get_max_threads` | |
| ✅ | `omp_get_default_device`, `omp_get_num_devices`, `omp_get_initial_device`, `omp_get_device_num`, `omp_is_initial_device` | |
| ✅ | `omp_get_wtime` | |
| ✅ | `OMP_NUM_THREADS`, `OMP_THREAD_LIMIT` | |
| ✅ | `OMP_DISPLAY_ENV` | |
| 🧪 | `taskgraph` construct | The `nogroup` clause is not supported efficiently |
| ⚠️ | `task` construct | Missing the `transparent` clause |
| ⚠️ | `parallel` construct | `num_threads` clause is supported |
| ⚠️ | `for` construct | Only supporting static schedule |
| ⚠️ | `parallel for` combined-construct | Can be improved by caching XKRT teams rather than forking new |
| ⚠️ | `OMP_PROC_BIND` | `spread`, `close` | Only 1-level of nesting |
| ⚠️ | `OMP_PLACES` | with default `abstract-names` (`threads`, `cores`, `sockets`) and extensions (`L1s`, `L2s`, `L3s`, `numas`, `devices`, `machines`) |
| ❌ | `teams` | |
| ❌ | `OMP_SCHEDULE` | |
| ❌ | `OMP_DISPLAY_AFFINITY` | |
| ❌ | `taskloop` | |
| ✅ | `taskgroup` | Deep-sync of the task subtree (XKRT-native); `task_reduction`/`allocate` clauses and `cancel` not yet supported |
| ❌ | Allocators | |
| ❌ | OMPT | |
| ❌ | OMPD | |
| ❓ | Reductions | |
| ❓ | `simd` | |
| ❓ | `critical,atomic,flush,ordered` | |
