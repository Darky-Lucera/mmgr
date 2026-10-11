#include <atomic>
#include <thread>
#include <vector>
#include "check.h"

static const uint32_t threadCount = 8;
static const uint32_t iterations  = 2000;

// Four allocations and one reallocation per iteration. The pointers of new are volatile, so that an optimizing compiler
// does not leave out the new-expressions.
static void
work() {
    for (uint32_t i = 0; i < iterations; ++i) {
        int * volatile value = new int(static_cast<int>(i));
        int * volatile array = new int[8];
        char * text          = strdup("thread");
        void * block         = malloc(16 + i % 64);
        block                = realloc(block, 128);

        delete value;
        delete[] array;
        free(text);
        free(block);
    }
}

// Without the lock, m_validateAllAllocUnits() could walk the hash table while another thread changes it, and find a
// different number of blocks than the statistics. m_calcAllUnused() and m_dumpMemoryReport() are not called here,
// because they read the contents of the blocks while their threads write them.
static void
readState(const std::atomic<bool> *stop) {
    while (!stop->load()) {
        m_validateAllAllocUnits();
        m_getMemoryStatistics();
    }
}

// Several threads allocate and release at the same time, after m_enableMultithread(), while another one reads the
// state of mmgr.
int
main() {
    m_enableMultithread();
    uint64_t before = m_getMemoryStatistics().accumulatedAllocUnitCount;

    {
        std::atomic<bool> stop(false);
        std::thread       reader(readState, &stop);

        std::vector<std::thread> threads;
        for (uint32_t i = 0; i < threadCount; ++i) {
            threads.emplace_back(work);
        }

        for (std::thread & thread : threads) {
            thread.join();
        }

        stop = true;
        reader.join();
    }

    // std::thread can allocate too, so the count may be higher, but a lost update would make it lower
    uint64_t allocations = m_getMemoryStatistics().accumulatedAllocUnitCount - before;
    CHECK(allocations >= threadCount * iterations * 4);

    // m_validateAllAllocUnits() returns true when it finds errors
    CHECK(!m_validateAllAllocUnits());
    CHECK(loggedErrors() == 0);
    CHECK(liveBlocks() == 0);
    return checkFailures;
}
