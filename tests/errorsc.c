#include "check.h"

// The same as errors.cpp for the C functions. The aligned mismatches call the functions behind the macros, so that they
// run on every platform.
int
main(void) {
    m_setStopOnErrors(false);
    int errors = loggedErrors();

    int * volatile twice = (int *)malloc(sizeof(int));
    free(twice);
    free(twice);
    CHECK(loggedErrors() == ++errors);

    // Overrun and underrun, detected when the block is released
    int * volatile overrun = (int *)malloc(5 * sizeof(int));
    overrun[5]             = 1;
    free(overrun);
    CHECK(loggedErrors() == ++errors);

    int * volatile underrun = (int *)malloc(5 * sizeof(int));
    underrun[-1]            = 1;
    free(underrun);
    CHECK(loggedErrors() == ++errors);

    int local                  = 0;
    int * volatile notFromMmgr = &local;
    free(notFromMmgr);
    CHECK(loggedErrors() == ++errors);

    CHECK(realloc(notFromMmgr, 16) == NULL);
    CHECK(loggedErrors() == ++errors);

    // free() on an _aligned_malloc() block
    void * aligned = m_alignedAllocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_aligned_malloc, 32, 64);
    m_deallocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_free, aligned);
    CHECK(loggedErrors() == ++errors);

    // _aligned_free() on a malloc() block
    void * plain = malloc(32);
    m_deallocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_aligned_free, plain);
    CHECK(loggedErrors() == ++errors);

    // realloc() on an _aligned_malloc() block, which becomes a realloc() block
    aligned = m_alignedAllocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_aligned_malloc, 32, 64);
    aligned = realloc(aligned, 64);
    CHECK(loggedErrors() == ++errors);
    free(aligned);

    // _aligned_realloc() on a malloc() block, which becomes an _aligned_realloc() block
    plain = malloc(32);
    plain = m_alignedReallocator(__FILE__, __LINE__, __FUNCTION__, plain, 64, 64);
    CHECK(loggedErrors() == ++errors);
    m_deallocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_aligned_free, plain);

    CHECK(liveBlocks() == 0);
    CHECK(loggedErrors() == errors);
    return checkFailures;
}
