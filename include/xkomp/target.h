#ifndef __TARGET_H__
# define __TARGET_H__

# include <map>
# include <vector>

struct EntryTy {
    void *Address;       // Pointer to the function
    char *name;       // Name of the function
    size_t size;      // 0 for function
    int32_t flags;    // OpenMPOffloadingDeclareTargetFlags::OMP_DECLARE_TARGET_FPTR
    int32_t reserved; // Reserved
};

/// This struct is a record of the device image information
struct __tgt_device_image {
    void *ImageStart; // Pointer to the target code start
    void *ImageEnd;   // Pointer to the target code end
    EntryTy *EntriesBegin; // Begin of table with all target entries
    EntryTy *EntriesEnd; // End of table (non inclusive)
};

struct __tgt_device_info {
    void *Context = nullptr;
    void *Device = nullptr;
};

/// This struct is a record of all the host code that may be offloaded to a
/// target.
struct __tgt_bin_desc {
    int32_t NumDeviceImages;          // Number of device types supported
    __tgt_device_image *DeviceImages; // Array of device images (1 per dev. type)
    EntryTy *HostEntriesBegin; // Begin of table with all host entries
    EntryTy *HostEntriesEnd; // End of table (non inclusive)
};

/// This struct contains the offload entries identified by the target runtime
struct __tgt_target_table {
    EntryTy *EntriesBegin; // Begin of the table with all the entries
    EntryTy *EntriesEnd; // End of the table with all the entries (non inclusive)
};

/// This struct contains a handle to a loaded binary in the plugin device.
struct __tgt_device_binary {
    uintptr_t handle;
};


/// This struct contains all of the arguments to a target kernel region launch.
struct KernelArgsTy {
    uint32_t Version = 0; // Version of this struct for ABI compatibility.
    uint32_t NumArgs = 0; // Number of arguments in each input pointer.
    void **ArgBasePtrs = nullptr; // Base pointer of each argument (e.g. a struct).
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

/// return flags of __tgt_target_XXX public APIs
enum __tgt_target_return_t : int {
    /// successful offload executed on a target device
    OMP_TGT_SUCCESS = 0,
    /// offload may not execute on the requested target device
    /// this scenario can be caused by the device not available or unsupported
    /// as described in the Execution Model in the specification
    /// this status may not be used for target device execution failure
    /// which should be handled internally in libomptarget
    OMP_TGT_FAIL = ~0
};

/// Map between the host entry begin and the translation table. Each
/// registered library gets one TranslationTable. Use the map from
/// llvm::offloading::EntryTy so that we may quickly determine whether we
/// are trying to (re)register an existing lib or really have a new one.
struct TranslationTable {
    __tgt_target_table HostTable;
    std::vector<__tgt_target_table> DeviceTables;

    // Image assigned to a given device.
    std::vector<__tgt_device_image *> TargetsImages; // One image per device ID.

    // Arrays of entries active on the device.
    std::vector<std::vector<EntryTy>> TargetsEntries; // One table per device ID.

    // Table of entry points or NULL if it was not already computed.
    std::vector<__tgt_target_table *>
        TargetsTable; // One table per device ID.
};
typedef std::map<EntryTy *, TranslationTable> HostEntriesBeginToTransTableTy;

/// Map between the host ptr and a table index
struct TableMap {
    TranslationTable *Table = nullptr; // table associated with the host ptr.
    uint32_t Index = 0; // index in which the host ptr translated entry is found.
    TableMap() = default;
    TableMap(TranslationTable *Table, uint32_t Index)
        : Table(Table), Index(Index) {}
};
typedef std::map<void *, TableMap> HostPtrToTableMapTy;

# define OFFLOAD_SUCCESS    0
# define OFFLOAD_FAIL       1

#endif /* __TARGET_H__ */
