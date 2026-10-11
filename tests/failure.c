#include <errno.h>
#include "check.h"

// A function with four allocations. When one of them fails, it must release the others and report the failure.
static int
buildBlocks(void) {
    char * first = (char *)malloc(16);
    if (first == NULL) {
        return 0;
    }

    char * text = strdup("text");
    if (text == NULL) {
        free(first);
        return 0;
    }

    char * zeroed = (char *)calloc(4, 4);
    if (zeroed == NULL) {
        free(text);
        free(first);
        return 0;
    }

    // A failed realloc() leaves zeroed as it was
    char * grown = (char *)realloc(zeroed, 64);
    if (grown == NULL) {
        free(zeroed);
        free(text);
        free(first);
        return 0;
    }

    free(grown);
    free(text);
    free(first);
    return 1;
}

int
main(void) {
    uint64_t before = liveBlocks();
    int      errors = loggedErrors();

    // Each allocation of buildBlocks() fails once. The loop ends when the n-th request never happens.
    uint64_t n;
    for (n = 1;; ++n) {
        m_setFailOnAllocation(n);
        int built = buildBlocks();
        if (m_getAllocationRequestCount() < n) {
            CHECK(built == 1);
            break;
        }

        CHECK(built == 0);
        CHECK(liveBlocks() == before);
    }
    CHECK(n == 5);

    // Only the n-th request fails
    m_setFailOnAllocation(2);
    CHECK(m_getFailOnAllocation() == 2);
    void * first  = malloc(8);
    void * second = malloc(8);
    void * third  = malloc(8);
    CHECK(first != NULL && second == NULL && third != NULL);
    CHECK(m_getAllocationRequestCount() == 3);
    free(first);
    free(third);

    m_setFailOnAllocation(0);
    for (int i = 0; i < 10; ++i) {
        void * block = malloc(8);
        CHECK(block != NULL);
        free(block);
    }

    char * kept = (char *)malloc(8);
    strcpy(kept, "keep");
    m_setFailOnAllocation(1);
    CHECK(realloc(kept, 1024) == NULL);
    CHECK(m_validateAddress(kept) && strcmp(kept, "keep") == 0);
    free(kept);

#if defined(_WIN32)
    m_setFailOnAllocation(1);
    CHECK(_aligned_malloc(32, 64) == NULL);
#else
    void * aligned = NULL;
    m_setFailOnAllocation(1);
    CHECK(posix_memalign(&aligned, 64, 32) == ENOMEM && aligned == NULL);
    m_setFailOnAllocation(1);
    CHECK(aligned_alloc(64, 32) == NULL);
#endif
    m_setFailOnAllocation(0);

    // An induced failure is not an error of the program
    CHECK(loggedErrors() == errors);
    CHECK(liveBlocks() == before);
    return checkFailures;
}
