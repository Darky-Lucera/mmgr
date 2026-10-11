#ifndef MMGR_TESTS_CHECK_H
#define MMGR_TESTS_CHECK_H

// Each test checks its own results and returns the number of failed checks, so that CTest only needs the exit code.
// Include this header after every other one, because it includes mmgr.h.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mmgr.h>

static int checkFailures = 0;

#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) {                                                       \
            printf("%s(%d): check failed: %s\n", __FILE__, __LINE__, #condition); \
            ++checkFailures;                                                      \
        }                                                                         \
    } while (0)

// Number of errors ("[!]" lines) that mmgr has written to memory.log so far
static inline int
loggedErrors(void) {
    char   line[512];
    int    count = 0;
    FILE * file  = fopen("memory.log", "rb");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "[!]", 3) == 0) {
            ++count;
        }
    }

    fclose(file);
    return count;
}

static inline uint64_t
liveBlocks(void) {
    return m_getMemoryStatistics().totalAllocUnitCount;
}

static inline int
isAligned(const void *pointer, size_t alignment) {
    return ((uintptr_t)pointer % alignment) == 0;
}

#endif
