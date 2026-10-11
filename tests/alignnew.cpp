#include <vector>
#include "check.h"

struct alignas(64) CacheLine {
    char data[64];
};

// Types with a large alignment use the aligned operator new of C++17. mmgr must track those blocks and check their
// padding like any other.
int
main() {
    m_setStopOnErrors(false);
    int errors = loggedErrors();

    CacheLine * one = new CacheLine;
    CHECK(isAligned(one, alignof(CacheLine)));
    CHECK(m_validateAddress(one));
    delete one;

    CacheLine * array = new CacheLine[3];
    CHECK(isAligned(array, alignof(CacheLine)));
    CHECK(m_validateAddress(array));
    delete[] array;

    {
        std::vector<CacheLine> lines(10);
        CHECK(isAligned(lines.data(), alignof(CacheLine)));
        CHECK(m_validateAddress(lines.data()));
    }

    // Overrun and underrun, detected when the block is released. Through volatile pointers, so that the compiler does
    // not warn about them.
    CacheLine * volatile overrun                                  = new CacheLine;
    reinterpret_cast<volatile char *>(overrun)[sizeof(CacheLine)] = 0x55;
    delete overrun;
    CHECK(loggedErrors() == ++errors);

    CacheLine * volatile underrun                   = new CacheLine;
    reinterpret_cast<volatile char *>(underrun)[-1] = 0x55;
    delete underrun;
    CHECK(loggedErrors() == ++errors);

    // Passing the leak to mmgr also keeps an optimizing compiler from removing the new (see leaks.cpp)
    CacheLine * leak = new CacheLine;
    leak->data[0]    = 1;
    CHECK(m_validateAddress(leak));

    CHECK(liveBlocks() == 1);
    CHECK(loggedErrors() == errors);
    return checkFailures;
}
