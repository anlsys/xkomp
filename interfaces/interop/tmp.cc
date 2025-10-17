omp_depend_t deps[] = {
   {OMP_IN,  &x, OBJECT},
   {OMP_OUT, &y, OBJECT}
};

// async(deps)      _kmp_task_deps_hash_entry_deps

depobj

# pragma depend(in: y)
    {}
