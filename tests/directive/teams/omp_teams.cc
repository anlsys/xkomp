// xkomp: XFAIL - host `teams` lowers to __kmpc_fork_teams, which is
//        LOGGER_NOT_IMPLEMENTED() in src/kmp/fork.cc.  The symbol links, so
//        this fails at *runtime* (abort), not at link time.
//        (Device-side teams via `target teams ...` go through libomptarget and
//         are exercised by the target tests instead.)

#include "common.h"

int
main(void)
{
    int nteams = 0;

    #pragma omp teams num_teams(2) shared(nteams)
    {
        #pragma omp atomic
        nteams += 1;
    }

    CHECK(nteams >= 1);

    TEST_PASS();
    return 0;
}
