// xkomp: XFAIL - `sections` lowering needs the sections/dispatch runtime entry
//        points (__kmpc_sections_init / __kmpc_dispatch_* depending on the
//        clang version), none of which XKOMP exports.  Expected to fail to link
//        (or, if the pragma is ignored, to run incorrectly).

#include "common.h"

int
main(void)
{
    int a = 0, b = 0, c = 0;

    #pragma omp parallel num_threads(4) shared(a, b, c)
    {
        #pragma omp sections
        {
            #pragma omp section
            a = 1;
            #pragma omp section
            b = 2;
            #pragma omp section
            c = 3;
        }
    }

    CHECK_EQ(a, 1);
    CHECK_EQ(b, 2);
    CHECK_EQ(c, 3);

    TEST_PASS();
    return 0;
}
