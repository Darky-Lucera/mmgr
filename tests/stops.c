#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
    #include <crtdbg.h>
#endif
#include <mmgr.h>

// CTest does not count a program that a signal stops as a failed run, even for a test that must fail, so the abort()
// of the assert becomes an ordinary exit code.
static void
onAbort(int signal) {
    (void)signal;
    _Exit(3);
}

// Each scenario must stop the program, so reaching the end of main is the failure, and CTest expects these runs to fail.
// The asserts are empty in release builds, so CMake only runs this test in debug builds.
int
main(int argc, char **argv) {
    signal(SIGABRT, onAbort);

#if defined(_MSC_VER)
    // The assert and abort() write to stderr, instead of opening a window that would block the test
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    const char * scenario = argc > 1 ? argv[1] : "";
    int          local    = 0;

    if (strcmp(scenario, "error") == 0) {
        // By default, an error stops the program
        int * volatile notFromMmgr = &local;
        free(notFromMmgr);
    }
    else if (strcmp(scenario, "allocation") == 0) {
        // The breakpoints stop the program even when the errors do not. Nothing allocates before main, so the first
        // allocation has the number 1.
        m_setStopOnErrors(false);
        m_setBreakOnAllocation(1);
        void * block = malloc(16);
        free(block);
    }
    else if (strcmp(scenario, "dealloc") == 0) {
        void * block = malloc(16);
        m_setStopOnErrors(false);
        m_setBreakOnDealloc(block, true);
        free(block);
    }
    else if (strcmp(scenario, "realloc") == 0) {
        void * block = malloc(16);
        m_setStopOnErrors(false);
        m_setBreakOnRealloc(block, true);
        block = realloc(block, 32);
        free(block);
    }

    printf("scenario '%s' did not stop the program\n", scenario);
    return 0;
}
