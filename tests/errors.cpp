#include "check.h"

struct Base {
    int base;
};

struct Derived : Base {
    int derived[4];
};

// Each error must add exactly one line to memory.log. The pointers are volatile, so that the compiler does not see the
// errors that the test makes on purpose, and does not warn about them.
int
main() {
    m_setStopOnErrors(false);
    int errors = loggedErrors();

    int * volatile twice = new int;
    delete twice;
    delete twice;
    CHECK(loggedErrors() == ++errors);

    // One element, so that a sized delete passes the same size and only the method is wrong
    int * volatile array = new int[1];
    delete array;
    CHECK(loggedErrors() == ++errors);

    int * volatile single = new int;
    delete[] single;
    CHECK(loggedErrors() == ++errors);

    int * volatile fromMalloc = static_cast<int *>(malloc(sizeof(int)));
    delete fromMalloc;
    CHECK(loggedErrors() == ++errors);

    int * volatile fromNew = new int;
    free(fromNew);
    CHECK(loggedErrors() == ++errors);

    // Overrun and underrun, detected when the block is released
    int * volatile overrun = new int[5];
    overrun[5]             = 1;
    delete[] overrun;
    CHECK(loggedErrors() == ++errors);

    int * volatile underrun = new int[5];
    underrun[-1]            = 1;
    delete[] underrun;
    CHECK(loggedErrors() == ++errors);

    int local                  = 0;
    int * volatile notFromMmgr = &local;
    delete notFromMmgr;
    CHECK(loggedErrors() == ++errors);

#if defined(__cpp_sized_deallocation)
    // Deleting through a base class without a virtual destructor passes the size of the base class
    Base * volatile sliced = new Derived;
    delete sliced;
    CHECK(loggedErrors() == ++errors);
#endif

    CHECK(liveBlocks() == 0);
    return checkFailures;
}
