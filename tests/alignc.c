#include <errno.h>
#include "check.h"

// The aligned allocation functions of C: the POSIX ones, or the _aligned_* ones on Windows. Leaves one block for the
// leak report.

#if defined(_WIN32)

static void
testPlatform(int *errors) {
    unsigned char * block = (unsigned char *)_aligned_malloc(100, 64);
    CHECK(block != NULL && isAligned(block, 64) && m_validateAddress(block));
    CHECK(_aligned_msize(block, 64, 0) == 100);
    for (int i = 0; i < 100; ++i) {
        block[i] = (unsigned char)i;
    }

    // _aligned_realloc() keeps the data and the alignment
    block = (unsigned char *)_aligned_realloc(block, 5000, 64);
    CHECK(block != NULL && isAligned(block, 64) && _aligned_msize(block, 64, 0) == 5000);
    for (int i = 0; i < 100; ++i) {
        CHECK(block[i] == (unsigned char)i);
    }

    block = (unsigned char *)_aligned_realloc(block, 20, 128);
    CHECK(block != NULL && isAligned(block, 128));
    for (int i = 0; i < 20; ++i) {
        CHECK(block[i] == (unsigned char)i);
    }

    // _aligned_recalloc() also fills with zeros the bytes beyond the old size
    block = (unsigned char *)_aligned_recalloc(block, 10, 8, 128);
    CHECK(block != NULL && isAligned(block, 128) && _aligned_msize(block, 128, 0) == 80);
    for (int i = 0; i < 80; ++i) {
        CHECK(block[i] == (i < 20 ? (unsigned char)i : 0));
    }
    _aligned_free(block);

    void * fromNull = _aligned_realloc(NULL, 30, 32);
    CHECK(fromNull != NULL && isAligned(fromNull, 32) && m_validateAddress(fromNull));

    // A size of 0 frees the block
    CHECK(_aligned_realloc(fromNull, 0, 32) == NULL);
    CHECK(!m_validateAddress(fromNull));
    _aligned_free(NULL);

    CHECK(_aligned_malloc(10, 24) == NULL);
    CHECK(loggedErrors() == ++*errors);

    // Overrun and underrun, detected when the block is released
    unsigned char * volatile overrun = (unsigned char *)_aligned_malloc(64, 64);
    overrun[64]                      = 0x55;
    _aligned_free(overrun);
    CHECK(loggedErrors() == ++*errors);

    unsigned char * volatile underrun = (unsigned char *)_aligned_malloc(64, 64);
    underrun[-1]                      = 0x55;
    _aligned_free(underrun);
    CHECK(loggedErrors() == ++*errors);

    void * leak = _aligned_malloc(10, 256);
    CHECK(leak != NULL);
}

#else

static void
testPlatform(int *errors) {
    unsigned char * block = (unsigned char *)aligned_alloc(64, 128);
    CHECK(block != NULL && isAligned(block, 64) && m_validateAddress(block));
    for (int i = 0; i < 128; ++i) {
        block[i] = (unsigned char)i;
    }

    // realloc() keeps the data when it grows or shrinks the block, but not the alignment
    block = (unsigned char *)realloc(block, 4096);
    CHECK(block != NULL && m_validateAddress(block));
    for (int i = 0; i < 128; ++i) {
        CHECK(block[i] == (unsigned char)i);
    }

    block = (unsigned char *)realloc(block, 16);
    CHECK(block != NULL);
    for (int i = 0; i < 16; ++i) {
        CHECK(block[i] == (unsigned char)i);
    }
    free(block);

    void * posix = NULL;
    CHECK(posix_memalign(&posix, 128, 50) == 0 && isAligned(posix, 128) && m_validateAddress(posix));
    free(posix);

    // On failure, posix_memalign() does not change the pointer. An alignment that is not a power of two is an error
    // that mmgr logs; one that is not a multiple of sizeof(void *) is only an EINVAL.
    void * untouched = &errors;
    CHECK(posix_memalign(&untouched, 3, 50) == EINVAL && untouched == (void *)&errors);
    CHECK(loggedErrors() == ++*errors);
    CHECK(posix_memalign(&untouched, 2, 50) == EINVAL);
    CHECK(loggedErrors() == *errors);

    void * old = memalign(32, 40);
    CHECK(old != NULL && isAligned(old, 32) && m_validateAddress(old));
    free(old);

    CHECK(aligned_alloc(48, 96) == NULL);
    CHECK(loggedErrors() == ++*errors);

    // Overrun and underrun, detected when the block is released
    unsigned char * volatile overrun = (unsigned char *)aligned_alloc(64, 64);
    overrun[64]                      = 0x55;
    free(overrun);
    CHECK(loggedErrors() == ++*errors);

    unsigned char * volatile underrun = (unsigned char *)aligned_alloc(64, 64);
    underrun[-1]                      = 0x55;
    free(underrun);
    CHECK(loggedErrors() == ++*errors);

    void * leak = aligned_alloc(256, 10);
    CHECK(leak != NULL);
}

#endif

int
main(void) {
    m_setStopOnErrors(false);
    int errors = loggedErrors();

    testPlatform(&errors);

    CHECK(liveBlocks() == 1);
    CHECK(loggedErrors() == errors);
    return checkFailures;
}
