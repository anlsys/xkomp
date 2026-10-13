Objective: 1-d stencil distributed across all available devices

```
// 'x' means present on the device
// '.' means not present on the device

// ON THE HOST
host = [xxxxx xxxxx xxxxx xxxxx]

// READ with 1 ghost cell
gpu0 = [xxxxx x.... ..... .....]
gpu1 = [....x xxxxx x.... .....]
gpu2 = [..... ....x xxxxx x....]
gpu3 = [..... ..... ....x xxxxx]
       —-----------------------> virtual memory

// WRITE
gpu0 = [xxxxx ..... ..... .....]
gpu1 = [..... xxxxx ..... .....]
gpu2 = [..... ..... xxxxx .....]
gpu3 = [..... ..... ..... xxxxx]
       —-----------------------> virtual memory
```

We want to generate that graph
```
GPU0 GPU1 GPU2 GPU3
    O  O  O  O         Iteration 1
    |\/|\/|\/|
    O  O  O  O         Iteration 2
     [...]
    |\/|\/|\/|
    O  O  O  O         Iteration n
     \ |  | /
         O             Read on the host
```

Shared code in both versions
```C
int      ndevices = omp_get_num_devices();
size_t       size = 1024;
size_t chunk_size = size / ndevices;
size_t      ghost = 1;
float    * domain1 = (float *) malloc(sizeof(float) * size);
float    * domain2 = (float *) malloc(sizeof(float) * size);
```


