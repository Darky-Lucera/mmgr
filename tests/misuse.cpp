#include "check.h"

// Every combination of an allocation function and a release function: a wrong one must add exactly one error to
// memory.log, and a right one none. The same for releasing a block twice, releasing a block that mmgr does not know,
// and writing just outside each kind of block. All the blocks have the size of an int, so that a sized delete always
// passes the right size and only the method can be wrong.

enum Family {
    NewFamily,
    ArrayFamily,
    MallocFamily,
    AlignedFamily // _aligned_malloc() and _aligned_realloc()
};

// Outside Windows there are no _aligned_* functions, so these make the calls that their macros would make
static void *
alignedMalloc(size_t size) {
#if defined(_WIN32)
    return _aligned_malloc(size, 64);
#else
    return m_alignedAllocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_aligned_malloc, size, 64);
#endif
}

static void *
alignedRealloc(void *block, size_t size) {
#if defined(_WIN32)
    return _aligned_realloc(block, size, 64);
#else
    return m_alignedReallocator(__FILE__, __LINE__, __FUNCTION__, block, size, 64);
#endif
}

static void
alignedFree(void *block) {
#if defined(_WIN32)
    _aligned_free(block);
#else
    m_deallocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_aligned_free, block);
#endif
}

static size_t
alignedSize(void *block) {
#if defined(_WIN32)
    return _aligned_msize(block, 64, 0);
#else
    return m_alignedSize(__FILE__, __LINE__, __FUNCTION__, block, 64, 0);
#endif
}

static void *
allocateNew() {
    return new int;
}

static void *
allocateArray() {
    return new int[1];
}

static void *
allocateMalloc() {
    return malloc(sizeof(int));
}

static void *
allocateCalloc() {
    return calloc(1, sizeof(int));
}

static void *
allocateRealloc() {
    return realloc(nullptr, sizeof(int));
}

static void *
allocateStrdup() {
    return strdup("abc");
}

static void *
allocateAligned() {
    return alignedMalloc(sizeof(int));
}

static void *
allocateAlignedRealloc() {
    return alignedRealloc(nullptr, sizeof(int));
}

#if !defined(_WIN32)
static void *
allocateAlignedAlloc() {
    return aligned_alloc(64, sizeof(int));
}

static void *
allocateMemalign() {
    return memalign(64, sizeof(int));
}

static void *
allocatePosixMemalign() {
    void * block = nullptr;
    if (posix_memalign(&block, 64, sizeof(int)) != 0) {
        return nullptr;
    }

    return block;
}
#endif

// Each release function returns the block when it is still alive (after a reallocation)
static void *
releaseDelete(void *block) {
    delete static_cast<int *>(block);
    return nullptr;
}

static void *
releaseArray(void *block) {
    delete[] static_cast<int *>(block);
    return nullptr;
}

static void *
releaseFree(void *block) {
    free(block);
    return nullptr;
}

static void *
releaseRealloc(void *block) {
    return realloc(block, 2 * sizeof(int));
}

static void *
releaseAligned(void *block) {
    alignedFree(block);
    return nullptr;
}

static void *
releaseAlignedRealloc(void *block) {
    return alignedRealloc(block, 2 * sizeof(int));
}

static void
releaseCorrectly(void *block, Family family) {
    switch (family) {
        case NewFamily:
            delete static_cast<int *>(block);
            break;
        case ArrayFamily:
            delete[] static_cast<int *>(block);
            break;
        case MallocFamily:
            free(block);
            break;
        case AlignedFamily:
            alignedFree(block);
            break;
    }
}

struct Allocation {
    const char * name;
    Family       family;
    void *       (*allocate)();
};

struct Release {
    const char * name;
    Family       family;
    void *       (*release)(void *);
    Family       familyAfter;       // Of the block that a reallocation returns
    bool         frees;
};

static const Allocation allocations[] = {
    { "new",                    NewFamily,     allocateNew            },
    { "new[]",                  ArrayFamily,   allocateArray          },
    { "malloc",                 MallocFamily,  allocateMalloc         },
    { "calloc",                 MallocFamily,  allocateCalloc         },
    { "realloc(NULL)",          MallocFamily,  allocateRealloc        },
    { "strdup",                 MallocFamily,  allocateStrdup         },
    { "_aligned_malloc",        AlignedFamily, allocateAligned        },
    { "_aligned_realloc(NULL)", AlignedFamily, allocateAlignedRealloc },
#if !defined(_WIN32)
    { "aligned_alloc",          MallocFamily,  allocateAlignedAlloc   },
    { "memalign",               MallocFamily,  allocateMemalign       },
    { "posix_memalign",         MallocFamily,  allocatePosixMemalign  },
#endif
};

