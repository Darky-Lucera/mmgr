#include "check.h"

// Leaves four blocks for the leak report (memleaks.log) and releases the others correctly.
int
main() {
    int *  leakNew    = new int;
    int *  leakArray  = new int[5];
    int *  leakMalloc = static_cast<int *>(malloc(sizeof(int)));
    char * leakString = strdup("leak");

    *leakNew      = 1;
    leakArray[1]  = 1;
    *leakMalloc   = 1;
    leakString[0] = 'L';

    // Passing the leaks to mmgr also keeps an optimizing compiler from removing them: since C++14 it may leave out a
    // new-expression whose result is never read.
    CHECK(m_validateAddress(leakNew));
    CHECK(m_validateAddress(leakArray));
    CHECK(m_validateAddress(leakMalloc));
    CHECK(m_validateAddress(leakString));

    int * released = new int[10];
    delete[] released;

    void * block = malloc(64);
    free(block);

    CHECK(liveBlocks() == 4);
    CHECK(loggedErrors() == 0);
    return checkFailures;
}
