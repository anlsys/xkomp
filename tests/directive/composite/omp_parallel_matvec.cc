// xkomp: supported  (parallel for over independent rows, checked vs serial)
//
// Dense matrix-vector product y = A*x.  Each row is independent, distributed by
// a `parallel for`; the result is validated against a serial reference.

#include "common.h"

#define ROWS 128
#define COLS 96

static void
test_matvec(void)
{
    static double A[ROWS * COLS];
    static double x[COLS];
    static double y[ROWS];
    static double yref[ROWS];

    for (int c = 0; c < COLS; ++c)
        x[c] = 0.5 * c + 1.0;
    for (int r = 0; r < ROWS; ++r)
        for (int c = 0; c < COLS; ++c)
            A[r * COLS + c] = 0.01 * (r + 1) + 0.001 * c;

    // serial reference
    for (int r = 0; r < ROWS; ++r)
    {
        double s = 0.0;
        for (int c = 0; c < COLS; ++c)
            s += A[r * COLS + c] * x[c];
        yref[r] = s;
    }

    // parallel
    #pragma omp parallel for num_threads(4) shared(A, x, y)
    for (int r = 0; r < ROWS; ++r)
    {
        double s = 0.0;
        for (int c = 0; c < COLS; ++c)
            s += A[r * COLS + c] * x[c];
        y[r] = s;
    }

    for (int r = 0; r < ROWS; ++r)
        CHECK_NEAR(y[r], yref[r], 1e-9);
}

int
main(void)
{
    for (int r = 0; r < REPETITIONS; ++r)
        test_matvec();

    TEST_PASS();
    return 0;
}
