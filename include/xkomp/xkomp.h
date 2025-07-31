#ifndef __XKOMP_H__
# define __XKOMP_H__

# include <xkrt/xkrt.h>

# include <xkomp/support.h>

# if XKOMP_SUPPORT_TARGET
#  include <xkomp/target.h>
# endif /* XKOMP_SUPPORT_TARGET */

/* environment variables parsed at program starts */
typedef struct  xkomp_env_t
{
    char    OMP_DISPLAY_ENV;
    int     OMP_NUM_THREADS;
    int     OMP_THREAD_LIMIT;
    char *  OMP_PLACES;
    char *  OMP_PROC_BIND;
}               xkomp_env_t;

/** global variable that holds the entire openmp context */
typedef struct  xkomp_t
{
    /* underlaying XKaapi runtime */
    xkrt_runtime_t runtime;

    /* omp task format */
    task_format_id_t task_format;

    /* environment variables */
    xkomp_env_t env;

    /* the team of thread for parallel region */
    xkrt_team_t team;

    ////////////// Target mapping /////////////////////

    # if XKOMP_SUPPORT_TARGET

    // Minimal plugin manager adapted from llvm offload
    struct {
        /// Translation table retrieved from the binary
        HostEntriesBeginToTransTableTy HostEntriesBeginToTransTable;
        std::mutex TrlTblMtx;
        std::vector<EntryTy *> HostEntriesBeginRegistrationOrder;

        /* mapping from host function to device function */
        HostPtrToTableMapTy HostPtrToTableMap;
        std::mutex TblMapMtx;

        /// Executable images and information extracted from the input images passed
        /// to the runtime.
        std::vector<std::unique_ptr<DeviceImageTy>> DeviceImages;

        # if 0
        // set of all device images currently in use.
        llvm::DenseSet<const __tgt_device_image *> UsedImages;
        # endif

    } PM;
    # endif /* XKOMP_SUPPORT_TARGET */
}               xkomp_t;

extern xkomp_t * xkomp;
xkomp_t * xkomp_get(void);

/** load env variables */
void xkomp_env_init(xkomp_env_t * env);

/* save task format */
void xkomp_task_register_format(xkomp_t * xkomp);

/* init target */
void xkomp_target_init(xkomp_t * xkomp);

# endif /* __XKOMP_H__ */
