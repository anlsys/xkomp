#ifndef __TARGET_H__
# define __TARGET_H__

struct __tgt_bin_desc
{
    // TODO
};

/// This struct contains all of the arguments to a target kernel region launch.
struct KernelArgsTy {
    uint32_t Version = 0; // Version of this struct for ABI compatibility.
    uint32_t NumArgs = 0; // Number of arguments in each input pointer.
    void **ArgBasePtrs =
        nullptr;                 // Base pointer of each argument (e.g. a struct).
    void **ArgPtrs = nullptr;    // Pointer to the argument data.
    int64_t *ArgSizes = nullptr; // Size of the argument data in bytes.
    int64_t *ArgTypes = nullptr; // Type of the data (e.g. to / from).
    void **ArgNames = nullptr;   // Name of the data for debugging, possibly null.
    void **ArgMappers = nullptr; // User-defined mappers, possibly null.
    uint64_t Tripcount =
        0; // Tripcount for the teams / distribute loop, 0 otherwise.
    struct {
        uint64_t NoWait : 1; // Was this kernel spawned with a `nowait` clause.
        uint64_t IsCUDA : 1; // Was this kernel spawned via CUDA.
        uint64_t Unused : 62;
    } Flags = {0, 0, 0};
    // The number of teams (for x,y,z dimension).
    uint32_t NumTeams[3] = {0, 0, 0};
    // The number of threads (for x,y,z dimension).
    uint32_t ThreadLimit[3] = {0, 0, 0};
    uint32_t DynCGroupMem = 0; // Amount of dynamic cgroup memory requested.
};
static_assert(sizeof(KernelArgsTy().Flags) == sizeof(uint64_t),
        "Invalid struct size");
static_assert(sizeof(KernelArgsTy) ==
        (8 * sizeof(int32_t) + 3 * sizeof(int64_t) +
         4 * sizeof(void **) + 2 * sizeof(int64_t *)),
        "Invalid struct size");

using map_var_info_t = void *;

#endif /* __TARGET_H__ */
