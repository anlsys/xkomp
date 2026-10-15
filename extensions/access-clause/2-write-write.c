int x[4];

# pragma omp target loop nowait access(write: x[0:4])   // T1
for (int i = 0 ; i < 4 ; ++i)
    x[i] = i;

// Does not trigger D2H copies (write only)
# pragma omp task access(write: x[2:2])                 // T2 depends on T1
{
    assert(x[0] == 0);  // May fail
    assert(x[1] == 1);  // May fail
    assert(x[2] == 2);  // May fail
    assert(x[3] == 3);  // May fail
}

# pragma omp taskwait
