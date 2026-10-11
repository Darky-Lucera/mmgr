// ---------------------------------------------------------------------------------------------------------------------------------
/*
//
//
//  _ __ ___  _ __ ___   __ _ _ __      ___ _ __  _ __
// | '_ ` _ \| '_ ` _ \ / _` | '__|    / __| '_ \| '_ \
// | | | | | | | | | | | (_| | |    _ | (__| |_) | |_) |
// |_| |_| |_|_| |_| |_|\__, |_|   (_) \___| .__/| .__/
//                       __/ |             | |   | |
//                      |___/              |_|   |_|
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
//
// !!IMPORTANT!!
//
// This software is self-documented with periodic comments. Before you start using this software, perform a search for the string
// "-DOC-" to locate pertinent information about how to use this software.
//
// You are also encouraged to read the comment blocks throughout this source file. They will help you understand how this memory
// tracking software works, so you can better utilize it within your applications.
//
// NOTES:
//
// 1. If you get compiler errors having to do with set_new_handler, then go through this source and search/replace
//    "std::set_new_handler" with "set_new_handler".
//
// 2. This code purposely uses no external routines that allocate RAM (other than the raw allocation routines, such as malloc). We
//    do this because we want this to be as self-contained as possible. As an example, we don't use assert, because when running
//    under WIN32, the assert brings up a dialog box, which allocates RAM. Doing this in the middle of an allocation would be bad.
//
// 3. When trying to override new/delete under MFC (which has its own version of global new/delete) the linker will complain. In
//    order to fix this error, use the compiler option: /FORCE, which will force it to build an executable even with linker errors.
//    Be sure to check those errors each time you compile, otherwise, you may miss a valid linker error.
//
// 4. If you see something that looks odd to you or seems like a strange way of going about doing something, then consider that this
//    code was carefully thought out. If something looks odd, then just assume I've got a good reason for doing it that way (an
//    example is the use of the class MemStaticTimeTracker.)
//
// 5. With MFC applications, you will need to comment out any occurance of "#define new DEBUG_NEW" from all source files.
//
// 6. Include file dependencies are _very_important_ for getting the MMGR to integrate nicely into your application. Be careful if
//    you're including standard includes from within your own project inclues; that will break this very specific dependency order.
//    It should look like this:
//
//      #include <stdio.h>   // Standard includes MUST come first
//      #include <stdlib.h>  //
//      #include <streamio>  //
//
//      #include "mmgr.h"    // mmgr.h MUST come next
//
//      #include "myfile1.h" // Project includes MUST come last
//      #include "myfile2.h" //
//      #include "myfile3.h" //
//
// ---------------------------------------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
// Older C libraries, such as the one of DJGPP, only define the PRIu32 and PRIu64 macros in C++ when this is defined
#if !defined(__STDC_FORMAT_MACROS)
    #define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <new>
#if !defined(__DJGPP__)
    #include <mutex>
#endif

#include "mmgr.h"

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- If you're like me, it's hard to gain trust in foreign code. This memory manager will try to INDUCE your code to crash (for
// very good reasons... like making bugs obvious as early as possible.) Some people may be inclined to remove this memory tracking
// software if it causes crashes that didn't exist previously. In reality, these new crashes are the BEST reason for using this
// software!
//
// Whether this software causes your application to crash, or if it reports errors, you need to be able to TRUST this software. To
// this end, you are given some very simple debugging tools.
//
// The quickest way to locate problems is to enable the STRESS_TEST macro (below.) This should catch 95% of the crashes before they
// occur by validating every allocation each time this memory manager performs an allocation function. If that doesn't work, keep
// reading...
//
// If you enable the TEST_MEMORY_MANAGER #define (below), this memory manager will log an entry in the memory.log file each time it
// enters and exits one of its primary allocation handling routines. Each call that succeeds should place an "ENTER" and an "EXIT"
// into the log. If the program crashes within the memory manager, it will log an "ENTER", but not an "EXIT". The log will also
// report the name of the routine.
//
// Just because this memory manager crashes does not mean that there is a bug here! First, an application could inadvertantly damage
// the heap, causing malloc(), realloc() or free() to crash. Also, an application could inadvertantly damage some of the memory used
// by this memory tracking software, causing it to crash in much the same way that a damaged heap would affect the standard
// allocation routines.
//
// In the event of a crash within this code, the first thing you'll want to do is to locate the actual line of code that is
// crashing. You can do this by adding log() entries throughout the routine that crashes, repeating this process until you narrow
// in on the offending line of code. If the crash happens in a standard C allocation routine (i.e. malloc, realloc or free) don't
// bother contacting me, your application has damaged the heap. You can help find the culprit in your code by enabling the
// STRESS_TEST macro (below.)
//
// If you truely suspect a bug in this memory manager (and you had better be sure about it! :) you can contact me at
// midnight@FluidStudios.com. Before you do, however, check for a newer version at:
//
//  http://www.FluidStudios.com/publications.html
//
// When using this debugging aid, make sure that you are NOT setting the alwaysLogAll variable on, otherwise the log could be
// cluttered and hard to read.
// ---------------------------------------------------------------------------------------------------------------------------------

//#define   TEST_MEMORY_MANAGER

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Enable this sucker if you really want to stress-test your app's memory usage, or to help find hard-to-find bugs
// ---------------------------------------------------------------------------------------------------------------------------------

//#define   STRESS_TEST

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Enable this sucker if you want to stress-test your app's error-handling. Set RANDOM_FAIL to the percentage of failures you
//       want to test with (0 = none, >100 = all failures).
// ---------------------------------------------------------------------------------------------------------------------------------

//#define   RANDOM_FAILURE 10.0

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Locals -- modify these flags to suit your needs
// ---------------------------------------------------------------------------------------------------------------------------------

#ifdef  STRESS_TEST
static const uint32_t      hashBits               = 12;
static bool                randomWipe             = true;
static bool                alwaysValidateAll      = true;
static bool                alwaysLogAll           = true;
static bool                alwaysWipeAll          = true;
static bool                cleanupLogOnFirstRun   = true;
static const uint32_t      paddingSize            = 1024; // An extra 8K per allocation!
#else
static const uint32_t      hashBits               = 12;
static bool                randomWipe             = false;
static bool                alwaysValidateAll      = false;
static bool                alwaysLogAll           = false;
static bool                alwaysWipeAll          = true;
static bool                cleanupLogOnFirstRun   = true;
static const uint32_t      paddingSize            = 4;
#endif
static bool                stopOnErrors           = true;

// ---------------------------------------------------------------------------------------------------------------------------------
// We define our own assert, because we don't want to bring up an assertion dialog, since that allocates RAM. Our new assert
// simply declares a forced breakpoint.
//
// The BEOS assert added by Arvid Norberg <arvid@iname.com>.
// ---------------------------------------------------------------------------------------------------------------------------------

#ifdef WIN32
//#ifdef  _DEBUG
//#define m_assert(x) if ((x) == false) __asm { int 3 }
//#else
//#define m_assert(x) {}
//#endif
    #define m_assertAlways(x) assert(x)
#elif defined(__BEOS__)
    #ifdef DEBUG
        extern void debugger(const char *message);
        #define m_assertAlways(x) do { if ((x) == false) { debugger("mmgr: assert failed"); } } while (false)
    #else
        #define m_assertAlways(x) do { } while (false)
    #endif
#else // Linux uses assert, which we can use safely, since it doesn't bring up a dialog within the program.
    #define m_assertAlways(cond) assert(cond)
#endif

#if defined(NDEBUG)
    // assert() is empty here, but the variables in the condition must still count as used for newer compilers
    #undef m_assertAlways
    #define m_assertAlways(x) ((void)sizeof(x))
#endif

// With m_setStopOnErrors(false), the check still runs (some checks log the error themselves), but a failure does not stop
// the program. m_assertAlways is for the breakpoints that the user asks for, which must stop anyway.
#define m_assert(x)            \
    do {                       \
        if (stopOnErrors) {    \
            m_assertAlways(x); \
        }                      \
        else {                 \
            (void)(x);         \
        }                      \
    } while (false)

// ---------------------------------------------------------------------------------------------------------------------------------
// Here, we turn off our macros because any place in this source file where the word 'new' or the word 'delete' (etc.)
// appear will be expanded by the macro. So to avoid problems using them within this source file, we'll just #undef them.
// ---------------------------------------------------------------------------------------------------------------------------------

#undef new
#undef delete
#undef malloc
#undef calloc
#undef realloc
#undef free

// ---------------------------------------------------------------------------------------------------------------------------------
// Multi thread support
// ---------------------------------------------------------------------------------------------------------------------------------

#if defined(__DJGPP__)

// DJGPP has no threads (and no std::recursive_mutex), so there is nothing to lock.
class LockGuard {
public:
    LockGuard() {
    }
};

    // GCC accepts thread_local here, but emulates it with a function call on each access
    #define MMGR_THREAD_LOCAL

#else

// The order in which the statics of different files are built and destroyed is not fixed, so a static object of another
// file can allocate before the statics of this file are built, or release a block after they are destroyed. A bool needs
// no constructor or destructor, and the mutex is built on first use and never destroyed, so both are valid at any time.
static bool multithread = false;

static std::recursive_mutex &
realMutex() {
    // Placement new, because a normal new would go through mmgr and show up as a leak
    alignas(std::recursive_mutex) static uint8_t storage[sizeof(std::recursive_mutex)];
    static std::recursive_mutex *                mutex = new (storage) std::recursive_mutex;
    return *mutex;
}

class LockGuard {
public:
    LockGuard() : mutex(multithread ? &realMutex() : nullptr) {
        if (mutex) {
            mutex->lock();
        }
    }

    ~LockGuard() {
        if (mutex) {
            mutex->unlock();
        }
    }

    LockGuard(const LockGuard &)             = delete;
    LockGuard & operator=(const LockGuard &) = delete;

protected:
    // The lock remembers whether it locked, in case m_enableMultithread() is called while it is alive
    std::recursive_mutex * mutex;
};

    #define MMGR_THREAD_LOCAL thread_local

#endif

// ---------------------------------------------------------------------------------------------------------------------------------

void
m_enableMultithread() {
#if !defined(__DJGPP__)
    // Built now, while there is only one thread, so that it does not depend on the thread-safe init of local statics,
    // which -fno-threadsafe-statics and /Zc:threadSafeInit- turn off
    realMutex();
    multithread = true;
#endif
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Defaults for the constants & statics in the MemoryManager class
// ---------------------------------------------------------------------------------------------------------------------------------

const uint32_t          m_alloc_unknown         = 0;
const uint32_t          m_alloc_new             = 1;
const uint32_t          m_alloc_new_array       = 2;
const uint32_t          m_alloc_malloc          = 3;
const uint32_t          m_alloc_calloc          = 4;
const uint32_t          m_alloc_realloc         = 5;
const uint32_t          m_alloc_delete          = 6;
const uint32_t          m_alloc_delete_array    = 7;
const uint32_t          m_alloc_free            = 8;
const uint32_t          m_alloc_aligned_malloc  = 9;
const uint32_t          m_alloc_aligned_realloc = 10;
const uint32_t          m_alloc_aligned_free    = 11;

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Get to know these values. They represent the values that will be used to fill unused and deallocated RAM.
// ---------------------------------------------------------------------------------------------------------------------------------

static uint32_t         prefixPattern           = 0xbaadf00d; // Fill pattern for bytes preceeding allocated blocks
static uint32_t         postfixPattern          = 0xdeadc0de; // Fill pattern for bytes following allocated blocks
static uint32_t         unusedPattern           = 0xfeedface; // Fill pattern for freshly allocated blocks
static uint32_t         releasedPattern         = 0xdeadbeef; // Fill pattern for deallocated blocks

// ---------------------------------------------------------------------------------------------------------------------------------
// Other locals
// ---------------------------------------------------------------------------------------------------------------------------------

static const uint32_t   hashSize                = 1 << hashBits;
static const char *     allocationTypes[]      = {
    "Unknown",
    "new",
    "new[]",
    "malloc",
    "calloc",
    "realloc",
    "delete",
    "delete[]",
    "free",
    "amalloc",
    "arealloc",
    "afree"
};
static sAllocUnit *     hashTable[hashSize];
static sAllocUnit *     reservoir;
static uint64_t         currentAllocationCount  = 0;
static uint64_t         breakOnAllocationCount  = 0;
static uint64_t         failOnAllocationRequest = 0;
static uint64_t         allocationRequests      = 0;
static sMStats          stats;
static bool             staticDeinitTime        = false;
static sAllocUnit **    reservoirBuffer         = nullptr;
static uint32_t         reservoirBufferSize     = 0;
static char             memoryLogFile[1024]     = "memory.log";
static char             memoryLeakLogFile[1024] = "memleaks.log";
static m_logFunction    logFunction             = m_logToStdout;

// The macros call m_setOwner() just before new and delete, outside the lock, so each thread needs its own owner
static MMGR_THREAD_LOCAL const char *   ownerFile = "??";
static MMGR_THREAD_LOCAL const char *   ownerFunc = "??";
static MMGR_THREAD_LOCAL uint32_t       ownerLine = 0;

static void doCleanupLogOnFirstRun();

// ---------------------------------------------------------------------------------------------------------------------------------

// Writes a line of the logs to its file, when the file is open, and to the log function, when there is one
static void
writeLogLine(const m_logKind kind, FILE *fp, const char *line) {
    if (fp) {
        fprintf(fp, "%s\r\n", line);
    }

    if (logFunction) {
        logFunction(kind, line);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void
reportLine(const m_logKind kind, FILE *fp, const char *format, ...) {
    static char buffer[2048];
    va_list     ap;

    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    writeLogLine(kind, fp, buffer);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Local functions only
// ---------------------------------------------------------------------------------------------------------------------------------

static void
log(const char *format, ...) {
    // Cleanup the log?

    if (cleanupLogOnFirstRun) {
        doCleanupLogOnFirstRun();
    }

    // Build the buffer

    static char buffer[2048];
    va_list     ap;

    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    // Open the log file

    FILE * fp = memoryLogFile[0] ? fopen(memoryLogFile, "ab") : nullptr;

    // Spit out the data to the log

    writeLogLine(m_log_memory, fp, buffer);
    if (fp) {
        fclose(fp);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void
doCleanupLogOnFirstRun() {
    if (cleanupLogOnFirstRun) {
        remove(memoryLogFile);
        cleanupLogOnFirstRun = false;

        // The header describes the file, so it does not go to the log function
        m_logFunction function = logFunction;
        logFunction            = nullptr;

        // Print a header for the log

        time_t t = time(nullptr);
        log("--------------------------------------------------------------------------------");
        log("");
        log("      %s - Memory logging file created on %s", memoryLogFile, asctime(localtime(&t)));
        log("--------------------------------------------------------------------------------");
        log("");
        log("This file contains a log of all memory operations performed during the last run.");
        log("");
        log("Interrogate this file to track errors or to help track down memory-related");
        log("issues. You can do this by tracing the allocations performed by a specific owner");
        log("or by tracking a specific address through a series of allocations and");
        log("reallocations.");
        log("");
        log("There is a lot of useful information here which, when used creatively, can be");
        log("extremely helpful.");
        log("");
        log("Note that the following guides are used throughout this file:");
        log("");
        log("   [!] - Error");
        log("   [+] - Allocation");
        log("   [~] - Reallocation");
        log("   [-] - Deallocation");
        log("   [I] - Generic information");
        log("   [F] - Failure induced for the purpose of stress-testing your application");
        log("   [D] - Information used for debugging this memory manager");
        log("");
        log("...so, to find all errors in the file, search for \"[!]\"");
        log("");
        log("--------------------------------------------------------------------------------");

        logFunction = function;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

static const char *
sourceFileStripper(const char *sourceFile) {
    const char * ptr = strrchr(sourceFile, '\\');
    if (ptr) {
        return ptr + 1;
    }

    ptr = strrchr(sourceFile, '/');
    if (ptr) {
        return ptr + 1;
    }

    return sourceFile;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static const char *
ownerString(const char *sourceFile, const uint32_t sourceLine, const char *sourceFunc) {
    static char str[90];
    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%s(%5" PRIu32 ")::%s", sourceFileStripper(sourceFile), sourceLine, sourceFunc);
    return str;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static const char *
insertCommas(uint64_t value) {
    static char str[30];
    char        digits[24];
    snprintf(digits, sizeof(digits), "%" PRIu64, value);

    size_t length = strlen(digits);
    char * out    = str;
    for (size_t i = 0; i < length; i++) {
        if (i > 0 && (length - i) % 3 == 0) {
            *out++ = ',';
        }
        *out++ = digits[i];
    }
    *out = '\0';

    return str;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static const char *
memorySizeString(uint64_t size) {
    static char str[90];

    if (size > (1024 * 1024)) {
        snprintf(str, sizeof(str), "%10s (%7.2fM)", insertCommas(size), static_cast<float>(size) / (1024.0f * 1024.0f));
    }
    else if (size > 1024) {
        snprintf(str, sizeof(str), "%10s (%7.2fK)", insertCommas(size), static_cast<float>(size) / 1024.0f);
    }
    else {
        snprintf(str, sizeof(str), "%10s bytes     ", insertCommas(size));
    }

    return str;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static sAllocUnit *
findAllocUnit(const void *reportedAddress) {
    // Just in case...
    m_assert(reportedAddress != nullptr);

    // Use the address to locate the hash index. Note that we shift off the lower four bits. This is because most allocated
    // addresses will be on four-, eight- or even sixteen-byte boundaries. If we didn't do this, the hash index would not have
    // very good coverage.

    size_t       hashIndex = (reinterpret_cast<size_t>(const_cast<void *>(reportedAddress)) >> 4) & (hashSize - 1);
    sAllocUnit * ptr       = hashTable[hashIndex];
    while (ptr) {
        if (ptr->reportedAddress == reportedAddress) {
            return ptr;
        }

        ptr = ptr->next;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static size_t
calculateActualSize(const size_t reportedSize) {
    return reportedSize + paddingSize * sizeof(uint32_t) * 2;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static size_t
calculateReportedSize(const size_t actualSize) {
    return actualSize - paddingSize * sizeof(uint32_t) * 2;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void *
calculateReportedAddress(void *actualAddress, const size_t alignment = 0) {
    // We allow this...

    if (!actualAddress) {
        return nullptr;
    }

    // JUst account for the padding

    uintptr_t address = reinterpret_cast<uintptr_t>(actualAddress) + sizeof(uint32_t) * paddingSize;
    if (alignment) {
        address = (address + alignment - 1) & ~static_cast<uintptr_t>(alignment - 1);
    }

    return reinterpret_cast<void *>(address);
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void
wipeWithPattern(sAllocUnit *allocUnit, uint32_t pattern, const size_t originalReportedSize = 0) {
    // For a serious test run, we use wipes of random a random value. However, if this causes a crash, we don't want it to
    // crash in a differnt place each time, so we specifically DO NOT call srand. If, by chance your program calls srand(),
    // you may wish to disable that when running with a random wipe test. This will make any crashes more consistent so they
    // can be tracked down easier.

    if (randomWipe) {
        pattern = ((rand() & 0xff) << 24) | ((rand() & 0xff) << 16) | ((rand() & 0xff) << 8) | (rand() & 0xff);
    }

    // -DOC- We should wipe with 0's if we're not in debug mode, so we can help hide bugs if possible when we release the
    // product. So uncomment the following line for releases.
    //
    // Note that the "alwaysWipeAll" should be turned on for this to have effect, otherwise it won't do much good. But we'll
    // leave it this way (as an option) because this does slow things down.
    //  pattern = 0;

    // This part of the operation is optional

    if (alwaysWipeAll && allocUnit->reportedSize > originalReportedSize) {
        // Fill the bulk

        uint32_t * lptr = reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(allocUnit->reportedAddress) + originalReportedSize);
        size_t     length = allocUnit->reportedSize - originalReportedSize;
        size_t     i;
        for (i = 0; i < (length >> 2); i++, lptr++) {
            *lptr = pattern;
        }

        // Fill the remainder

        uint32_t shiftCount = 0;
        char *   cptr       = reinterpret_cast<char *>(lptr);
        for (i = 0; i < (length & 0x3); i++, cptr++, shiftCount += 8) {
            *cptr = static_cast<char>((pattern & (0xffu << shiftCount)) >> shiftCount);
        }
    }

    // Write in the prefix/postfix bytes

    // Measured from the reported address because aligned allocations leave a gap between it and the actual address.
    uint32_t * pre =
        reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(allocUnit->reportedAddress) - paddingSize * sizeof(uint32_t));
    uint32_t * post = reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(allocUnit->reportedAddress) + allocUnit->reportedSize);
    for (uint32_t i = 0; i < paddingSize; i++, pre++, post++) {
        *pre  = prefixPattern;
        *post = postfixPattern;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void
dumpAllocations(const m_logKind kind, FILE *fp) {
    // An address takes as many hex digits as a pointer, so that the columns line up on 32-bit and on 64-bit platforms
    const int    digits = static_cast<int>(2 * sizeof(void *));
    const int    column = digits + 2;
    const char * dashes = "---------------------------------------------------";

    reportLine(kind, fp, "%-6s %-*s %-10s %-*s %-10s %-10s %-8s %-7s %s",
        "Alloc.",
        column, "Addr",
        "Size",
        column, "Addr",
        "Size",
        "",
        "",
        "BreakOn",
        "BreakOn"
    );
    reportLine(kind, fp, "%-6s %-*s %-10s %-*s %-10s %-10s %-8s %-7s %-7s %s",
        "Number",
        column, "Reported",
        "Reported",
        column, "Actual",
        "Actual",
        "Unused",
        "Method",
        "Dealloc",
        "Realloc",
        "Allocated by"
    );
    reportLine(kind, fp, "%.6s %.*s %.10s %.*s %.10s %.10s %.8s %.7s %.7s %s",
        dashes,
        column, dashes,
        dashes,
        column, dashes,
        dashes,
        dashes,
        dashes,
        dashes,
        dashes,
        dashes
    );

    for (uint32_t i = 0; i < hashSize; i++) {
        sAllocUnit * ptr = hashTable[i];
        while (ptr) {
            reportLine(kind, fp, "%06" PRIu64 " 0x%0*zX 0x%08zX 0x%0*zX 0x%08zX 0x%08zX %-8s    %c       %c    %s",
                ptr->allocationNumber,
                digits, reinterpret_cast<size_t>(ptr->reportedAddress),
                ptr->reportedSize,
                digits, reinterpret_cast<size_t>(ptr->actualAddress),
                ptr->actualSize,
                m_calcUnused(ptr),
                allocationTypes[ptr->allocationType],
                ptr->breakOnDealloc ? 'Y' : 'N',
                ptr->breakOnRealloc ? 'Y' : 'N',
                ownerString(ptr->sourceFile, ptr->sourceLine, ptr->sourceFunc)
            );
            ptr = ptr->next;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void
dumpLeakReport() {
    // Open the report file

    FILE * fp = memoryLeakLogFile[0] ? fopen(memoryLeakLogFile, "w+b") : nullptr;

    // Any leaks?

    // Header

    static char timeString[25];
    memset(timeString, 0, sizeof(timeString));
    time_t      t   = time(nullptr);
    struct tm * tme = localtime(&t);
    reportLine(
        m_log_leaks,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(
        m_log_leaks,
        fp,
        "|                                          Memory leak report for:  %02d/%02d/%04d %02d:%02d:%02d                                            |",
        tme->tm_mon + 1,
        tme->tm_mday,
        tme->tm_year + 1900,
        tme->tm_hour,
        tme->tm_min,
        tme->tm_sec
    );
    reportLine(
        m_log_leaks,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(m_log_leaks, fp, "");
    reportLine(m_log_leaks, fp, "");
    if (stats.totalAllocUnitCount) {
        reportLine(
            m_log_leaks,
            fp,
            "%" PRIu64 " memory leak%s found:",
            stats.totalAllocUnitCount,
            stats.totalAllocUnitCount == 1 ? "" : "s"
        );
    }
    else {
        reportLine(m_log_leaks, fp, "Congratulations! No memory leaks found!");

        // We can finally free up our own memory allocations

        if (reservoirBuffer) {
            for (uint32_t i = 0; i < reservoirBufferSize; i++) {
                free(reservoirBuffer[i]);
            }
            free(reservoirBuffer);
            reservoirBuffer     = 0;
            reservoirBufferSize = 0;
            reservoir           = nullptr;
        }
    }
    reportLine(m_log_leaks, fp, "");

    if (stats.totalAllocUnitCount) {
        dumpAllocations(m_log_leaks, fp);
    }

    if (fp) {
        fclose(fp);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// We use a static class to let us know when we're in the midst of static deinitialization
// ---------------------------------------------------------------------------------------------------------------------------------

class MemStaticTimeTracker {
public:
    // The header of the log waits for its first line, so that m_setLogFiles() can still change the name of the file
    MemStaticTimeTracker() {
        remove(memoryLogFile);
    }

    ~MemStaticTimeTracker() {
        staticDeinitTime = true;
        dumpLeakReport();
    }
};

static MemStaticTimeTracker mstt;

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Flags & options -- Call these routines to enable/disable the following options
// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setAlwaysValidateAll(bool set) {
    LockGuard lock;

    // Force a validation of all allocation units each time we enter this software
    alwaysValidateAll = set;
}

bool
m_getAlwaysValidateAll(void) {
    LockGuard lock;
    return alwaysValidateAll;
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setAlwaysLogAll(bool set) {
    LockGuard lock;

    // Force a log of every allocation & deallocation into memory.log
    alwaysLogAll = set;
}

bool
m_getAlwaysLogAll(void) {
    LockGuard lock;
    return alwaysLogAll;
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setAlwaysWipeAll(bool set) {
    LockGuard lock;

    // Force this software to always wipe memory with a pattern when it is being allocated/deallocated
    alwaysWipeAll = set;
}

bool
m_getAlwaysWipeAll(void) {
    LockGuard lock;
    return alwaysWipeAll;
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setRandomWipe(bool set) {
    LockGuard lock;

    // Force this software to use a random pattern when wiping memory -- good for stress testing
    randomWipe = set;
}

bool
m_getRandomWipe(void) {
    LockGuard lock;
    return randomWipe;
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setStopOnErrors(bool set) {
    LockGuard lock;
    stopOnErrors = set;
}

bool
m_getStopOnErrors(void) {
    LockGuard lock;
    return stopOnErrors;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Simply call this routine with the address of an allocated block of RAM, to cause it to force a breakpoint when it is
// reallocated.
// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setBreakOnRealloc(const void *reportedAddress, bool set) {
    LockGuard lock;

    // Locate the existing allocation unit

    sAllocUnit *au = findAllocUnit(reportedAddress);

    // If you hit this assert, you tried to set a breakpoint on reallocation for an address that doesn't exist. Interrogate the
    // stack frame or the variable 'au' to see which allocation this is.
    m_assert(au != nullptr);
    if (au == nullptr) {
        log("[!] Breakpoint on reallocation of RAM that was never allocated: addr 0x%08zX",
            reinterpret_cast<size_t>(const_cast<void *>(reportedAddress))
        );
        return;
    }

    // If you hit this assert, you tried to set a breakpoint on reallocation for an address that wasn't allocated in a way that
    // is compatible with reallocation.
    m_assert(
        au->allocationType == m_alloc_malloc ||
        au->allocationType == m_alloc_calloc ||
        au->allocationType == m_alloc_realloc ||
        au->allocationType == m_alloc_aligned_malloc ||
        au->allocationType == m_alloc_aligned_realloc
    );

    au->breakOnRealloc = set;
}

bool
m_getBreakOnRealloc(const void *reportedAddress) {
    LockGuard          lock;
    const sAllocUnit * au = findAllocUnit(reportedAddress);

    return au != nullptr && au->breakOnRealloc;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Simply call this routine with the address of an allocated block of RAM, to cause it to force a breakpoint when it is
// deallocated.
// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setBreakOnDealloc(const void *reportedAddress, bool set) {
    LockGuard lock;

    // Locate the existing allocation unit

    sAllocUnit * au = findAllocUnit(reportedAddress);

    // If you hit this assert, you tried to set a breakpoint on deallocation for an address that doesn't exist. Interrogate the
    // stack frame or the variable 'au' to see which allocation this is.
    m_assert(au != nullptr);
    if (au == nullptr) {
        log("[!] Breakpoint on deallocation of RAM that was never allocated: addr 0x%08zX",
            reinterpret_cast<size_t>(const_cast<void *>(reportedAddress))
        );
        return;
    }

    au->breakOnDealloc = set;
}

bool
m_getBreakOnDealloc(const void *reportedAddress) {
    LockGuard          lock;
    const sAllocUnit * au = findAllocUnit(reportedAddress);
    return au != nullptr && au->breakOnDealloc;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- When tracking down a difficult bug, use this routine to force a breakpoint on a specific allocation count
// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setBreakOnAllocation(uint64_t count) {
    LockGuard lock;
    breakOnAllocationCount = count;
}

uint64_t
m_getBreakOnAllocation(void) {
    LockGuard lock;
    return breakOnAllocationCount;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Failure injection, to test the code that handles a failed allocation
// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setFailOnAllocation(uint64_t count) {
    LockGuard lock;
    failOnAllocationRequest = count;
    allocationRequests      = 0;
}

uint64_t
m_getFailOnAllocation(void) {
    LockGuard lock;
    return failOnAllocationRequest;
}

// ---------------------------------------------------------------------------------------------------------------------------------

uint64_t
m_getAllocationRequestCount(void) {
    LockGuard lock;
    return allocationRequests;
}

// ---------------------------------------------------------------------------------------------------------------------------------

// Counts the request, and tells whether m_setFailOnAllocation() asked for this one to fail
static bool
failThisRequest(const char *file, const uint32_t line, const char *func) {
    ++allocationRequests;
    if (failOnAllocationRequest == 0 || allocationRequests != failOnAllocationRequest) {
        return false;
    }

    log("[F] Allocation request %" PRIu64 " fails, as m_setFailOnAllocation() asked, by %s",
        allocationRequests,
        ownerString(file, line, func)
    );
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Used by the macros
// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setOwner(const char *file, const uint32_t line, const char *func) {
    // You're probably wondering about this...
    //
    // It's important for this memory manager to primarily work with global new/delete in their original forms (i.e. with
    // no extra parameters.) In order to do this, we use macros that call this function prior to operators new & delete. This
    // is fine... usually. Here's what actually happens when you use this macro to delete an object:
    //
    // m_setOwner(__FILE__, __LINE__, __FUNCTION__) --> object::~object() --> delete
    //
    // Note that the compiler inserts a call to the object's destructor just prior to calling our overridden operator delete.
    // But what happens when we delete an object whose destructor deletes another object, whose desctuctor deletes another
    // object? Here's a diagram (indentation follows stack depth):
    //
    // m_setOwner(...) -> ~obj1()                          // original call to delete obj1
    //     m_setOwner(...) -> ~obj2()                      // obj1's destructor deletes obj2
    //         m_setOwner(...) -> ~obj3()                  // obj2's destructor deletes obj3
    //             ...                                     // obj3's destructor just does some stuff
    //         delete                                      // back in obj2's destructor, we call delete
    //     delete                                          // back in obj1's destructor, we call delete
    // delete                                              // back to our original call, we call delete
    //
    // Because m_setOwner() just sets up some static variables (below) it's important that each call to m_setOwner() and
    // successive calls to new/delete alternate. However, in this case, three calls to m_setOwner() happen in succession
    // followed by three calls to delete in succession (with a few calls to destructors mixed in for fun.) This means that
    // only the final call to delete (in this chain of events) will have the proper reporting, and the first two in the chain
    // will not have ANY owner-reporting information. The deletes will still work fine, we just won't know who called us.
    //
    // "Then build a stack, my friend!" you might think... but it's a very common thing that people will be working with third-
    // party libraries (including MFC under Windows) which is not compiled with this memory manager's macros. In those cases,
    // m_setOwner() is never called, and rightfully should not have the proper trace-back information. So if one of the
    // destructors in the chain ends up being a call to a delete from a non-mmgr-compiled library, the stack will get confused.
    //
    // I've been unable to find a solution to this problem, but at least we can detect it and report the data before we
    // lose it. That's what this is all about. It makes it somewhat confusing to read in the logs, but at least ALL the
    // information is present...
    //
    // There's a caveat here... The compiler is not required to call operator delete if the value being deleted is nullptr.
    // In this case, any call to delete with a nullptr will sill call m_setOwner(), which will make m_setOwner() think that
    // there is a destructor chain becuase we setup the variables, but nothing gets called to clear them. Because of this
    // we report a "Possible destructor chain".
    //
    // Thanks to J. Woznack (from Kodiak Interactive Software Studios -- www.kodiakgames.com) for pointing this out.

    if (ownerLine && alwaysLogAll) {
        log("[I] NOTE! Possible destructor chain: previous owner is %s", ownerString(ownerFile, ownerLine, ownerFunc));
    }

    // Okay... save this stuff off so we can keep track of the caller

    ownerFile = file;
    ownerLine = line;
    ownerFunc = func;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void
resetGlobals() {
    ownerFile = "??";
    ownerLine = 0;
    ownerFunc = "??";
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void
reportFailure(const char *error) {
    log("[!] %s", error);
    resetGlobals();
}

// ---------------------------------------------------------------------------------------------------------------------------------

// Without C++ exceptions, a failed new stops the program, as the standard library does
[[noreturn]] static void
throwBadAlloc() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    throw std::bad_alloc();
#else
    abort();
#endif
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Global new/new[]
//
// These are the standard new/new[] operators. They are merely interface functions that operate like normal new/new[], but use our
// memory tracking routines.
// ---------------------------------------------------------------------------------------------------------------------------------

void *
operator new(size_t reportedSize) {
    LockGuard lock;

#ifdef TEST_MEMORY_MANAGER
    log("[D] ENTER: new");
#endif

    // Save these off...

    const char *   file = ownerFile;
    const uint32_t line = ownerLine;
    const char *   func = ownerFunc;

    // ANSI says: allocation requests of 0 bytes will still return a valid value

    if (reportedSize == 0) {
        reportedSize = 1;
    }

    // ANSI says: loop continuously because the error handler could possibly free up some memory

    for (;;) {
        // Try the allocation

        void * ptr = m_allocator(file, line, func, m_alloc_new, reportedSize);
        if (ptr) {
#ifdef TEST_MEMORY_MANAGER
            log("[D] EXIT : new");
#endif
            return ptr;
        }

        // There isn't a way to determine the new handler, except through setting it. So we'll just set it to nullptr, then
        // set it back again.

        std::new_handler nh = std::set_new_handler(0);
        std::set_new_handler(nh);

        // If there is an error handler, call it

        if (nh) {
            (*nh)();
        }

        // Otherwise, throw std::bad_alloc, or abort if the code is built without exceptions

        else {
#ifdef TEST_MEMORY_MANAGER
            log("[D] EXIT : new");
#endif
            throwBadAlloc();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
operator new[](size_t reportedSize) {
    LockGuard lock;

#ifdef TEST_MEMORY_MANAGER
    log("[D] ENTER: new[]");
#endif

    // Save these off...

    const char *   file = ownerFile;
    const uint32_t line = ownerLine;
    const char *   func = ownerFunc;

    // The ANSI standard says that allocation requests of 0 bytes will still return a valid value

    if (reportedSize == 0) {
        reportedSize = 1;
    }

    // ANSI says: loop continuously because the error handler could possibly free up some memory

    for (;;) {
        // Try the allocation

        void * ptr = m_allocator(file, line, func, m_alloc_new_array, reportedSize);
        if (ptr) {
#ifdef TEST_MEMORY_MANAGER
            log("[D] EXIT : new[]");
#endif
            return ptr;
        }

        // There isn't a way to determine the new handler, except through setting it. So we'll just set it to nullptr, then
        // set it back again.

        std::new_handler nh = std::set_new_handler(0);
        std::set_new_handler(nh);

        // If there is an error handler, call it

        if (nh) {
            (*nh)();
        }

        // Otherwise, throw std::bad_alloc, or abort if the code is built without exceptions

        else {
#ifdef TEST_MEMORY_MANAGER
            log("[D] EXIT : new[]");
#endif
            throwBadAlloc();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Other global new/new[]
//
// These are the standard new/new[] operators as used by Microsoft's memory tracker. We don't want them interfering with our memory
// tracking efforts. Like the previous versions, these are merely interface functions that operate like normal new/new[], but use
// our memory tracking routines.
// ---------------------------------------------------------------------------------------------------------------------------------

void *
operator new(size_t reportedSize, const char *sourceFile, uint32_t sourceLine) {
    LockGuard lock;

#ifdef TEST_MEMORY_MANAGER
    log("[D] ENTER: new");
#endif

    // The ANSI standard says that allocation requests of 0 bytes will still return a valid value

    if (reportedSize == 0) {
        reportedSize = 1;
    }

    // ANSI says: loop continuously because the error handler could possibly free up some memory

    for (;;) {
        // Try the allocation

        void * ptr = m_allocator(sourceFile, sourceLine, "??", m_alloc_new, reportedSize);
        if (ptr) {
#ifdef TEST_MEMORY_MANAGER
            log("[D] EXIT : new");
#endif
            return ptr;
        }

        // There isn't a way to determine the new handler, except through setting it. So we'll just set it to nullptr, then
        // set it back again.

        std::new_handler nh = std::set_new_handler(0);
        std::set_new_handler(nh);

        // If there is an error handler, call it

        if (nh) {
            (*nh)();
        }

        // Otherwise, throw std::bad_alloc, or abort if the code is built without exceptions

        else {
#ifdef TEST_MEMORY_MANAGER
            log("[D] EXIT : new");
#endif
            throwBadAlloc();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
operator new[](size_t reportedSize, const char *sourceFile, uint32_t sourceLine) {
    LockGuard lock;

#ifdef TEST_MEMORY_MANAGER
    log("[D] ENTER: new[]");
#endif

    // The ANSI standard says that allocation requests of 0 bytes will still return a valid value

    if (reportedSize == 0) {
        reportedSize = 1;
    }

    // ANSI says: loop continuously because the error handler could possibly free up some memory

    for (;;) {
        // Try the allocation

        void * ptr = m_allocator(sourceFile, sourceLine, "??", m_alloc_new_array, reportedSize);
        if (ptr) {
#ifdef TEST_MEMORY_MANAGER
            log("[D] EXIT : new[]");
#endif
            return ptr;
        }

        // There isn't a way to determine the new handler, except through setting it. So we'll just set it to nullptr, then
        // set it back again.

        std::new_handler nh = std::set_new_handler(0);
        std::set_new_handler(nh);

        // If there is an error handler, call it

        if (nh) {
            (*nh)();
        }

        // Otherwise, throw std::bad_alloc, or abort if the code is built without exceptions

        else {
#ifdef TEST_MEMORY_MANAGER
            log("[D] EXIT : new[]");
#endif
            throwBadAlloc();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Global delete/delete[]
//
// These are the standard delete/delete[] operators. They are merely interface functions that operate like normal delete/delete[],
// but use our memory tracking routines.
// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete(void *reportedAddress) noexcept {
    LockGuard lock;

#ifdef TEST_MEMORY_MANAGER
    log("[D] ENTER: delete");
#endif

    // ANSI says: delete & delete[] allow nullptr pointers (they do nothing)

    if (reportedAddress) {
        m_deallocator(ownerFile, ownerLine, ownerFunc, m_alloc_delete, reportedAddress);
    }
    else if (alwaysLogAll) {
        log("[-] ----- %8s of nullptr                      by %s",
            allocationTypes[m_alloc_delete],
            ownerString(ownerFile, ownerLine, ownerFunc)
        );
    }

    // Resetting the globals insures that if at some later time, somebody calls our memory manager from an unknown
    // source (i.e. they didn't include our H file) then we won't think it was the last allocation.

    resetGlobals();

#ifdef TEST_MEMORY_MANAGER
    log("[D] EXIT : delete");
#endif
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete[](void *reportedAddress) noexcept {
    LockGuard lock;

#ifdef TEST_MEMORY_MANAGER
    log("[D] ENTER: delete[]");
#endif

    // ANSI says: delete & delete[] allow nullptr pointers (they do nothing)

    if (reportedAddress) {
        m_deallocator(ownerFile, ownerLine, ownerFunc, m_alloc_delete_array, reportedAddress);
    }
    else if (alwaysLogAll) {
        log("[-] ----- %8s of nullptr                      by %s",
            allocationTypes[m_alloc_delete_array],
            ownerString(ownerFile, ownerLine, ownerFunc)
        );
    }

    // Resetting the globals insures that if at some later time, somebody calls our memory manager from an unknown
    // source (i.e. they didn't include our H file) then we won't think it was the last allocation.

    resetGlobals();

#ifdef TEST_MEMORY_MANAGER
    log("[D] EXIT : delete[]");
#endif
}

// ---------------------------------------------------------------------------------------------------------------------------------
// nothrow new/new[]/delete/delete[]
//
// The versions of the library should call the ones above, but not all of them do (AddressSanitizer has its own, and so
// had libstdc++ before GCC 9). Without these, mmgr would not see those blocks and would refuse to delete them.
// ---------------------------------------------------------------------------------------------------------------------------------

static void * allocateUnit(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const uint32_t allocationType,
    const size_t   reportedSize,
    const size_t   alignment
);

// Like operator new, but returns nullptr instead of throwing std::bad_alloc when there is no new_handler
static void *
newWithHandler(size_t reportedSize, const size_t alignment, const uint32_t allocationType) {
    LockGuard lock;

    const char *   file = ownerFile;
    const uint32_t line = ownerLine;
    const char *   func = ownerFunc;

    if (reportedSize == 0) {
        reportedSize = 1;
    }

    for (;;) {
        void * ptr = allocateUnit(file, line, func, allocationType, reportedSize, alignment);
        if (ptr) {
            return ptr;
        }

        std::new_handler nh = std::get_new_handler();
        if (!nh) {
            return nullptr;
        }

        (*nh)();
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
operator new(size_t reportedSize, const std::nothrow_t &) noexcept {
    return newWithHandler(reportedSize, 0, m_alloc_new);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
operator new[](size_t reportedSize, const std::nothrow_t &) noexcept {
    return newWithHandler(reportedSize, 0, m_alloc_new_array);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete(void *reportedAddress, const std::nothrow_t &) noexcept {
    operator delete(reportedAddress);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete[](void *reportedAddress, const std::nothrow_t &) noexcept {
    operator delete[](reportedAddress);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Sized delete/delete[] (C++14)
// ---------------------------------------------------------------------------------------------------------------------------------

#if __cplusplus >= 201402L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)
static void
checkDeallocationSize(const void *reportedAddress, size_t reportedSize) {
    if (!reportedAddress) {
        return;
    }

    LockGuard lock;

    // operator new turns requests of 0 bytes into requests of 1 byte
    if (reportedSize == 0) {
        reportedSize = 1;
    }

    const sAllocUnit * au = findAllocUnit(reportedAddress);
    if (!au || au->reportedSize == reportedSize) {
        return;
    }

    log("[!] Deallocation size mismatch: delete of addr 0x%08zX with size %zu, allocated with size %zu, by %s",
        reinterpret_cast<size_t>(const_cast<void *>(reportedAddress)),
        reportedSize,
        au->reportedSize,
        ownerString(ownerFile, ownerLine, ownerFunc)
    );
    m_dumpAllocUnit(au, "  ");

    // If you hit this assert, the size that the compiler passed to delete is not the size that was allocated. The usual cause
    // is deleting an object through a pointer to a base class that has no virtual destructor.
    m_assert(au->reportedSize == reportedSize);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete(void *reportedAddress, size_t reportedSize) noexcept {
    checkDeallocationSize(reportedAddress, reportedSize);
    operator delete(reportedAddress);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete[](void *reportedAddress, size_t reportedSize) noexcept {
    checkDeallocationSize(reportedAddress, reportedSize);
    operator delete[](reportedAddress);
}
#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// Aligned new/new[]/delete/delete[] (C++17)
//
// The compiler uses these for types whose alignment is bigger than __STDCPP_DEFAULT_NEW_ALIGNMENT__ (for example alignas(64)).
// ---------------------------------------------------------------------------------------------------------------------------------

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
void *
operator new(size_t reportedSize, std::align_val_t alignment) {
    void * ptr = newWithHandler(reportedSize, static_cast<size_t>(alignment), m_alloc_new);
    if (!ptr) {
        throwBadAlloc();
    }

    return ptr;
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
operator new[](size_t reportedSize, std::align_val_t alignment) {
    void * ptr = newWithHandler(reportedSize, static_cast<size_t>(alignment), m_alloc_new_array);
    if (!ptr) {
        throwBadAlloc();
    }

    return ptr;
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete(void *reportedAddress, std::align_val_t) noexcept {
    operator delete(reportedAddress);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete[](void *reportedAddress, std::align_val_t) noexcept {
    operator delete[](reportedAddress);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete(void *reportedAddress, size_t reportedSize, std::align_val_t) noexcept {
    checkDeallocationSize(reportedAddress, reportedSize);
    operator delete(reportedAddress);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete[](void *reportedAddress, size_t reportedSize, std::align_val_t) noexcept {
    checkDeallocationSize(reportedAddress, reportedSize);
    operator delete[](reportedAddress);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
operator new(size_t reportedSize, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return newWithHandler(reportedSize, static_cast<size_t>(alignment), m_alloc_new);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
operator new[](size_t reportedSize, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return newWithHandler(reportedSize, static_cast<size_t>(alignment), m_alloc_new_array);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete(void *reportedAddress, std::align_val_t, const std::nothrow_t &) noexcept {
    operator delete(reportedAddress);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
operator delete[](void *reportedAddress, std::align_val_t, const std::nothrow_t &) noexcept {
    operator delete[](reportedAddress);
}
#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// Allocate memory and track it
// ---------------------------------------------------------------------------------------------------------------------------------

static void *
allocateUnit(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const uint32_t allocationType,
    const size_t   reportedSize,
    const size_t   alignment
) {
    LockGuard lock;

    {
#ifdef TEST_MEMORY_MANAGER
        log("[D] ENTER: m_allocator()");
#endif

        // Increase our allocation count

        currentAllocationCount++;

        // Log the request

        if (alwaysLogAll) {
            log("[+] %05" PRIu64 " %8s of size 0x%08zX(%08zu) by %s",
                currentAllocationCount,
                allocationTypes[allocationType],
                reportedSize,
                reportedSize,
                ownerString(sourceFile, sourceLine, sourceFunc)
            );
        }

        // If you hit this assert, you requested a breakpoint on a specific allocation count
        m_assertAlways(currentAllocationCount != breakOnAllocationCount);

        if (failThisRequest(sourceFile, sourceLine, sourceFunc)) {
            resetGlobals();
            return nullptr;
        }

        // If necessary, grow the reservoir of unused allocation units

        if (!reservoir) {
            // Allocate 256 reservoir elements

            reservoir = (sAllocUnit *)malloc(sizeof(sAllocUnit) * 256);

            // If you hit this assert, then the memory manager failed to allocate internal memory for tracking the
            // allocations
            m_assert(reservoir != nullptr);

            // Danger Will Robinson!

            if (reservoir == nullptr) {
                reportFailure("Unable to allocate RAM for internal memory tracking data");
                return nullptr;
            }

            // Build a linked-list of the elements in our reservoir

            memset(reservoir, 0, sizeof(sAllocUnit) * 256);
            for (uint32_t i = 0; i < 256 - 1; i++) {
                reservoir[i].next = &reservoir[i + 1];
            }

            // Add this address to our reservoirBuffer so we can free it later

            sAllocUnit ** temp = (sAllocUnit **)realloc(reservoirBuffer, (reservoirBufferSize + 1) * sizeof(sAllocUnit *));
            m_assert(temp);
            if (temp) {
                reservoirBuffer                        = temp;
                reservoirBuffer[reservoirBufferSize++] = reservoir;
            }
        }

        // Logical flow says this should never happen...
        m_assert(reservoir != nullptr);

        // Grab a new allocaton unit from the front of the reservoir

        sAllocUnit * au = reservoir;
        reservoir       = au->next;

        // Populate it with some real data

        memset(au, 0, sizeof(sAllocUnit));
        // The extra bytes let calculateReportedAddress() move the reported address up to the next aligned boundary.
        au->actualSize = calculateActualSize(reportedSize) + (alignment ? alignment - 1 : 0);
#ifdef RANDOM_FAILURE
        double a = rand();
        double b = RAND_MAX / 100.0 * RANDOM_FAILURE;
        if (a > b) {
            au->actualAddress = malloc(au->actualSize);
        }
        else {
            log("[F] Random faiure");
            au->actualAddress = nullptr;
        }
#else
        au->actualAddress = malloc(au->actualSize);
#endif
        au->reportedSize     = reportedSize;
        au->reportedAddress  = calculateReportedAddress(au->actualAddress, alignment);
        au->allocationType   = allocationType;
        au->sourceLine       = sourceLine;
        au->allocationNumber = currentAllocationCount;
        if (sourceFile) {
            strncpy(au->sourceFile, sourceFileStripper(sourceFile), sizeof(au->sourceFile) - 1);
        }
        else {
            strcpy(au->sourceFile, "??");
        }
        if (sourceFunc) {
            strncpy(au->sourceFunc, sourceFunc, sizeof(au->sourceFunc) - 1);
        }
        else {
            strcpy(au->sourceFunc, "??");
        }

        // We don't want to assert with random failures, because we want the application to deal with them.

#ifndef RANDOM_FAILURE
        // If you hit this assert, then the requested allocation simply failed (you're out of memory.) Interrogate the
        // variable 'au' or the stack frame to see what you were trying to do.
        m_assert(au->actualAddress != nullptr);
#endif

        if (au->actualAddress == nullptr) {
            // The allocation unit goes back to the reservoir, so that the failure does not lose it
            au->next  = reservoir;
            reservoir = au;
            reportFailure("Request for allocation failed. Out of memory.");
            return nullptr;
        }

        // If you hit this assert, then this allocation was made from a source that isn't setup to use this memory tracking
        // software, use the stack frame to locate the source and include our H file.
        m_assert(allocationType != m_alloc_unknown);

        // Insert the new allocation into the hash table

        size_t hashIndex = (reinterpret_cast<size_t>(au->reportedAddress) >> 4) & (hashSize - 1);
        if (hashTable[hashIndex]) {
            hashTable[hashIndex]->prev = au;
        }
        au->next             = hashTable[hashIndex];
        au->prev             = nullptr;
        hashTable[hashIndex] = au;

        // Account for the new allocatin unit in our stats

        stats.totalReportedMemory += au->reportedSize;
        stats.totalActualMemory   += au->actualSize;
        stats.totalAllocUnitCount++;
        if (stats.totalReportedMemory > stats.peakReportedMemory) {
            stats.peakReportedMemory = stats.totalReportedMemory;
        }
        if (stats.totalActualMemory > stats.peakActualMemory) {
            stats.peakActualMemory = stats.totalActualMemory;
        }
        if (stats.totalAllocUnitCount > stats.peakAllocUnitCount) {
            stats.peakAllocUnitCount = stats.totalAllocUnitCount;
        }
        stats.accumulatedReportedMemory += au->reportedSize;
        stats.accumulatedActualMemory   += au->actualSize;
        stats.accumulatedAllocUnitCount++;

        // Prepare the allocation unit for use (wipe it with recognizable garbage)

        wipeWithPattern(au, unusedPattern);

        // calloc() expects the reported memory address range to be filled with 0's

        if (allocationType == m_alloc_calloc) {
            memset(au->reportedAddress, 0, au->reportedSize);
        }

        // Validate every single allocated unit in memory

        if (alwaysValidateAll) {
            m_validateAllAllocUnits();
        }

        // Log the result

        if (alwaysLogAll) {
            log("[+] ---->             addr 0x%08zx", reinterpret_cast<size_t>(au->reportedAddress));
        }

        // Resetting the globals insures that if at some later time, somebody calls our memory manager from an unknown
        // source (i.e. they didn't include our H file) then we won't think it was the last allocation.

        resetGlobals();

        // Return the (reported) address of the new allocation unit

#ifdef TEST_MEMORY_MANAGER
        log("[D] EXIT : m_allocator()");
#endif

        return au->reportedAddress;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
m_allocator(const char *file, const uint32_t line, const char *func, const uint32_t allocationType, const size_t reportedSize) {
    return allocateUnit(file, line, func, allocationType, reportedSize, 0);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// calloc
// ---------------------------------------------------------------------------------------------------------------------------------

void *
m_callocator(const char *file, const uint32_t line, const char *func, const size_t count, const size_t size) {
    // Like calloc(), fail instead of returning a smaller block when count * size does not fit in a size_t
    if (size != 0 && count > SIZE_MAX / size) {
        LockGuard lock;
        log("[!] calloc of %zu elements of size %zu is too big, by %s", count, size, ownerString(file, line, func));
        return nullptr;
    }

    return allocateUnit(file, line, func, m_alloc_calloc, count * size, 0);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// strdup
// ---------------------------------------------------------------------------------------------------------------------------------

char *
m_allocatorCpy(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const uint32_t allocationType,
    const char *   _str
) {
    char * memory;
    size_t size;

    assert(_str != nullptr);
    size = strlen(_str);

    memory = (char *)m_allocator(sourceFile, sourceLine, sourceFunc, allocationType, size + 1);
    if (memory != nullptr) {
        memcpy(memory, _str, size + 1);
    }

    return memory;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Reallocate memory and track it
// ---------------------------------------------------------------------------------------------------------------------------------

// realloc() keeps the bytes at the same distance from the start of the block. When the data of the old block is not at
// the usual distance (it was aligned), or the new block must be aligned, the data is copied to a new block instead.
static void *
resizeActualBlock(const sAllocUnit *au, const size_t newActualSize, const size_t newReportedSize, const size_t alignment) {
    const size_t usualOffset = sizeof(uint32_t) * paddingSize;
    const size_t oldOffset = static_cast<size_t>(static_cast<char *>(au->reportedAddress) - static_cast<char *>(au->actualAddress));
    if (alignment == 0 && oldOffset == usualOffset) {
        return realloc(au->actualAddress, newActualSize);
    }

    void * newActualAddress = malloc(newActualSize);
    if (newActualAddress) {
        const size_t bytes = au->reportedSize < newReportedSize ? au->reportedSize : newReportedSize;
        memcpy(calculateReportedAddress(newActualAddress, alignment), au->reportedAddress, bytes);
        free(au->actualAddress);
    }

    return newActualAddress;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void *
reallocateUnit(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const uint32_t reallocationType,
    const size_t   reportedSize,
    void *         reportedAddress,
    const size_t   alignment
) {
    LockGuard lock;

    {
#ifdef TEST_MEMORY_MANAGER
        log("[D] ENTER: m_reallocator()");
#endif

        // Calling realloc with a nullptr should force same operations as a malloc

        if (!reportedAddress) {
            return allocateUnit(sourceFile, sourceLine, sourceFunc, reallocationType, reportedSize, alignment);
        }

        // Increase our allocation count

        currentAllocationCount++;

        // If you hit this assert, you requested a breakpoint on a specific allocation count
        m_assertAlways(currentAllocationCount != breakOnAllocationCount);

        // Log the request

        if (alwaysLogAll) {
            log("[~] %05" PRIu64 " %8s of size 0x%08zX(%08zu) by %s",
                currentAllocationCount,
                allocationTypes[reallocationType],
                reportedSize,
                reportedSize,
                ownerString(sourceFile, sourceLine, sourceFunc)
            );
        }

        // The old block stays as it was, as with a real failed realloc()
        if (failThisRequest(sourceFile, sourceLine, sourceFunc)) {
            resetGlobals();
            return nullptr;
        }

        // Locate the existing allocation unit

        sAllocUnit * au = findAllocUnit(reportedAddress);

        // If you hit this assert, you tried to reallocate RAM that wasn't allocated by this memory manager.
        m_assert(au != nullptr);
        if (au == nullptr) {
            reportFailure("Request to reallocate RAM that was never allocated");
            return nullptr;
        }

        // If you hit this assert, then the allocation unit that is about to be reallocated is damaged. But you probably
        // already know that from a previous assert you should have seen in validateAllocUnit() :)
        m_assert(m_validateAllocUnit(au));

        // If you hit this assert, then this reallocation was made from a source that isn't setup to use this memory
        // tracking software, use the stack frame to locate the source and include our H file.
        m_assert(reallocationType != m_alloc_unknown);

        const bool  methodsMatch =
            (reallocationType == m_alloc_aligned_realloc) ?
                (au->allocationType == m_alloc_aligned_malloc ||
                 au->allocationType == m_alloc_aligned_realloc) :
                (au->allocationType == m_alloc_malloc ||
                 au->allocationType == m_alloc_calloc ||
                 au->allocationType == m_alloc_realloc);
        if (!methodsMatch) {
            log("[!] Allocation and reallocation methods do not match: %s of a block from %s, by %s",
                allocationTypes[reallocationType],
                allocationTypes[au->allocationType],
                ownerString(sourceFile, sourceLine, sourceFunc)
            );
            m_dumpAllocUnit(au, "  ");
        }

        // If you hit this assert, you were trying to reallocate RAM that was not allocated in a way that is compatible with
        // realloc. In other words, you have a allocation/reallocation mismatch.
        m_assert(methodsMatch);

        // If you hit this assert, then the "break on realloc" flag for this allocation unit is set (and will continue to be
        // set until you specifically shut it off. Interrogate the 'au' variable to determine information about this
        // allocation unit.
        m_assertAlways(au->breakOnRealloc == false);

        // Keep track of the original size

        size_t originalReportedSize = au->reportedSize;

        if (alwaysLogAll) {
            log("[~] ---->             from 0x%08zX(%08zu)", originalReportedSize, originalReportedSize);
        }

        // Do the reallocation

        void * oldReportedAddress = reportedAddress;
        size_t alignmentSlack     = alignment ? alignment - 1 : 0;
        size_t newActualSize      = calculateActualSize(reportedSize) + alignmentSlack;
        void * newActualAddress   = nullptr;
#ifdef RANDOM_FAILURE
        double a = rand();
        double b = RAND_MAX / 100.0 * RANDOM_FAILURE;
        if (a > b) {
            newActualAddress = resizeActualBlock(au, newActualSize, reportedSize, alignment);
        }
        else {
            log("[F] Random faiure");
        }
#else
        newActualAddress = resizeActualBlock(au, newActualSize, reportedSize, alignment);
#endif

        // We don't want to assert with random failures, because we want the application to deal with them.

#ifndef RANDOM_FAILURE
        // If you hit this assert, then the requested allocation simply failed (you're out of memory) Interrogate the
        // variable 'au' to see the original allocation. You can also query 'newActualSize' to see the amount of memory
        // trying to be allocated. Finally, you can query 'reportedSize' to see how much memory was requested by the caller.
        m_assert(newActualAddress);
#endif

        if (!newActualAddress) {
            reportFailure("Request for reallocation failed. Out of memory.");
            return nullptr;
        }

        // Remove this allocation from our stats (we'll add the new reallocation again later)

        stats.totalReportedMemory -= au->reportedSize;
        stats.totalActualMemory   -= au->actualSize;

        // Update the allocation with the new information

        au->actualSize       = newActualSize;
        au->actualAddress    = newActualAddress;
        au->reportedSize     = calculateReportedSize(newActualSize - alignmentSlack);
        au->reportedAddress  = calculateReportedAddress(newActualAddress, alignment);
        au->allocationType   = reallocationType;
        au->sourceLine       = sourceLine;
        au->allocationNumber = currentAllocationCount;
        if (sourceFile) {
            strncpy(au->sourceFile, sourceFileStripper(sourceFile), sizeof(au->sourceFile) - 1);
        }
        else {
            strcpy(au->sourceFile, "??");
        }
        if (sourceFunc) {
            strncpy(au->sourceFunc, sourceFunc, sizeof(au->sourceFunc) - 1);
        }
        else {
            strcpy(au->sourceFunc, "??");
        }

        // The reallocation may cause the address to change, so we should relocate our allocation unit within the hash table

        size_t hashIndex = static_cast<size_t>(-1);
        if (oldReportedAddress != au->reportedAddress) {
            // Remove this allocation unit from the hash table

            {
                size_t oldHashIndex = (reinterpret_cast<size_t>(oldReportedAddress) >> 4) & (hashSize - 1);
                if (hashTable[oldHashIndex] == au) {
                    hashTable[oldHashIndex] = hashTable[oldHashIndex]->next;
                }
                else {
                    if (au->prev) {
                        au->prev->next = au->next;
                    }
                    if (au->next) {
                        au->next->prev = au->prev;
                    }
                }
            }

            // Re-insert it back into the hash table

            hashIndex = (reinterpret_cast<size_t>(au->reportedAddress) >> 4) & (hashSize - 1);
            if (hashTable[hashIndex]) {
                hashTable[hashIndex]->prev = au;
            }
            au->next             = hashTable[hashIndex];
            au->prev             = nullptr;
            hashTable[hashIndex] = au;
        }

        // Account for the new allocatin unit in our stats

        stats.totalReportedMemory += au->reportedSize;
        stats.totalActualMemory   += au->actualSize;
        if (stats.totalReportedMemory > stats.peakReportedMemory) {
            stats.peakReportedMemory = stats.totalReportedMemory;
        }
        if (stats.totalActualMemory > stats.peakActualMemory) {
            stats.peakActualMemory = stats.totalActualMemory;
        }
        if (reportedSize > originalReportedSize) {
            size_t deltaReportedSize         = reportedSize - originalReportedSize;
            stats.accumulatedReportedMemory += deltaReportedSize;
            stats.accumulatedActualMemory   += deltaReportedSize;
        }

        // Prepare the allocation unit for use (wipe it with recognizable garbage)

        wipeWithPattern(au, unusedPattern, originalReportedSize);

        // If you hit this assert, then something went wrong, because the allocation unit was properly validated PRIOR to
        // the reallocation. This should not happen.
        m_assert(m_validateAllocUnit(au));

        // Validate every single allocated unit in memory

        if (alwaysValidateAll) {
            m_validateAllAllocUnits();
        }

        // Log the result

        if (alwaysLogAll) {
            log("[~] ---->             addr 0x%08zX", reinterpret_cast<size_t>(au->reportedAddress));
        }

        // Resetting the globals insures that if at some later time, somebody calls our memory manager from an unknown
        // source (i.e. they didn't include our H file) then we won't think it was the last allocation.

        resetGlobals();

        // Return the (reported) address of the new allocation unit

#ifdef TEST_MEMORY_MANAGER
        log("[D] EXIT : m_reallocator()");
#endif

        return au->reportedAddress;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
m_reallocator(
    const char *   file,
    const uint32_t line,
    const char *   func,
    const uint32_t reallocationType,
    const size_t   reportedSize,
    void *         reportedAddress
) {
    return reallocateUnit(file, line, func, reallocationType, reportedSize, reportedAddress, 0);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Aligned allocations: aligned_alloc, memalign and posix_memalign, and the _aligned_* functions of Windows
// ---------------------------------------------------------------------------------------------------------------------------------

static bool
isValidAlignment(const char *file, const uint32_t line, const char *func, const size_t alignment) {
    if (alignment != 0 && (alignment & (alignment - 1)) == 0) {
        return true;
    }

    LockGuard lock;
    log("[!] Aligned allocation with an alignment that is not a power of two (%zu), by %s",
        alignment,
        ownerString(file, line, func)
    );
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
m_alignedAllocator(
    const char *   file,
    const uint32_t line,
    const char *   func,
    const uint32_t allocationType,
    const size_t   size,
    const size_t   alignment
) {
    if (!isValidAlignment(file, line, func, alignment)) {
        return nullptr;
    }

    return allocateUnit(file, line, func, allocationType, size, alignment);
}

// ---------------------------------------------------------------------------------------------------------------------------------

int
m_posixMemalign(
    const char *   file,
    const uint32_t line,
    const char *   func,
    void **        result,
    const size_t   alignment,
    const size_t   size
) {
    // POSIX also asks for a multiple of sizeof(void *), and leaves *result untouched when it fails
    if (!isValidAlignment(file, line, func, alignment) || alignment % sizeof(void *) != 0) {
        return EINVAL;
    }

    void * ptr = allocateUnit(file, line, func, m_alloc_malloc, size, alignment);
    if (!ptr) {
        return ENOMEM;
    }

    *result = ptr;
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
m_alignedReallocator(
    const char *   file,
    const uint32_t line,
    const char *   func,
    void *         reportedAddress,
    const size_t   size,
    const size_t   alignment
) {
    if (!isValidAlignment(file, line, func, alignment)) {
        return nullptr;
    }

    // Like _aligned_realloc(), a size of 0 frees the block
    if (reportedAddress && size == 0) {
        m_deallocator(file, line, func, m_alloc_aligned_free, reportedAddress);
        return nullptr;
    }

    return reallocateUnit(file, line, func, m_alloc_aligned_realloc, size, reportedAddress, alignment);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void *
m_alignedRecallocator(
    const char *   file,
    const uint32_t line,
    const char *   func,
    void *         reportedAddress,
    const size_t   count,
    const size_t   size,
    const size_t   alignment
) {
    LockGuard lock;

    if (size != 0 && count > SIZE_MAX / size) {
        log("[!] _aligned_recalloc of %zu elements of size %zu is too big, by %s", count, size, ownerString(file, line, func));
        return nullptr;
    }

    const size_t newSize = count * size;
    size_t       oldSize = 0;
    if (reportedAddress) {
        const sAllocUnit * au = findAllocUnit(reportedAddress);
        if (au) {
            oldSize = au->reportedSize;
        }
    }

    // Like _aligned_recalloc(), fill with zeros the bytes beyond the old size
    char * ptr = static_cast<char *>(m_alignedReallocator(file, line, func, reportedAddress, newSize, alignment));
    if (ptr && newSize > oldSize) {
        memset(ptr + oldSize, 0, newSize - oldSize);
    }

    return ptr;
}

// ---------------------------------------------------------------------------------------------------------------------------------

size_t
m_alignedSize(const char *file, const uint32_t line, const char *func, const void *reportedAddress, const size_t, const size_t) {
    LockGuard lock;

    const sAllocUnit * au = reportedAddress ? findAllocUnit(reportedAddress) : nullptr;
    if (!au) {
        log("[!] _aligned_msize of RAM that was never allocated, by %s", ownerString(file, line, func));
        // Like _aligned_msize(), return -1 on error
        return static_cast<size_t>(-1);
    }

    return au->reportedSize;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Deallocate memory and track it
// ---------------------------------------------------------------------------------------------------------------------------------

void
m_deallocator(
    const char *   sourceFile,
    const uint32_t sourceLine,
    const char *   sourceFunc,
    const uint32_t deallocationType,
    const void *   reportedAddress
) {
    LockGuard lock;

    {
#ifdef TEST_MEMORY_MANAGER
        log("[D] ENTER: m_deallocator()");
#endif

        // Log the request

        if (alwaysLogAll) {
            log("[-] ----- %8s of addr 0x%08zX           by %s",
                allocationTypes[deallocationType],
                reinterpret_cast<size_t>(const_cast<void *>(reportedAddress)),
                ownerString(sourceFile, sourceLine, sourceFunc)
            );
        }

        // We should only ever get here with a nullptr pointer if they try to do so with a call to free() (delete[] and delete will
        // both bail before they get here.) So, since ANSI allows free(nullptr), we'll not bother trying to actually free the allocated
        // memory or track it any further.

        if (reportedAddress) {
            // Go get the allocation unit

            sAllocUnit * au = findAllocUnit(reportedAddress);

            // If you hit this assert, you tried to deallocate RAM that wasn't allocated by this memory manager.
            m_assert(au != nullptr);
            if (au == nullptr) {
                reportFailure("Request to deallocate RAM that was never allocated");
                return;
            }

            // If you hit this assert, then the allocation unit that is about to be deallocated is damaged. But you probably
            // already know that from a previous assert you should have seen in validateAllocUnit() :)
            m_assert(m_validateAllocUnit(au));

            // If you hit this assert, then this deallocation was made from a source that isn't setup to use this memory
            // tracking software, use the stack frame to locate the source and include our H file.
            m_assert(deallocationType != m_alloc_unknown);

            const bool  methodsMatch =
                (deallocationType == m_alloc_delete       && au->allocationType == m_alloc_new            ) ||
                (deallocationType == m_alloc_delete_array && au->allocationType == m_alloc_new_array      ) ||
                (deallocationType == m_alloc_free         && au->allocationType == m_alloc_malloc         ) ||
                (deallocationType == m_alloc_free         && au->allocationType == m_alloc_calloc         ) ||
                (deallocationType == m_alloc_free         && au->allocationType == m_alloc_realloc        ) ||
                (deallocationType == m_alloc_aligned_free && au->allocationType == m_alloc_aligned_malloc ) ||
                (deallocationType == m_alloc_aligned_free && au->allocationType == m_alloc_aligned_realloc) ||
                (deallocationType == m_alloc_unknown                                                      );
            if (!methodsMatch) {
                log("[!] Allocation and deallocation methods do not match: %s of a block from %s, by %s",
                    allocationTypes[deallocationType],
                    allocationTypes[au->allocationType],
                    ownerString(sourceFile, sourceLine, sourceFunc)
                );
                m_dumpAllocUnit(au, "  ");
            }

            // If you hit this assert, you were trying to deallocate RAM that was not allocated in a way that is compatible with
            // the deallocation method requested. In other words, you have a allocation/deallocation mismatch.
            m_assert(methodsMatch);

            // If you hit this assert, then the "break on dealloc" flag for this allocation unit is set. Interrogate the 'au'
            // variable to determine information about this allocation unit.
            m_assertAlways(au->breakOnDealloc == false);

            // Wipe the deallocated RAM with a new pattern. This doen't actually do us much good in debug mode under WIN32,
            // because Microsoft's memory debugging & tracking utilities will wipe it right after we do. Oh well.

            wipeWithPattern(au, releasedPattern);

            // Do the deallocation

            free(au->actualAddress);

            // Remove this allocation unit from the hash table

            size_t hashIndex = (reinterpret_cast<size_t>(au->reportedAddress) >> 4) & (hashSize - 1);
            if (hashTable[hashIndex] == au) {
                hashTable[hashIndex] = au->next;
            }
            else {
                if (au->prev) {
                    au->prev->next = au->next;
                }
                if (au->next) {
                    au->next->prev = au->prev;
                }
            }

            // Remove this allocation from our stats

            stats.totalReportedMemory -= au->reportedSize;
            stats.totalActualMemory   -= au->actualSize;
            stats.totalAllocUnitCount--;

            // Add this allocation unit to the front of our reservoir of unused allocation units

            memset(au, 0, sizeof(sAllocUnit));
            au->next  = reservoir;
            reservoir = au;
        }

        // Resetting the globals insures that if at some later time, somebody calls our memory manager from an unknown
        // source (i.e. they didn't include our H file) then we won't think it was the last allocation.

        resetGlobals();

        // Validate every single allocated unit in memory

        if (alwaysValidateAll) {
            m_validateAllAllocUnits();
        }

        // If we're in the midst of static deinitialization time, track any pending memory leaks

        if (staticDeinitTime) {
            dumpLeakReport();
        }
    }

#ifdef TEST_MEMORY_MANAGER
    log("[D] EXIT : m_deallocator()");
#endif
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- The following utilitarian allow you to become proactive in tracking your own memory, or help you narrow in on those tough
// bugs.
// ---------------------------------------------------------------------------------------------------------------------------------

bool
m_validateAddress(const void *reportedAddress) {
    LockGuard lock;

    // Just see if the address exists in our allocation routines

    return findAllocUnit(reportedAddress) != nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool
m_validateAllocUnit(const sAllocUnit *allocUnit) {
    LockGuard lock;

    // Make sure the padding is untouched

    uint32_t * pre       = reinterpret_cast<uint32_t *>((char *)allocUnit->reportedAddress - paddingSize * sizeof(uint32_t));
    uint32_t * post      = reinterpret_cast<uint32_t *>((char *)allocUnit->reportedAddress + allocUnit->reportedSize);
    bool       errorFlag = false;
    for (uint32_t i = 0; i < paddingSize; i++, pre++, post++) {
        if (*pre != prefixPattern) {
            log("[!] A memory allocation unit was corrupt because of an underrun:");
            m_dumpAllocUnit(allocUnit, "  ");
            errorFlag = true;
        }

        // If you hit this assert, then you should know that this allocation unit has been damaged. Something (possibly the
        // owner?) has underrun the allocation unit (modified a few bytes prior to the start). You can interrogate the
        // variable 'allocUnit' to see statistics and information about this damaged allocation unit.
        m_assert(*pre == prefixPattern);

        if (*post != postfixPattern) {
            log("[!] A memory allocation unit was corrupt because of an overrun:");
            m_dumpAllocUnit(allocUnit, "  ");
            errorFlag = true;
        }

        // If you hit this assert, then you should know that this allocation unit has been damaged. Something (possibly the
        // owner?) has overrun the allocation unit (modified a few bytes after the end). You can interrogate the variable
        // 'allocUnit' to see statistics and information about this damaged allocation unit.
        m_assert(*post == postfixPattern);
    }

    // Return the error status (we invert it, because a return of 'false' means error)

    return !errorFlag;
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool
m_validateAllAllocUnits() {
    LockGuard lock;

    // Just go through each allocation unit in the hash table and count the ones that have errors

    uint64_t errors     = 0;
    uint64_t allocCount = 0;
    for (uint32_t i = 0; i < hashSize; i++) {
        sAllocUnit * ptr = hashTable[i];
        while (ptr) {
            allocCount++;
            if (!m_validateAllocUnit(ptr)) {
                errors++;
            }
            ptr = ptr->next;
        }
    }

    // Test for hash-table correctness

    if (allocCount != stats.totalAllocUnitCount) {
        log("[!] Memory tracking hash table corrupt!");
        errors++;
    }

    // If you hit this assert, then the internal memory (hash table) used by this memory tracking software is damaged! The
    // best way to track this down is to use the alwaysLogAll flag in conjunction with STRESS_TEST macro to narrow in on the
    // offending code. After running the application with these settings (and hitting this assert again), interrogate the
    // memory.log file to find the previous successful operation. The corruption will have occurred between that point and this
    // assertion.
    m_assert(allocCount == stats.totalAllocUnitCount);

    // If you hit this assert, then you've probably already been notified that there was a problem with a allocation unit in a
    // prior call to validateAllocUnit(), but this assert is here just to make sure you know about it. :)
    m_assert(errors == 0);

    // Log any errors

    if (errors) {
        log("[!] While validting all allocation units, %" PRIu64 " allocation unit(s) were found to have problems", errors);
    }

    // Return the error status

    return errors != 0;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Unused RAM calculation routines. Use these to determine how much of your RAM is unused (in bytes)
// ---------------------------------------------------------------------------------------------------------------------------------

size_t
m_calcUnused(const sAllocUnit *allocUnit) {
    LockGuard lock;

    const uint32_t * ptr   = reinterpret_cast<const uint32_t *>(allocUnit->reportedAddress);
    size_t           count = 0;

    for (size_t i = 0; i < allocUnit->reportedSize; i += sizeof(uint32_t), ptr++) {
        if (*ptr == unusedPattern) {
            count += sizeof(uint32_t);
        }
    }

    return count;
}

// ---------------------------------------------------------------------------------------------------------------------------------

uint64_t
m_calcAllUnused() {
    LockGuard lock;

    // Just go through each allocation unit in the hash table and count the unused RAM

    uint64_t total = 0;
    for (uint32_t i = 0; i < hashSize; i++) {
        sAllocUnit * ptr = hashTable[i];
        while (ptr) {
            total += m_calcUnused(ptr);
            ptr    = ptr->next;
        }
    }

    return total;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- The following functions are for logging and statistics reporting.
// ---------------------------------------------------------------------------------------------------------------------------------

void
m_dumpAllocUnit(const sAllocUnit *allocUnit, const char *prefix) {
    LockGuard lock;

    log("[I] %sAddress (reported): 0x%08zX",               prefix, reinterpret_cast<size_t>(allocUnit->reportedAddress));
    log("[I] %sAddress (actual)  : 0x%08zX",               prefix, reinterpret_cast<size_t>(allocUnit->actualAddress));
    log("[I] %sSize (reported)   : 0x%08zX (%s)",          prefix, allocUnit->reportedSize, memorySizeString(allocUnit->reportedSize));
    log("[I] %sSize (actual)     : 0x%08zX (%s)",          prefix, allocUnit->actualSize, memorySizeString(allocUnit->actualSize));
    log("[I] %sOwner             : %s(%" PRIu32 ")::%s",   prefix, allocUnit->sourceFile, allocUnit->sourceLine, allocUnit->sourceFunc);
    log("[I] %sAllocation type   : %s",                    prefix, allocationTypes[allocUnit->allocationType]);
    log("[I] %sAllocation number : %" PRIu64,              prefix, allocUnit->allocationNumber);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
m_dumpMemoryReport(const char *filename, const bool overwrite) {
    LockGuard lock;

    // Open the report file

    // Without a file name, the report only goes to the log function
    FILE * fp = nullptr;

    if (filename) {
        fp = fopen(filename, overwrite ? "w+b" : "ab");
    }

    // Header

    static char timeString[25];
    memset(timeString, 0, sizeof(timeString));
    time_t      t   = time(nullptr);
    struct tm * tme = localtime(&t);
    reportLine(
        m_log_report,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(
        m_log_report,
        fp,
        "|                                             Memory report for: %02d/%02d/%04d %02d:%02d:%02d                                               |",
        tme->tm_mon + 1,
        tme->tm_mday,
        tme->tm_year + 1900,
        tme->tm_hour,
        tme->tm_min,
        tme->tm_sec
    );
    reportLine(
        m_log_report,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(m_log_report, fp, "");
    reportLine(m_log_report, fp, "");

    // Report summary

    reportLine(
        m_log_report,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(
        m_log_report,
        fp,
        "|                                                           T O T A L S                                                            |"
    );
    reportLine(
        m_log_report,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(m_log_report, fp, "              Allocation unit count: %10s", insertCommas(stats.totalAllocUnitCount));
    reportLine(m_log_report, fp, "            Reported to application: %s", memorySizeString(stats.totalReportedMemory));
    reportLine(m_log_report, fp, "         Actual total memory in use: %s", memorySizeString(stats.totalActualMemory));
    reportLine(
        m_log_report,
        fp,
        "           Memory tracking overhead: %s",
        memorySizeString(stats.totalActualMemory - stats.totalReportedMemory)
    );
    reportLine(m_log_report, fp, "");

    reportLine(
        m_log_report,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(
        m_log_report,
        fp,
        "|                                                            P E A K S                                                             |"
    );
    reportLine(
        m_log_report,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(m_log_report, fp, "              Allocation unit count: %10s", insertCommas(stats.peakAllocUnitCount));
    reportLine(m_log_report, fp, "            Reported to application: %s", memorySizeString(stats.peakReportedMemory));
    reportLine(m_log_report, fp, "                             Actual: %s", memorySizeString(stats.peakActualMemory));
    reportLine(
        m_log_report,
        fp,
        "           Memory tracking overhead: %s",
        memorySizeString(stats.peakActualMemory - stats.peakReportedMemory)
    );
    reportLine(m_log_report, fp, "");

    reportLine(
        m_log_report,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(
        m_log_report,
        fp,
        "|                                                      A C C U M U L A T E D                                                       |"
    );
    reportLine(
        m_log_report,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(m_log_report, fp, "              Allocation unit count: %s", memorySizeString(stats.accumulatedAllocUnitCount));
    reportLine(m_log_report, fp, "            Reported to application: %s", memorySizeString(stats.accumulatedReportedMemory));
    reportLine(m_log_report, fp, "                             Actual: %s", memorySizeString(stats.accumulatedActualMemory));
    reportLine(m_log_report, fp, "");

    reportLine(
        m_log_report,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(
        m_log_report,
        fp,
        "|                                                           U N U S E D                                                            |"
    );
    reportLine(
        m_log_report,
        fp,
        " ---------------------------------------------------------------------------------------------------------------------------------- "
    );
    reportLine(m_log_report, fp, "    Memory allocated but not in use: %s", memorySizeString(m_calcAllUnused()));
    reportLine(m_log_report, fp, "");

    dumpAllocations(m_log_report, fp);

    if (fp) {
        fclose(fp);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
m_dumpLeakReport(void) {
    LockGuard lock;
    dumpLeakReport();
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setLogFunction(m_logFunction function) {
    LockGuard lock;
    logFunction = function;
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
m_logToStdout(m_logKind, const char *line) {
    // Flushed at once, because an error is often followed by an assert that stops the program
    printf("%s\n", line);
    fflush(stdout);
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void
setLogFileName(char *destination, const size_t size, const char *name) {
    if (!name) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, name, size - 1);
    destination[size - 1] = '\0';
}

// ---------------------------------------------------------------------------------------------------------------------------------

void
m_setLogFiles(const char *memoryLog, const char *leakReport) {
    LockGuard lock;
    setLogFileName(memoryLogFile, sizeof(memoryLogFile), memoryLog);
    setLogFileName(memoryLeakLogFile, sizeof(memoryLeakLogFile), leakReport);

    // The new memory log starts empty, and its header waits for its first line
    if (memoryLogFile[0]) {
        remove(memoryLogFile);
    }
    cleanupLogOnFirstRun = true;
}

// ---------------------------------------------------------------------------------------------------------------------------------

sMStats
m_getMemoryStatistics() {
    LockGuard lock;
    return stats;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// mmgr.cpp - End of file
// ---------------------------------------------------------------------------------------------------------------------------------
