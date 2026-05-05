# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdarg.h>

# include <ffi.h>
struct tls_ffi_buffer_t
{
    ffi_type * args[XKOMP_MICROTASK_MAX_ARGS+2];
    void     * vals[XKOMP_MICROTASK_MAX_ARGS+2];
};
thread_local tls_ffi_buffer_t tls_ffi_buffer;

int
__kmp_invoke_microtask(
    kmpc_micro f,
    int gtid,
    int tid,
    int argc,
    void ** p
) {
    switch (argc)
    {
        case 0:
            (*f)(&gtid, &tid);
            break;
        case 1:
            (*f)(&gtid, &tid, p[0]);
            break;
        case 2:
            (*f)(&gtid, &tid, p[0], p[1]);
            break;
        case 3:
            (*f)(&gtid, &tid, p[0], p[1], p[2]);
            break;
        case 4:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3]);
            break;
        case 5:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4]);
            break;
        case 6:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5]);
            break;
        case 7:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
            break;
        case 8:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
            break;
        case 9:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);
            break;
        case 10:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
           break;
        case 11:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10]);
           break;
        case 12:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11]);
           break;
        case 13:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12]);
           break;
        case 14:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13]);
           break;
        case 15:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14]);
           break;
        case 16:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
           break;

        default:
        {
            if (argc >= XKOMP_MICROTASK_MAX_ARGS - 2)
            {
                LOGGER_FATAL("XKOMP_MICROTASK_MAX_ARGS is too small");
                return 0;
            }

            int* p_gtid = &gtid;
            int* p_tid  = &tid;

            tls_ffi_buffer.args[0] = &ffi_type_pointer;
            tls_ffi_buffer.args[1] = &ffi_type_pointer;

            tls_ffi_buffer.vals[0] = &p_gtid;
            tls_ffi_buffer.vals[1] = &p_tid;

            for (int i = 0; i < argc; ++i)
            {
                tls_ffi_buffer.args[i + 2] = &ffi_type_pointer;
                tls_ffi_buffer.vals[i + 2] = &p[i];
            }

            ffi_cif cif;
            if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc + 2, &ffi_type_void, tls_ffi_buffer.args) == FFI_OK)
            {
                ffi_call(&cif, FFI_FN(f), NULL, tls_ffi_buffer.vals);
                return 1;
            }

            return 0;
        }
    }
    return 1;
}
