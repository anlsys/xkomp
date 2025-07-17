# include <xkomp/kmp.h>
# include <xkomp/target.h>
# include <xkomp/xkomp.h>

# include <xkrt/logger/logger.h>
# include <xkrt/xkrt.h>

/////////////////
// Run kernels //
/////////////////

/// Find the table information in the map or look it up in the translation
/// tables.
TableMap *
getTableMap(xkomp_t * xkomp, void * HostPtr)
{
    std::lock_guard<std::mutex> TblMapLock(xkomp->TblMapMtx);
    HostPtrToTableMapTy::iterator TableMapIt = xkomp->HostPtrToTableMap.find(HostPtr);

    if (TableMapIt != xkomp->HostPtrToTableMap.end())
        return &TableMapIt->second;

    // We don't have a map. So search all the registered libraries.
    TableMap *TM = nullptr;
    std::lock_guard<std::mutex> TrlTblLock(xkomp->TrlTblMtx);
    for (HostEntriesBeginToTransTableTy::iterator Itr = xkomp->HostEntriesBeginToTransTable.begin();
            Itr != xkomp->HostEntriesBeginToTransTable.end(); ++Itr) {
        // get the translation table (which contains all the good info).
        TranslationTable *TransTable = &Itr->second;
        // iterate over all the host table entries to see if we can locate the
        // host_ptr.
        EntryTy *Cur = TransTable->HostTable.EntriesBegin;
        for (uint32_t I = 0; Cur < TransTable->HostTable.EntriesEnd; ++Cur, ++I) {
            if (Cur->Address != HostPtr)
                continue;
            // we got a match, now fill the HostPtrToTableMap so that we
            // may avoid this search next time.
            TM = &(xkomp->HostPtrToTableMap)[HostPtr];
            TM->Table = TransTable;
            TM->Index = I;
            return TM;
        }
    }
    return nullptr;
}

static inline int
target(
    ident_t * loc,
    int64_t DeviceId,
    int32_t NumTeams,
    int32_t ThreadLimit,
    void *HostPtr,
    KernelArgsTy *KernelArgs
) {
    // retrieve xkaapi device
    xkrt_device_global_id_t device_global_id = DeviceId + 1;

    // retrieve device function pointer
    xkomp_t * xkomp = xkomp_get();

    TableMap * TM = getTableMap(xkomp, HostPtr);
    if (TM == NULL)
        LOGGER_FATAL("No target pointer found");

    # if 0
    // No map for this host pointer found!
    if (!TM) {
        REPORT("Host ptr " DPxMOD " does not have a matching target pointer.\n",
                DPxPTR(HostPtr));
        return OFFLOAD_FAIL;
    }

    // get target table.
    __tgt_target_table *TargetTable = nullptr;
    {
        std::lock_guard<std::mutex> TrlTblLock(xkomp->TrlTblMtx);
        assert(TM->Table->TargetsTable.size() > (size_t)DeviceId &&
                "Not expecting a device ID outside the table's bounds!");
        TargetTable = TM->Table->TargetsTable[DeviceId];
    }
    assert(TargetTable && "Global data has not been mapped\n");

    DP("loop trip count is %" PRIu64 ".\n", KernelArgs.Tripcount);

    // We need to keep bases and offsets separate. Sometimes (e.g. in OpenCL) we
    // need to manifest base pointers prior to launching a kernel. Even if we have
    // mapped an object only partially, e.g. A[N:M], although the kernel is
    // expected to access elements starting at address &A[N] and beyond, we still
    // need to manifest the base of the array &A[0]. In other cases, e.g. the COI
    // API, we need the begin address itself, i.e. &A[N], as the API operates on
    // begin addresses, not bases. That's why we pass args and offsets as two
    // separate entities so that each plugin can do what it needs. This behavior
    // was introduced via https://reviews.llvm.org/D33028 and commit 1546d319244c.
    SmallVector<void *> TgtArgs;
    SmallVector<ptrdiff_t> TgtOffsets;

    PrivateArgumentManagerTy PrivateArgumentManager(Device, AsyncInfo);

    int NumClangLaunchArgs = KernelArgs.NumArgs;
    int Ret = OFFLOAD_SUCCESS;
    if (NumClangLaunchArgs) {
        // Process data, such as data mapping, before launching the kernel
        Ret = processDataBefore(Loc, DeviceId, HostPtr, NumClangLaunchArgs,
                KernelArgs.ArgBasePtrs, KernelArgs.ArgPtrs,
                KernelArgs.ArgSizes, KernelArgs.ArgTypes,
                KernelArgs.ArgNames, KernelArgs.ArgMappers, TgtArgs,
                TgtOffsets, PrivateArgumentManager, AsyncInfo);
        if (Ret != OFFLOAD_SUCCESS) {
            REPORT("Failed to process data before launching the kernel.\n");
            return OFFLOAD_FAIL;
        }

        // Clang might pass more values via the ArgPtrs to the runtime that we pass
        // on to the kernel.
        // TODO: Next time we adjust the KernelArgsTy we should introduce a new
        // NumKernelArgs field.
        KernelArgs.NumArgs = TgtArgs.size();
    }

    // Launch device execution.
    void *TgtEntryPtr = TargetTable->EntriesBegin[TM->Index].Address;
    DP("Launching target execution %s with pointer " DPxMOD " (index=%d).\n",
            TargetTable->EntriesBegin[TM->Index].SymbolName, DPxPTR(TgtEntryPtr),
            TM->Index);

    {
        assert(KernelArgs.NumArgs == TgtArgs.size() && "Argument count mismatch!");
        TIMESCOPE_WITH_DETAILS_AND_IDENT(
                "Kernel Target",
                "NumArguments=" + std::to_string(KernelArgs.NumArgs) +
                ";NumTeams=" + std::to_string(KernelArgs.NumTeams[0]) +
                ";TripCount=" + std::to_string(KernelArgs.Tripcount),
                Loc);

        Ret = Device.launchKernel(TgtEntryPtr, TgtArgs.data(), TgtOffsets.data(),
                KernelArgs, AsyncInfo);
    }

    if (Ret != OFFLOAD_SUCCESS) {
        REPORT("Executing target region abort target.\n");
        return OFFLOAD_FAIL;
    }

    if (NumClangLaunchArgs) {
        // Transfer data back and deallocate target memory for (first-)private
        // variables
        Ret = processDataAfter(Loc, DeviceId, HostPtr, NumClangLaunchArgs,
                KernelArgs.ArgBasePtrs, KernelArgs.ArgPtrs,
                KernelArgs.ArgSizes, KernelArgs.ArgTypes,
                KernelArgs.ArgNames, KernelArgs.ArgMappers,
                PrivateArgumentManager, AsyncInfo);
        if (Ret != OFFLOAD_SUCCESS) {
            REPORT("Failed to process data after launching the kernel.\n");
            return OFFLOAD_FAIL;
        }
    }
    # endif

    return OFFLOAD_SUCCESS;
}

