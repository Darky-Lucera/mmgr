// ---------------------------------------------------------------------------------------------------------------------------------
/*
//                                     _
//                                    | |
//  _ __ ___  _ __ ___   __ _ _ __    | |__
// | '_ ` _ \| '_ ` _ \ / _` | '__|   | '_ \
// | | | | | | | | | | | (_| | |    _ | | | |
// |_| |_| |_|_| |_| |_|\__, |_|   (_)|_| |_|
//                       __/ |
//                      |___/
//
*/
// Memory manager & tracking software
//
// Best viewed with 8-character tabs and (at least) 132 columns
//
// ---------------------------------------------------------------------------------------------------------------------------------
//
// Originally released under a custom license.
// This historical re-release is provided under the MIT License.
// See the LICENSE file in the repo root for details.
//
// https://github.com/nettlep
//
// ---------------------------------------------------------------------------------------------------------------------------------
// Originally created on 12/22/2000 by Paul Nettle
//
// Copyright 2000, Fluid Studios, Inc., all rights reserved.
// ---------------------------------------------------------------------------------------------------------------------------------

#ifndef _H_MMGR
#define _H_MMGR

#include <stddef.h>
#include <stdint.h>

#if !defined(__cplusplus)
    #include <stdbool.h>
#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// For systems that don't have the __FUNCTION__ variable, we can just define it here
// ---------------------------------------------------------------------------------------------------------------------------------

//#define __FUNCTION__ "??"

// ---------------------------------------------------------------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct tag_au {
    size_t          actualSize;
    size_t          reportedSize;
    void *          actualAddress;
    void *          reportedAddress;
    char            sourceFile[40];
    char            sourceFunc[40];
    uint32_t        sourceLine;
    uint32_t        allocationType;
    bool            breakOnDealloc;
    bool            breakOnRealloc;
    uint64_t        allocationNumber;
    struct tag_au * next;
    struct tag_au * prev;
} sAllocUnit;

typedef struct {
    uint64_t        totalReportedMemory;
    uint64_t        totalActualMemory;
    uint64_t        peakReportedMemory;
    uint64_t        peakActualMemory;
    uint64_t        accumulatedReportedMemory;
    uint64_t        accumulatedActualMemory;
    uint64_t        accumulatedAllocUnitCount;
    uint64_t        totalAllocUnitCount;
    uint64_t        peakAllocUnitCount;
} sMStats;

// ---------------------------------------------------------------------------------------------------------------------------------
// External constants
// ---------------------------------------------------------------------------------------------------------------------------------

extern const uint32_t       m_alloc_unknown;
extern const uint32_t       m_alloc_new;
extern const uint32_t       m_alloc_new_array;
extern const uint32_t       m_alloc_malloc;
extern const uint32_t       m_alloc_calloc;
extern const uint32_t       m_alloc_realloc;
extern const uint32_t       m_alloc_delete;
extern const uint32_t       m_alloc_delete_array;
extern const uint32_t       m_alloc_free;
extern const uint32_t       m_alloc_aligned_malloc;
extern const uint32_t       m_alloc_aligned_realloc;
extern const uint32_t       m_alloc_aligned_free;

// ---------------------------------------------------------------------------------------------------------------------------------
// Multi thread support
// ---------------------------------------------------------------------------------------------------------------------------------

void        m_enableMultithread(void);

// ---------------------------------------------------------------------------------------------------------------------------------
// Used by the macros
// ---------------------------------------------------------------------------------------------------------------------------------

void        m_setOwner(const char *file, const uint32_t line, const char *func);

// ---------------------------------------------------------------------------------------------------------------------------------
// Allocation breakpoints
// ---------------------------------------------------------------------------------------------------------------------------------

void        m_setBreakOnAllocation(uint64_t count);
uint64_t    m_getBreakOnAllocation(void);
void        m_setBreakOnRealloc(const void *reportedAddress, bool set);
bool        m_getBreakOnRealloc(const void *reportedAddress);
void        m_setBreakOnDealloc(const void *reportedAddress, bool set);
bool        m_getBreakOnDealloc(const void *reportedAddress);

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Flags & options -- Call these routines to enable/disable the following options
// ---------------------------------------------------------------------------------------------------------------------------------

