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
// Restrictions & freedoms pertaining to usage and redistribution of this software:
//
//  * This software is 100% free
//  * If you use this software (in part or in whole) you must credit the author.
//  * This software may not be re-distributed (in part or in whole) in a modified
//    form without clear documentation on how to obtain a copy of the original work.
//  * You may not use this software to directly or indirectly cause harm to others.
//  * This software is provided as-is and without warrantee. Use at your own risk.
//
// For more information, visit HTTP://www.FluidStudios.com
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

typedef struct tag_au
{
    size_t          actualSize;
    size_t          reportedSize;
    void            *actualAddress;
    void            *reportedAddress;
    char            sourceFile[40];
    char            sourceFunc[40];
    unsigned int    sourceLine;
    unsigned int    allocationType;
    bool            breakOnDealloc;
    bool            breakOnRealloc;
    unsigned int    allocationNumber;
    struct tag_au   *next;
    struct tag_au   *prev;
} sAllocUnit;

typedef struct
{
    unsigned int    totalReportedMemory;
    unsigned int    totalActualMemory;
    unsigned int    peakReportedMemory;
    unsigned int    peakActualMemory;
    unsigned int    accumulatedReportedMemory;
    unsigned int    accumulatedActualMemory;
    unsigned int    accumulatedAllocUnitCount;
    unsigned int    totalAllocUnitCount;
    unsigned int    peakAllocUnitCount;
} sMStats;

// ---------------------------------------------------------------------------------------------------------------------------------
// External constants
// ---------------------------------------------------------------------------------------------------------------------------------

extern  const   unsigned int    m_alloc_unknown;
extern  const   unsigned int    m_alloc_new;
extern  const   unsigned int    m_alloc_new_array;
extern  const   unsigned int    m_alloc_malloc;
extern  const   unsigned int    m_alloc_calloc;
extern  const   unsigned int    m_alloc_realloc;
extern  const   unsigned int    m_alloc_delete;
extern  const   unsigned int    m_alloc_delete_array;
extern  const   unsigned int    m_alloc_free;

// ---------------------------------------------------------------------------------------------------------------------------------
// Multi thread support
// ---------------------------------------------------------------------------------------------------------------------------------

void		EnableMultithreadInMMGR();

// ---------------------------------------------------------------------------------------------------------------------------------
// Used by the macros
// ---------------------------------------------------------------------------------------------------------------------------------

void        m_setOwner(const char *file, const unsigned int line, const char *func);

// ---------------------------------------------------------------------------------------------------------------------------------
// Allocation breakpoints
// ---------------------------------------------------------------------------------------------------------------------------------

void		m_breakOnAllocation(unsigned int count);
void        m_breakOnRealloc(void *reportedAddress, bool set);
void        m_breakOnDealloc(void *reportedAddress, bool set);

void        m_alwaysValidateAll(bool set);  // Force a validation of all allocation units each time we enter this software
void        m_alwaysLogAll(bool set);       // Force a log of every allocation & deallocation into memory.log
void        m_alwaysWipeAll(bool set);      // Force this software to always wipe memory with a pattern when it is being allocated/deallocated
void        m_randomeWipe(bool set);        // Force this software to use a random pattern when wiping memory -- good for stress testing

#if defined(__cplusplus)
}

bool        &m_breakOnRealloc(void *reportedAddress);
bool        &m_breakOnDealloc(void *reportedAddress);

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Flags & options -- Call these routines to enable/disable the following options
// ---------------------------------------------------------------------------------------------------------------------------------

bool        &m_alwaysValidateAll(); // Force a validation of all allocation units each time we enter this software
bool        &m_alwaysLogAll();      // Force a log of every allocation & deallocation into memory.log
bool        &m_alwaysWipeAll();     // Force this software to always wipe memory with a pattern when it is being allocated/deallocated
bool        &m_randomeWipe();       // Force this software to use a random pattern when wiping memory -- good for stress testing

extern "C" {

#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// The meat of the memory tracking software
// ---------------------------------------------------------------------------------------------------------------------------------

void        *m_allocator(const char *sourceFile, const unsigned int sourceLine, const char *sourceFunc, const unsigned int allocationType, const size_t reportedSize);
void        *m_reallocator(const char *sourceFile, const unsigned int sourceLine, const char *sourceFunc, const unsigned int reallocationType, const size_t reportedSize, void *reportedAddress);
void        m_deallocator(const char *sourceFile, const unsigned int sourceLine, const char *sourceFunc, const unsigned int deallocationType, const void *reportedAddress);

// strdup
char        *m_allocatorCpy(const char *sourceFile, const unsigned int sourceLine, const char *sourceFunc, const unsigned int allocationType, const char *_str);

// ---------------------------------------------------------------------------------------------------------------------------------
// Utilitarian functions
// ---------------------------------------------------------------------------------------------------------------------------------

bool        m_validateAddress(const void *reportedAddress);
bool        m_validateAllocUnit(const sAllocUnit *allocUnit);
bool        m_validateAllAllocUnits();

// ---------------------------------------------------------------------------------------------------------------------------------
// Unused RAM calculations
// ---------------------------------------------------------------------------------------------------------------------------------

unsigned int    m_calcUnused(const sAllocUnit *allocUnit);
unsigned int    m_calcAllUnused();

// ---------------------------------------------------------------------------------------------------------------------------------
// Logging and reporting
// ---------------------------------------------------------------------------------------------------------------------------------

#if !defined(__cplusplus)
void        m_dumpAllocUnit(const sAllocUnit *allocUnit, const char *prefix);
void        m_dumpMemoryReport(const char *filename, const bool overwrite);
#else
void        m_dumpAllocUnit(const sAllocUnit *allocUnit, const char *prefix = "");
void        m_dumpMemoryReport(const char *filename = "memreport.log", const bool overwrite = true);
#endif
sMStats     m_getMemoryStatistics();

#if defined(__cplusplus)
} // end extern "C"
#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// Variations of global operators new & delete
// ---------------------------------------------------------------------------------------------------------------------------------

#if defined(__cplusplus)
void    *operator new(size_t reportedSize);
void    *operator new[](size_t reportedSize);
void    *operator new(size_t reportedSize, const char *sourceFile, int sourceLine);
void    *operator new[](size_t reportedSize, const char *sourceFile, int sourceLine);
void    operator delete(void *reportedAddress);
void    operator delete[](void *reportedAddress);
#endif

#endif // _H_MMGR

// ---------------------------------------------------------------------------------------------------------------------------------
// Macros -- "Kids, please don't try this at home. We're trained professionals here." :)
// ---------------------------------------------------------------------------------------------------------------------------------

#include "nommgr.h"

#define new             (m_setOwner   (__FILE__, __LINE__, __FUNCTION__), false) ? nullptr : new
#define delete          (m_setOwner   (__FILE__, __LINE__, __FUNCTION__), false) ? m_setOwner("", 0, "") : delete
#define malloc(sz)      m_allocator   (__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, sz)
#define calloc(sz)      m_allocator   (__FILE__, __LINE__, __FUNCTION__, m_alloc_calloc, sz)
#define realloc(ptr,sz) m_reallocator (__FILE__, __LINE__, __FUNCTION__, m_alloc_realloc, sz, ptr)
#define free(ptr)       m_deallocator (__FILE__, __LINE__, __FUNCTION__, m_alloc_free, ptr)
#define strdup(str)		m_allocatorCpy(__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, str)
#define _strdup(str)	m_allocatorCpy(__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, str)

// ---------------------------------------------------------------------------------------------------------------------------------
// mmgr.h - End of file
// ---------------------------------------------------------------------------------------------------------------------------------
