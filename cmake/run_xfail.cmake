# run_xfail.cmake -- driver for a "not-yet-supported construct" test.
#
# Builds TARGET on demand, then runs EXE.  Exits 0 only if the target BOTH builds
# and runs successfully; otherwise exits non-zero so CTest reports the test as
# FAILED.
#
# These targets are EXCLUDE_FROM_ALL (a link failure must not break `make`), so
# they are compiled here, at test time.  The test is registered WITHOUT the
# WILL_FAIL property: a construct XKOMP does not support yet therefore shows up
# as "Failed" in `make test` (keeping the gap visible), and flips to "Passed"
# the day it compiles, links and runs correctly.
#
# Required -D arguments: TARGET, BIN_DIR, EXE

if(NOT DEFINED TARGET OR NOT DEFINED BIN_DIR OR NOT DEFINED EXE)
    message(FATAL_ERROR "run_xfail.cmake requires -DTARGET, -DBIN_DIR and -DEXE")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${BIN_DIR} --target ${TARGET}
    RESULT_VARIABLE build_rc
    OUTPUT_VARIABLE build_out
    ERROR_VARIABLE  build_out)

if(NOT build_rc EQUAL 0)
    message(STATUS "[unsupported] '${TARGET}' did not build:\n${build_out}")
    message(FATAL_ERROR "[unsupported] '${TARGET}': construct not supported (build failed)")
endif()

execute_process(COMMAND ${EXE} RESULT_VARIABLE run_rc)

if(NOT run_rc EQUAL 0)
    message(FATAL_ERROR "[unsupported] '${TARGET}': construct not supported (exited ${run_rc})")
endif()

message(STATUS "[unsupported] '${TARGET}' now builds and runs -- the construct "
               "appears supported; move it to a normal test.")