void        m_setAlwaysValidateAll(bool set);   // Force a validation of all allocation units each time we enter this software
bool        m_getAlwaysValidateAll(void);
void        m_setAlwaysLogAll(bool set);        // Force a log of every allocation & deallocation into memory.log
bool        m_getAlwaysLogAll(void);
void        m_setAlwaysWipeAll(bool set);       // Force this software to always wipe memory with a pattern when it is being allocated/deallocated
bool        m_getAlwaysWipeAll(void);
void        m_setRandomWipe(bool set);          // Force this software to use a random pattern when wiping memory -- good for stress testing
bool        m_getRandomWipe(void);
void        m_setStopOnErrors(bool set);        // Stop the program on each error (the default), or only log the error and go on
bool        m_getStopOnErrors(void);

// ---------------------------------------------------------------------------------------------------------------------------------
// Failure injection
// ---------------------------------------------------------------------------------------------------------------------------------

void        m_setFailOnAllocation(uint64_t count);      // Only the count-th allocation request from now fails (0 turns it off)
uint64_t    m_getFailOnAllocation(void);
uint64_t    m_getAllocationRequestCount(void);          // Allocation requests since the last call to m_setFailOnAllocation()

// ---------------------------------------------------------------------------------------------------------------------------------
// The meat of the memory tracking software
// ---------------------------------------------------------------------------------------------------------------------------------

void *      m_allocator(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const uint32_t allocationType,
    const size_t   reportedSize
);
void *      m_reallocator(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const uint32_t reallocationType,
    const size_t   reportedSize,
    void *         reportedAddress
);
void        m_deallocator(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const uint32_t deallocationType,
    const void *   reportedAddress
);

// strdup
char *      m_allocatorCpy(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const uint32_t allocationType,
    const char *   _str
);

// calloc
void *      m_callocator(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const size_t   count,
    const size_t   size
);

// aligned_alloc, memalign, posix_memalign
void *      m_alignedAllocator(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const uint32_t allocationType,
    const size_t   size,
    const size_t   alignment
);
int         m_posixMemalign(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    void **        result,
    const size_t   alignment,
    const size_t   size
);

// _aligned_malloc (with m_alignedAllocator), _aligned_realloc, _aligned_recalloc, _aligned_msize
void *      m_alignedReallocator(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    void *         reportedAddress,
    const size_t   size,
    const size_t   alignment
);
void *      m_alignedRecallocator(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    void *         reportedAddress,
    const size_t   count,
    const size_t   size,
    const size_t   alignment
);
size_t      m_alignedSize(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const void *   reportedAddress,
    const size_t   alignment,
    const size_t   offset
);

// ---------------------------------------------------------------------------------------------------------------------------------
// Utilitarian functions
// ---------------------------------------------------------------------------------------------------------------------------------

bool        m_validateAddress(const void *reportedAddress);
bool        m_validateAllocUnit(const sAllocUnit *allocUnit);
bool        m_validateAllAllocUnits(void);

// ---------------------------------------------------------------------------------------------------------------------------------
// Unused RAM calculations
// ---------------------------------------------------------------------------------------------------------------------------------

size_t      m_calcUnused(const sAllocUnit *allocUnit);
uint64_t    m_calcAllUnused(void);

// ---------------------------------------------------------------------------------------------------------------------------------
// Logging and reporting
// ---------------------------------------------------------------------------------------------------------------------------------

// Each line of the logs goes to its file and to the log function
typedef enum {
    m_log_memory,   // memory.log: the errors, and every operation with m_setAlwaysLogAll()
    m_log_leaks,    // memleaks.log: the leak report, written when the program ends
    m_log_report    // m_dumpMemoryReport()
} m_logKind;

typedef void (*m_logFunction)(m_logKind kind, const char *line);

void        m_setLogFunction(m_logFunction function);                       // m_logToStdout by default, NULL for none
void        m_logToStdout(m_logKind kind, const char *line);                // Prints the line with printf
void        m_setLogFiles(const char *memoryLog, const char *leakReport);   // memory.log and memleaks.log by default, NULL for no file
void        m_dumpLeakReport(void);                                         // Writes the leak report now, as at the end of the program

