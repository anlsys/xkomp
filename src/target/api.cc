# include <xkomp/kmp.h>
# include <xkomp/target.h>
# include <xkrt/logger/logger.h>

/// adds a target shared library to the target execution image
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
    LOGGER_NOT_IMPLEMENTED();
    return 0;
}

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
