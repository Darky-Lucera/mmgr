#include "check.h"

#if !defined(MMGR_NO_DELETE_MACRO)
    #error "CMake must define MMGR_NO_DELETE_MACRO for this test"
#endif

// Without the delete macro, "= delete" compiles after mmgr.h, and mmgr still finds the errors of delete (only without
// the file and line of the delete).
struct NonCopyable {
    NonCopyable() {
    }

    NonCopyable(const NonCopyable &) = delete;
};

int
main() {
    m_setStopOnErrors(false);
    int errors = loggedErrors();

    NonCopyable * object = new NonCopyable;
    CHECK(m_validateAddress(object));
    delete object;

    // Through volatile pointers, so that the compiler does not warn about the errors made on purpose
    int * volatile twice = new int;
    delete twice;
    delete twice;
    CHECK(loggedErrors() == ++errors);

    int * volatile array = new int[1];
    delete array;
    CHECK(loggedErrors() == ++errors);

    // Passing the leak to mmgr also keeps an optimizing compiler from removing the new (see leaks.cpp)
    int * leak = new int;
    *leak      = 1;
    CHECK(m_validateAddress(leak));

    CHECK(liveBlocks() == 1);
    CHECK(loggedErrors() == errors);
    return checkFailures;
}
