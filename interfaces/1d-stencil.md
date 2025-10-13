Objective: 1-d stencil distributed across all available devices

```
// 'x' means present on the device
// '.' means not present on the device
host = [xxxxx xxxxx xxxxx xxxxx]
gpu0 = [xxxxx x.... ..... .....]
gpu1 = [....x xxxxx x.... .....]
gpu2 = [..... ....x xxxxx x....]
gpu3 = [..... ..... ....x xxxxx]
       —-----------------------> memory domain
```

// We want to generate that graph

    O O O O         Iteration 1
    |/|/\ |
    O O O O         Iteration 2
     [...]
    |/|/\ |
    O O O O         Iteration n
    \ \ / /
       O            Read on the host

Shared code in both versions
```C
int      ndevices = omp_get_num_devices();
size_t       size = 1024;
size_t chunk_size = size / ndevices;
size_t      ghost = 1;
float    * domain = (float *) malloc(sizeof(float) * size);
```

With current map+depend
```C
char virtual_deps[ndevices+2];  // ugly VLA, whatever

for (int i = 0 ; i < ndevices ; ++i)
{
    size_t x = MAX(0,    (i+0)*chunk_size - ghost);
    size_t y = MIN(size, (i+1)*chunk_size + ghost);
    # pragma omp enter data map(storage: domain[x:y-x]) device(i)                         // allocate each replica

    # pragma omp target update nowait depend(out: virtual_deps[i+1]) to(device[x:y-x])  // H2D each replica
}

for (int iter = 0 ; iter < niter)
{
    for (int i = 0 ; i < ndevices ; ++i)
    {
        size_t x = MAX(0,    (i+0)*chunk_size - ghost);
        size_t y = MIN(size, (i+1)*chunk_size + ghost);
        # pragma omp target nowait device(i)                \
            depend(in:  virtual_deps[i+0])                  \
            depend(out: virtual_deps[i+1])                  \
            depend(in:  virtual_deps[i+2])
            stencil(domain, x, y, i, chunk_size);

        // forward ghost cells to neighbors
        for (int j = i - 1 ; j < i + 1 ; ++j)
        {
            if (i == j || j < 0 || j >= ndevices)
                continue ;

            # pragma omp target update nowait depend(out: virtual_deps[i+1])
            {
                void * src = [...]; // TODO: retrieve device pointer of device 'i' (How to ?)
                void * dst = [...]; // TODO: retrieve device pointer of device 'j' (How to ?)
                size_t len = ghost;
                size_t src_offset = (j==i-1) ?                  0 : chunk_size + ghost; // forward 'left'
                size_t dst_offset = (j==i-1) ? chunk_size + ghost :                  0; // forward 'right'
                int src_device = i;
                int dst_device = j;

                // almost certain that we loose a thread here in some cuStreamSynchronize-alike calls
                omp_target_memcpy(dst, src, len, dst_offset, src_offset, dst_device, src_device);
            }
        }
    }
}

// D2H
for (int i = 0 ; i < ndevices ; ++i)
{
    size_t x = MAX(0,    (i+0)*chunk_size - ghost);
    size_t y = MIN(size, (i+1)*chunk_size + ghost);
    # pragma omp target nowait depend(out: virtual_deps[i+1]) device(i) from(domain[x:y-x])
}

// depend on all previous D2H
# pragma omp target nowait device(omp_get_initial_device())
    depend(iterator(i=0:ndevices), in: virtual_deps[i+1])
    puts("Domain is now coherent on the host");

for (int i = 0 ; i < ndevices ; ++i)
{
    size_t x = MAX(0,    (i+0)*chunk_size - ghost);
    size_t y = MIN(size, (i+1)*chunk_size + ghost);
    # pragma omp exit data map(storage: domain[x:y-x]) device(i)
}
```


With an 'access' clause
```C
for (int iter = 0 ; iter < niter)
{
    for (int i = 0 ; i < omp_get_num_devices() ; ++i)
    {
        size_t x = MAX(0,    (i+0)*chunk_size - ghost);
        size_t y = MIN(size, (i+1)*chunk_size + ghost);
        # pragma omp target nowait device(i) access(readwrite: segment(domain + x, domain + y)))
            stencil(domain, x, y, i, chunk_size);
    }
}
# pragma omp target nowait device(omp_get_initial_device()) access(read: segment(domain, size))
    puts("Domain is now coherent on the host");

// TODO: some pragma to release device replicas (in xkblas, xkblas_invalidate_caches)
```
