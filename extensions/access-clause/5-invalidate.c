int x[4];

for (int i = 0 ; i < 4 ; ++i)
{
    # pragma omp target nowait device (i) access(write: x[i:1]) // T0, T1, T2, T3
        x[i] = i;
}

# pragma omp taskwait nowait access(invalidate: x[1:1])         // T4 depends on T1

// May trigger 3 D2H copies
# pragma omp task access(read: x[0:4])                          // T5 depends on T0, T1, T2, T3, and T4
{
    assert(x[0] == 0);  // Success
    assert(x[1] == 1);  // May fail
    assert(x[2] == 2);  // Success
    assert(x[3] == 3);  // Success
}
