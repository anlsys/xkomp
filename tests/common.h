/*
 * Shared helpers for the XKOMP test-suite.
 *
 * Design notes:
 *  - Tests assert correctness.  We `#undef NDEBUG` *before* including
 *    <assert.h> so that asserts always fire, even if someone accidentally
 *    builds with -DNDEBUG.  The CMake harness additionally refuses to
 *    configure unless CMAKE_BUILD_TYPE=Debug.
 *  - Tests must be correct for *any* team size the runtime hands out (the
 *    number of threads actually spawned may differ from what is requested
 *    via num_threads()/OMP_NUM_THREADS).  Per-thread scratch arrays are
 *    therefore sized to XKOMP_TEST_MAXT and we assert the team fits.
 *  - Only constructs/clauses that XKOMP actually implements are used here;
 *    unsupported ones live under tests/xfail/.
 */

#ifndef XKOMP_TEST_COMMON_H
#define XKOMP_TEST_COMMON_H

#ifdef NDEBUG
# undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <omp.h>

/* Upper bound on the team sizes the tests rely on for per-thread scratch. */
#define XKOMP_TEST_MAXT 1024

/* LLVM-testsuite-style knobs: run each test several times to shake out races,
 * and split a decently large iteration space across the team. */
#define REPETITIONS 10
#define LOOPCOUNT   1000
#define NUM_TASKS   64

/*
 * XKOMP maps `depend` clauses to HANDLE accesses keyed on the *base address*
 * of the list item (pointer identity), not on byte ranges.  Two dependences
 * therefore conflict iff they share the same base address.  The depend tests
 * below use distinct scalar "tokens" as dependency handles accordingly.
 */

/* Evaluate `cond` exactly once; on failure print context then abort via assert. */
#define CHECK(cond)                                                            \
    do {                                                                       \
        const bool _xk_ok = (cond);                                            \
        if (!_xk_ok)                                                           \
            fprintf(stderr, "[xkomp-test] CHECK failed: (%s) at %s:%d\n",      \
                    #cond, __FILE__, __LINE__);                                \
        assert(_xk_ok);                                                        \
    } while (0)

/* Integral equality with a helpful message. */
#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        const long long _xk_a = (long long)(a);                               \
        const long long _xk_b = (long long)(b);                               \
        if (_xk_a != _xk_b)                                                    \
            fprintf(stderr,                                                    \
                    "[xkomp-test] CHECK_EQ failed: %s (%lld) != %s (%lld)"     \
                    " at %s:%d\n",                                             \
                    #a, _xk_a, #b, _xk_b, __FILE__, __LINE__);                 \
        assert(_xk_a == _xk_b);                                                \
    } while (0)

/* Floating-point closeness (absolute or relative tolerance). */
#define CHECK_NEAR(a, b, tol)                                                  \
    do {                                                                       \
        const double _xk_a = (double)(a);                                      \
        const double _xk_b = (double)(b);                                      \
        const double _xk_d = fabs(_xk_a - _xk_b);                              \
        const double _xk_s = fabs(_xk_b) > 1.0 ? fabs(_xk_b) : 1.0;           \
        const bool _xk_ok = _xk_d <= (tol) * _xk_s;                            \
        if (!_xk_ok)                                                           \
            fprintf(stderr,                                                    \
                    "[xkomp-test] CHECK_NEAR failed: %s (%g) vs %s (%g),"      \
                    " diff %g at %s:%d\n",                                      \
                    #a, _xk_a, #b, _xk_b, _xk_d, __FILE__, __LINE__);          \
        assert(_xk_ok);                                                        \
    } while (0)

/* Mark a test as passed (single, consistent success line). */
#define TEST_PASS()                                                           \
    do {                                                                       \
        printf("[xkomp-test] %s: PASS\n", __FILE__);                          \
        fflush(stdout);                                                        \
    } while (0)

#endif /* XKOMP_TEST_COMMON_H */
