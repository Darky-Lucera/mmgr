#include <cstdlib>
#include "check.h"

// The std:: allocation functions compile after mmgr.h, and mmgr tracks their blocks.
int
main() {
    void * block = std::malloc(16);
    CHECK(m_validateAddress(block));

    block = std::realloc(block, 64);
    CHECK(m_validateAddress(block));

    int * zeroed = static_cast<int *>(std::calloc(4, sizeof(int)));
    CHECK(m_validateAddress(zeroed) && zeroed[0] == 0 && zeroed[3] == 0);

    std::free(block);
    std::free(zeroed);

#if !defined(_WIN32)
    // Windows has no aligned_alloc
    void * aligned = std::aligned_alloc(64, 128);
    CHECK(isAligned(aligned, 64) && m_validateAddress(aligned));
    std::free(aligned);
#endif

    CHECK(liveBlocks() == 0);
    CHECK(loggedErrors() == 0);
    return checkFailures;
}