/// Implements a kernel entry that executes the target region on the specified
/// device.
///
/// \param Loc Source location associated with this target region.
/// \param DeviceId The device to execute this region, -1 indicated the default.
/// \param NumTeams Number of teams to launch the region with, -1 indicates a
///                 non-teams region and 0 indicates it was unspecified.
/// \param ThreadLimit Limit to the number of threads to use in the kernel
///                    launch, 0 indicates it was unspecified.
/// \param HostPtr  The pointer to the host function registered with the kernel.
/// \param Args     All arguments to this kernel launch (see struct definition).
extern "C"
int
__tgt_target_kernel(
    ident_t * loc,
    int64_t DeviceId,
    int32_t NumTeams,
    int32_t ThreadLimit,
    void *HostPtr,
    KernelArgsTy *KernelArgs
) {
    if (KernelArgs->Flags.NoWait)
        LOGGER_FATAL("impl me");
    LOGGER_NOT_IMPLEMENTED();

    if (NumTeams == -1)
        KernelArgs->NumTeams[0] = NumTeams = 1;

    return target(loc, DeviceId, NumTeams, ThreadLimit, HostPtr, KernelArgs);
}


/////////////////
// Data
/////////////////

/// creates host-to-target data mapping, stores it in the
/// libomptarget.so internal structure (an entry in a stack of data maps)
/// and passes the data to the device.
extern "C"
void
__tgt_target_data_begin_mapper(ident_t *Loc, int64_t DeviceId,
        int32_t ArgNum, void **ArgsBase,
        void **Args, int64_t *ArgSizes,
        int64_t *ArgTypes,
        map_var_info_t *ArgNames,
        void **ArgMappers
) {
    LOGGER_NOT_IMPLEMENTED();
}

/// passes data from the target, releases target memory and destroys
/// the host-target mapping (top entry from the stack of data maps)
/// created by the last __tgt_target_data_begin.
extern "C"
void
__tgt_target_data_end_mapper(ident_t *Loc, int64_t DeviceId,
                                         int32_t ArgNum, void **ArgsBase,
                                         void **Args, int64_t *ArgSizes,
                                         int64_t *ArgTypes,
                                         map_var_info_t *ArgNames,
                                         void **ArgMappers
) {
    LOGGER_NOT_IMPLEMENTED();
}

/////////////////
/// adds a target shared library to the target execution image
/////////////////
extern "C"
void
__tgt_register_lib(__tgt_bin_desc * desc)
{
    LOGGER_NOT_IMPLEMENTED();
}

extern "C"
void
__tgt_unregister_lib(__tgt_bin_desc * desc)
{
    LOGGER_NOT_IMPLEMENTED();
}

//////////////////////////////////
void
xkomp_target_init(xkomp_t * xkomp)
{
    new (&xkomp->HostEntriesBeginToTransTable) HostEntriesBeginToTransTableTy();
    new (&xkomp->TrlTblMtx) std::mutex();
    new (&xkomp->HostEntriesBeginRegistrationOrder) std::vector<EntryTy *>();

    new (&xkomp->HostPtrToTableMap) HostPtrToTableMapTy();
    new (&xkomp->TblMapMtx) std::mutex();
}