#if !defined(__cplusplus)
void        m_dumpAllocUnit(const sAllocUnit *allocUnit, const char *prefix);
void        m_dumpMemoryReport(const char *filename, const bool overwrite);
#else
void        m_dumpAllocUnit(const sAllocUnit *allocUnit, const char *prefix = "");
void        m_dumpMemoryReport(const char *filename = "memreport.log", const bool overwrite = true);
#endif

sMStats     m_getMemoryStatistics(void);

#if defined(__cplusplus)
} // end extern "C"
#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// Variations of global operators new & delete
// ---------------------------------------------------------------------------------------------------------------------------------

#if defined(__cplusplus)

void *  operator new(size_t reportedSize);
void *  operator new[](size_t reportedSize);
void *  operator new(size_t reportedSize, const char *sourceFile, uint32_t sourceLine);
void *  operator new[](size_t reportedSize, const char *sourceFile, uint32_t sourceLine);
void    operator delete(void *reportedAddress) noexcept;
void    operator delete[](void *reportedAddress) noexcept;

// The macros turn std::malloc(size) into std::m_allocator(...), so the functions that they call must also be in std.
namespace std {
    using ::m_allocator;
    using ::m_callocator;
    using ::m_reallocator;
    using ::m_deallocator;
    using ::m_alignedAllocator;
}

#endif

#endif // _H_MMGR

// ---------------------------------------------------------------------------------------------------------------------------------
// Macros -- "Kids, please don't try this at home. We're trained professionals here." :)
// ---------------------------------------------------------------------------------------------------------------------------------

#include "nommgr.h"

// In C, new and delete are ordinary identifiers that programs may use
#if defined(__cplusplus)

#define new             (m_setOwner   (__FILE__, __LINE__, __FUNCTION__), false) ? nullptr : new

// The delete macro breaks "= delete" (C++11 deleted functions) in any code compiled after this header.
#if !defined(MMGR_NO_DELETE_MACRO)
    #define delete      (m_setOwner   (__FILE__, __LINE__, __FUNCTION__), false) ? m_setOwner("", 0, "") : delete
#endif

#endif

#define malloc(sz)      m_allocator   (__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, sz)
#define calloc(num,sz)  m_callocator  (__FILE__, __LINE__, __FUNCTION__, num, sz)
#define realloc(ptr,sz) m_reallocator (__FILE__, __LINE__, __FUNCTION__, m_alloc_realloc, sz, ptr)
#define free(ptr)       m_deallocator (__FILE__, __LINE__, __FUNCTION__, m_alloc_free, ptr)
#define strdup(str)     m_allocatorCpy(__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, str)
#define _strdup(str)    m_allocatorCpy(__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, str)

// Only the aligned allocation functions that the platform has, so that code which calls a missing one does not start
// to compile because of mmgr.
#if defined(_WIN32)

#define _aligned_malloc(sz,al)          m_alignedAllocator   (__FILE__, __LINE__, __FUNCTION__, m_alloc_aligned_malloc, sz, al)
#define _aligned_realloc(ptr,sz,al)     m_alignedReallocator (__FILE__, __LINE__, __FUNCTION__, ptr, sz, al)
#define _aligned_recalloc(ptr,n,sz,al)  m_alignedRecallocator(__FILE__, __LINE__, __FUNCTION__, ptr, n, sz, al)
#define _aligned_msize(ptr,al,off)      m_alignedSize        (__FILE__, __LINE__, __FUNCTION__, ptr, al, off)
#define _aligned_free(ptr)              m_deallocator        (__FILE__, __LINE__, __FUNCTION__, m_alloc_aligned_free, ptr)

#else

#define aligned_alloc(al,sz)            m_alignedAllocator   (__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, sz, al)
#define memalign(al,sz)                 m_alignedAllocator   (__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, sz, al)
#define posix_memalign(ptr,al,sz)       m_posixMemalign      (__FILE__, __LINE__, __FUNCTION__, ptr, al, sz)

#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// mmgr.h - End of file
// ---------------------------------------------------------------------------------------------------------------------------------
