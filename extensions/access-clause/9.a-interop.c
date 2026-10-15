int x[4];

for (int i = 0 ; i < 4 ; ++i)
{
    # pragma omp target nowait access(write: x[i:1]) device(i)  // T0, T1, T2, T3
        x[i] = i;
}

const void * base_ptr   = (const void *) x;         // Base pointer
const ssize_t size      = sizeof(x);                // Size in bytes
const bool coherent     = 1;                        // Must be coherent
const int device_id     = omp_get_initial_device(); // The host device (could have been any other device)

// The routine may block until D2H copies completed
// It increments an internal ref counter to prohibit eviction by the implementation
int * ptr = (int *) omp_acquire_access_replica(base_ptr, size, coherent, device);

assert(ptr[0] == 0);    // Success
assert(ptr[1] == 1);    // Success
assert(ptr[2] == 2);    // Success
assert(ptr[3] == 3);    // Success

MPI_Send(ptr, 4, MPI_INT, dst, tag, MPI_COMM_WORLD);

// Decrease ref counter on the replica 'ptr' ; so the implementation may evict
omp_release_access_replica(base_ptr, ptr, size, device);
