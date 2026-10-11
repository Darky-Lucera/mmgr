#include <new>
#include "check.h"

struct alignas(64) CacheLine {
    char data[64];
};

static int handlerCalls = 0;

static void
onAllocationFailure() {
    ++handlerCalls;
}

// m_setFailOnAllocation() with new. The pointers are volatile, so that an optimizing compiler cannot leave out the
// new-expressions (C++14 allows it when the result is not used).
int
main() {
    uint64_t before = liveBlocks();

    // Without a new_handler, new throws std::bad_alloc
    bool thrown = false;
    m_setFailOnAllocation(1);
    try {
        int * volatile value = new int;
        delete value;
    }
    catch (const std::bad_alloc &) {
        thrown = true;
    }
    CHECK(thrown);

    m_setFailOnAllocation(1);
    int * volatile nothrow = new (std::nothrow) int;
    CHECK(nothrow == nullptr);

    // When it works, mmgr tracks the nothrow block like any other
    nothrow = new (std::nothrow) int;
    CHECK(m_validateAddress(nothrow));
    delete nothrow;

    // With a new_handler, new calls it and tries again, and the second request does not fail
    m_setFailOnAllocation(1);
    std::set_new_handler(onAllocationFailure);
    int * volatile retried = new int;
    std::set_new_handler(nullptr);
    CHECK(retried != nullptr && handlerCalls == 1);
    CHECK(m_getAllocationRequestCount() == 2);
    delete retried;

    thrown = false;
    m_setFailOnAllocation(1);
    try {
        CacheLine * volatile line = new CacheLine;
        delete line;
    }
    catch (const std::bad_alloc &) {
        thrown = true;
    }
    CHECK(thrown);
    m_setFailOnAllocation(0);

    CHECK(liveBlocks() == before);
    CHECK(loggedErrors() == 0);
    return checkFailures;
}
