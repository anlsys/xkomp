int x[4];

for (int i = 0 ; i < 4 ; ++i)
{
    # pragma omp target nowait device (i) access(write: x[i:1]) // T0, T1, T2, T3
        x[i] = i;
}

// May trigger 4x D2H
# pragma omp task access(read: x[0:4])                          // T4 depends on T0, T1, T2, T3
{
    assert(x[0] == 0);  // Success
    assert(x[1] == 1);  // Success
    assert(x[2] == 2);  // Success
    assert(x[3] == 3);  // Success
}
