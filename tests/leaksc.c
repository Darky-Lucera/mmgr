#include "check.h"

// The same as leaks.cpp in C, plus calloc and realloc.
int
main(void) {
    // In C, new is an ordinary identifier, so mmgr.h must not redefine it
    int new           = 1;
    int *  leakMalloc = (int *)malloc(sizeof(int));
    char * leakString = strdup("leak");

    *leakMalloc   = new;
    leakString[0] = 'L';

    int * zeroed = (int *)calloc(4, sizeof(int));
    CHECK(zeroed != NULL && zeroed[0] == 0 && zeroed[3] == 0);

    int * grown = (int *)realloc(zeroed, 8 * sizeof(int));
    CHECK(grown != NULL && grown[0] == 0 && grown[3] == 0);
    free(grown);

    // calloc() fails when count * size does not fit in a size_t
    CHECK(calloc(SIZE_MAX, 2) == NULL);

    CHECK(liveBlocks() == 2);
    CHECK(loggedErrors() == 1);
    return checkFailures;
}