static const Release releases[] = {
    { "delete",           NewFamily,     releaseDelete,         NewFamily,     true  },
    { "delete[]",         ArrayFamily,   releaseArray,          ArrayFamily,   true  },
    { "free",             MallocFamily,  releaseFree,           MallocFamily,  true  },
    { "realloc",          MallocFamily,  releaseRealloc,        MallocFamily,  false },
    { "_aligned_free",    AlignedFamily, releaseAligned,        AlignedFamily, true  },
    { "_aligned_realloc", AlignedFamily, releaseAlignedRealloc, AlignedFamily, false },
};

static void
expectErrors(int before, int expected, const char *what, const char *withWhat) {
    int errors = loggedErrors() - before;
    if (errors != expected) {
        printf("%s + %s: %d errors, expected %d\n", what, withWhat, errors, expected);
        ++checkFailures;
    }
}

int
main() {
    m_setStopOnErrors(false);

    for (const Allocation & allocation : allocations) {
        for (const Release & release : releases) {
            int    before = loggedErrors();
            void * alive  = release.release(allocation.allocate());
            if (alive != nullptr) {
                releaseCorrectly(alive, release.familyAfter);
            }
            expectErrors(before, allocation.family == release.family ? 0 : 1, allocation.name, release.name);
        }
    }

    // Releasing a block twice
    for (const Release & release : releases) {
        if (!release.frees) {
            continue;
        }

        for (const Allocation & allocation : allocations) {
            if (allocation.family == release.family) {
                void * block = allocation.allocate();
                release.release(block);
                int before = loggedErrors();
                release.release(block);
                expectErrors(before, 1, release.name, "a released block");
                break;
            }
        }
    }

    // A block that mmgr does not know
    int local = 0;
    for (const Release & release : releases) {
        int before = loggedErrors();
        CHECK(release.release(&local) == nullptr);
        expectErrors(before, 1, release.name, "a block that mmgr does not know");
    }

    int before = loggedErrors();
    CHECK(alignedSize(&local) == static_cast<size_t>(-1));
    expectErrors(before, 1, "_aligned_msize", "a block that mmgr does not know");

    // Writing just after and just before each kind of block, found when the block is released
    for (const Allocation & allocation : allocations) {
        before                           = loggedErrors();
        unsigned char * volatile overrun = static_cast<unsigned char *>(allocation.allocate());
        overrun[sizeof(int)]             = 0x55;
        releaseCorrectly(overrun, allocation.family);
        expectErrors(before, 1, allocation.name, "a write after the block");

        before                            = loggedErrors();
        unsigned char * volatile underrun = static_cast<unsigned char *>(allocation.allocate());
        underrun[-1]                      = 0x55;
        releaseCorrectly(underrun, allocation.family);
        expectErrors(before, 1, allocation.name, "a write before the block");
    }

    // The same in the part that realloc() adds to a block
    before                         = loggedErrors();
    unsigned char * volatile grown = static_cast<unsigned char *>(malloc(sizeof(int)));
    grown                          = static_cast<unsigned char *>(realloc(grown, 16));
    grown[16]                      = 0x55;
    free(grown);
    expectErrors(before, 1, "realloc", "a write after the grown block");

    // Arguments that the aligned functions reject: an alignment that is not a power of two, and a count * size that
    // does not fit in a size_t
    before = loggedErrors();
    CHECK(m_alignedReallocator(__FILE__, __LINE__, __FUNCTION__, nullptr, 16, 24) == nullptr);
    expectErrors(before, 1, "_aligned_realloc", "an alignment of 24");

    before = loggedErrors();
    CHECK(m_alignedRecallocator(__FILE__, __LINE__, __FUNCTION__, nullptr, SIZE_MAX, 2, 64) == nullptr);
    expectErrors(before, 1, "_aligned_recalloc", "a size that does not fit");

    CHECK(liveBlocks() == 0);
    return checkFailures;
}
