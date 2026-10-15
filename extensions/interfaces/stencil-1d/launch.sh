#!/bin/zsh

export XKRT_WARMUP=0
export XKRT_TASK_PREFETCH=0
export XKRT_VERBOSE=0
export XKRT_ALLOCATOR_CHUNK_INITIAL="50%"
export XKRT_BUSY_POLLING=0

export KMP_ENABLE_TASK_THROTTLING=0

export OMP_NUM_THREADS=16
export OMP_PLACES="cores"
export OMP_PROC_BIND=close

I=1000
n=0
GHOST=64

#bin=./llvm-60
bin=./xkomp-60
#bin=./xkomp-access

echo "bin:OMP_NUM_THREADS:perf:n:I:GHOST:ndevthreads:ngpu"

# for nstreams in 0 ; do
for iter in {1..5} ; do
    for ngpu in 1 2 4 ; do

        export XKRT_NGPUS=$ngpu
        export CUDA_VISIBLE_DEVICES=$(seq -s, 0 $[ngpu-1])
        #echo $XKRT_NGPUS
        #echo $CUDA_VISIBLE_DEVICES

        for ndevthreads in 4 ; do
            export LIBOMP_NUM_HIDDEN_HELPER_THREADS=$ndevthreads
            export XKRT_DRIVERS="host,1;cuda,$ndevthreads"
            #for j in {2..29} ; do
            for j in {27..29} ; do
                n=$[2**j]
                #echo "$bin $I $n $GHOST"
                r=$(./$bin $I $n $GHOST)
                perf=$(echo "$r" | grep "Gflop" | cut -d 'G' -f 1 | xargs)
                echo "$bin:$OMP_NUM_THREADS:$perf:$n:$I:$GHOST:$ndevthreads:$ngpu"
            done
        done
    done
done
