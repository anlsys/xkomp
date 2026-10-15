int x[4];

// May allocate a new device replica
// Does not trigger any copy (write only)
# pragma omp target loop nowait device(0) access(write: x[0:4])     // T1
for (int i = 0 ; i < 4 ; ++i)
    x[i] = i;   // 'x' is replace by its elected replica, ala map clause

// May trigger H2D copies after completion of T1
# pragma omp task access(read: x[2:2])                              // T2 depends on T1
{
    assert(x[0] == 0);  // May fail
    assert(x[1] == 1);  // May fail
    assert(x[2] == 2);  // Success
    assert(x[3] == 3);  // Success
}

# pragma omp taskwait
