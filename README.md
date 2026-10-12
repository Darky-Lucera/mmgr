# MMGR: The Memory Manager

[![Windows](https://github.com/Darky-Lucera/mmgr/actions/workflows/windows.yml/badge.svg)](https://github.com/Darky-Lucera/mmgr/actions/workflows/windows.yml)
[![MinGW](https://github.com/Darky-Lucera/mmgr/actions/workflows/mingw.yml/badge.svg)](https://github.com/Darky-Lucera/mmgr/actions/workflows/mingw.yml)
[![Linux](https://github.com/Darky-Lucera/mmgr/actions/workflows/linux.yml/badge.svg)](https://github.com/Darky-Lucera/mmgr/actions/workflows/linux.yml)
[![macOS](https://github.com/Darky-Lucera/mmgr/actions/workflows/macos.yml/badge.svg)](https://github.com/Darky-Lucera/mmgr/actions/workflows/macos.yml)
[![Web](https://github.com/Darky-Lucera/mmgr/actions/workflows/web.yml/badge.svg)](https://github.com/Darky-Lucera/mmgr/actions/workflows/web.yml)

mmgr finds memory bugs in C and C++ programs: leaks, writes just outside a block, blocks released twice or with the wrong function, and more. It replaces `new`, `delete`, `malloc`, `free` and the related functions with its own versions. These record the file, line and function of each call, and put a few bytes with a known pattern before and after each block, so that mmgr can tell when something writes over them.

It is a modernized version of the memory manager that Paul Nettle published in 2000. See [History](#history) for where it comes from, and [Changes](#changes) for what is new.

## What mmgr detects

| Problem | When mmgr reports it |
| --- | --- |
| A leak: memory that is never released | When the program ends, in `memleaks.log`, or when you call `m_dumpLeakReport()` |
| An overrun: a write just after the end of a block | When the block is reallocated or released, or when you call `m_validateAllAllocUnits()` |
| An underrun: a write just before the start of a block | When the block is reallocated or released, or when you call `m_validateAllAllocUnits()` |
| Releasing a block with the wrong function, such as `new[]` with `delete`, `new` with `free`, or `_aligned_malloc` with `free` | When the block is released |
| Releasing a block twice, or releasing a pointer that mmgr never returned | When it is released |
| A `delete` with the wrong size (C++14), such as deleting a derived object through a base class that has no virtual destructor | At the `delete` |
| An aligned allocation with an alignment that is not a power of two | At the call |
| A `calloc` or `_aligned_recalloc` whose total size does not fit in a `size_t` | At the call |

mmgr writes each error to `memory.log`, with the file and line where the error happened and where the block was allocated. Then it stops the program with an assert, so that a debugger shows the call stack. With `m_setStopOnErrors(false)`, it only writes the error and the program goes on. Some problems are out of its reach; see [What mmgr cannot detect](#what-mmgr-cannot-detect).

## Quick start

### 1. Add mmgr to your project

With CMake, link the static library target `mmgr::mmgr`. It also adds `src` to the include path:

```cmake
include(FetchContent)
FetchContent_Declare(mmgr
    GIT_REPOSITORY https://github.com/Darky-Lucera/mmgr.git
    GIT_TAG        v1.0.1
)
FetchContent_MakeAvailable(mmgr)

target_link_libraries(my_program PRIVATE mmgr::mmgr)
```

`add_subdirectory(path/to/mmgr)` works too. To make `FetchContent` use a local copy instead of downloading it, pass `-DFETCHCONTENT_SOURCE_DIR_MMGR=path/to/mmgr`.

| CMake option | Default | Effect |
| --- | --- | --- |
| `MMGR_BUILD_TESTS` | `ON` only when mmgr is the main project | Builds the tests in `tests/` and adds them to CTest, so that `ctest` runs them |
| `MMGR_NO_DELETE_MACRO` | `OFF` | Keeps `delete` as it is; see [Conflicts with the `new` and `delete` macros](#conflicts-with-the-new-and-delete-macros) |

The target compiles `mmgr.cpp` as C++17, and your code can use any standard. A C project must also enable C++, for example with `project(my_project C CXX)`.

Without CMake, copy the `src` folder into your project and compile `mmgr.cpp` with the rest of the code. It needs C++11 or later; compile it as C++17 to also track the sized `delete` (C++14) and the aligned `new` (C++17). Build it into the program or into a static library, not into a DLL: in a DLL, it would only replace `new` and `delete` for the code of that DLL.

### 2. Include `mmgr.h` last

`mmgr.h` redefines `new`, `delete`, `malloc` and the related functions as macros, so it must come after every other header: the standard headers, the headers of libraries, and your own headers. A good place is the end of a precompiled header, or of a header that every file includes:

```cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
// ... every other header

#include "mmgr.h"
```

It works the same in C files.

### 3. Run the program

mmgr writes two files in the current folder:

| File | Contents |
| --- | --- |
| `memory.log` | The errors, and every operation if you call `m_setAlwaysLogAll(true)` |
| `memleaks.log` | The leak report, written when the program ends |

Every line also goes to a log function, which prints it to the standard output by default. You can change the names and the folder of the files, or send the lines somewhere else; see [Logs](#logs).

## Output

The leak report (`memleaks.log`) of `tests/leaks.cpp`:

```text
 ----------------------------------------------------------------------------------------------------------------------------------
|                                          Memory leak report for:  10/11/2026 12:58:27                                            |
 ----------------------------------------------------------------------------------------------------------------------------------


4 memory leaks found:

Alloc. Addr               Size       Addr               Size                           BreakOn BreakOn
Number Reported           Reported   Actual             Actual     Unused     Method   Dealloc Realloc Allocated by
------ ------------------ ---------- ------------------ ---------- ---------- -------- ------- ------- ---------------------------------------------------
000001 0x00005D176F514860 0x00000004 0x00005D176F514850 0x00000024 0x00000000 new         N       N    leaks.cpp(    6)::main
000002 0x00005D176F514890 0x00000014 0x00005D176F514880 0x00000034 0x00000010 new[]       N       N    leaks.cpp(    7)::main
000003 0x00005D176F5148D0 0x00000004 0x00005D176F5148C0 0x00000024 0x00000000 malloc      N       N    leaks.cpp(    8)::main
000004 0x00005D176F514900 0x00000005 0x00005D176F5148F0 0x00000025 0x00000000 malloc      N       N    leaks.cpp(    9)::main
```

An error in `memory.log` from `tests/errors.cpp`, where a block from `new[]` is released with `delete`:

```text
[!] Allocation and deallocation methods do not match: delete of a block from new[], by errors.cpp(   25)::main
[I]   Address (reported): 0x5E0F44FD4D40
[I]   Address (actual)  : 0x5E0F44FD4D30
[I]   Size (reported)   : 0x00000004 (         4 bytes     )
[I]   Size (actual)     : 0x00000024 (        36 bytes     )
[I]   Owner             : errors.cpp(24)::main
[I]   Allocation type   : new[]
[I]   Allocation number : 2
```

## Guides

### Logs

Each line that mmgr writes goes to its file and to a log function:

| Kind | File | Contents |
| --- | --- | --- |
| `m_log_memory` | `memory.log` | The errors, and every operation with `m_setAlwaysLogAll(true)` |
| `m_log_leaks` | `memleaks.log` | The leak report, written when the program ends and by `m_dumpLeakReport()` |
| `m_log_report` | the one given to `m_dumpMemoryReport()` | The memory report |

- `m_setLogFunction(function)` sets the log function, which receives the kind and the line. The default one, `m_logToStdout`, prints the line with `printf`. `NULL` turns it off.
- `m_setLogFiles(memoryLog, leakReport)` sets the names of the two files, with a path if needed. Use it to run several programs in the same folder without mixing their logs, or to write the logs to a folder where the program can write. `NULL` writes no file for that log.
- `m_dumpMemoryReport(NULL, ...)` only sends the report to the log function.
- `m_dumpLeakReport()` writes the leak report right away. If the program then ends normally, the report is written again, with the blocks that are still alive at that point.

If a file cannot be opened, the line still goes to the log function. The header at the top of `memory.log` describes the file, so it is written only to the file, not to the log function.

### Conflicts with the `new` and `delete` macros

`mmgr.h` defines `new` and `delete` as macros, so code after it that uses these keywords for something else no longer compiles. The most common case is a C++11 deleted function:

```cpp
NonCopyable(const NonCopyable &) = delete;  // error after including mmgr.h
```

The same happens with `::new` and with classes that declare their own `operator new` or `operator delete`.

There are two ways to avoid it:

- Include `mmgr.h` after every header that uses these keywords. If a header has to come after `mmgr.h`, surround it with `nommgr.h` and `mmgr.h`:

  ```cpp
  #include "nommgr.h"
  #include "header_with_deleted_functions.h"
  #include "mmgr.h"
  ```

- Define `MMGR_NO_DELETE_MACRO` for the whole project (with the CMake option of the same name, or with `-DMMGR_NO_DELETE_MACRO` for the compiler). Then `mmgr.h` does not redefine `delete`, and `= delete` works everywhere. The cost is that the logs no longer show the file and line of each `delete`. Leaks, double deletes and `new[]`/`delete` mismatches are still detected.

### Testing the code that handles a failed allocation

`m_setFailOnAllocation(n)` makes the n-th allocation request after the call fail, and only that one. On that request:

- `malloc`, `calloc`, `realloc`, `strdup` and the aligned functions return `NULL`, and `posix_memalign` returns `ENOMEM`. A failed `realloc` leaves the old block as it was.
- `new` calls the `new_handler` and tries again, or throws `std::bad_alloc` when there is no `new_handler`. `new (std::nothrow)` returns `nullptr`.

`m_setFailOnAllocation(0)` turns it off. `m_getAllocationRequestCount()` returns the number of requests since the last call to `m_setFailOnAllocation()`. mmgr writes each failure that it causes to `memory.log` with `[F]`, not as an error.

To make each allocation of a piece of code fail once, and check that the code releases everything every time:

```c
uint64_t before = m_getMemoryStatistics().totalAllocUnitCount;
for (uint64_t n = 1; ; ++n) {
    m_setFailOnAllocation(n);
    run_the_code();
    assert(m_getMemoryStatistics().totalAllocUnitCount == before);

    // When the n-th request never happens, every allocation has failed once
    if (m_getAllocationRequestCount() < n) {
        break;
    }
}
m_setFailOnAllocation(0);
```

### C++ exceptions

mmgr does not use C++ exceptions, so it also compiles with `-fno-exceptions`, and Emscripten needs no extra options.

There is one case that the standard requires: when `new` cannot allocate memory and there is no `new_handler`, it throws `std::bad_alloc`. If the code is compiled without exceptions, `new` calls `abort()` instead, as the standard library does. `new (std::nothrow)` returns `nullptr` in both cases.

## Platform notes

| Platform | Note |
| --- | --- |
| Web (Emscripten), Android, iOS | The program may never end normally, so the leak report at the end may never be written. Call `m_dumpLeakReport()` when the program closes. |
| Android, iOS | The current folder is read-only. Call `m_setLogFiles()` with a folder that the program can write to, or send the logs to a log function (see the example below). |
| macOS, iOS, Linux | mmgr replaces the global `operator new` of the whole process, so it also tracks the C++ code of the system libraries (AppKit, UIKit and Metal on macOS and iOS, Mesa on Linux). When those blocks are still in use at the end, the leak report lists them as `??(0)::??`. They are not leaks of your program. Some of those libraries allocate from their own threads, so call `m_enableMultithread()` at the start of `main`, even if the program itself has only one thread. |
| MinGW | Link libstdc++ statically. `libstdc++-6.dll` has its own `operator new` and `operator delete`, which mmgr cannot replace, so mmgr does not know the blocks that the STL code inside the DLL allocates, and releasing them is reported as an error. The CMake target adds `-static-libstdc++` for you; without CMake, add it to the link options. |
| DOS (DJGPP) | There are no threads, so `m_enableMultithread()` does nothing. |

To send the logs to logcat on Android:

```c
static void logToLogcat(m_logKind kind, const char *line) {
    __android_log_print(kind == m_log_memory ? ANDROID_LOG_WARN : ANDROID_LOG_INFO, "mmgr", "%s", line);
}

m_setLogFunction(logToLogcat);
m_setLogFiles(NULL, NULL);
```

## API

Everything is declared in `mmgr.h`, and every function can be called from C and from C++. **(new)** marks what the original mmgr does not have, and **(changed)** what works in a different way.

### Threads

`m_enableMultithread()` **(new)** protects mmgr with a mutex. Call it before a second thread uses mmgr. On DJGPP, which has no threads, it does nothing.

**(changed)** All the functions of mmgr take the mutex, so they can be called while other threads allocate. The only result that is approximate then is the count of unused memory of `m_calcAllUnused()` and `m_dumpMemoryReport()`, because it reads the contents of every block while other threads may be writing to them.

When the program ends, mmgr writes the leak report again after each release, but only in the thread that runs the static destructors. The process does not wait for its other threads, so a thread that wrote the report could be stopped halfway through it. As a result, a block that another thread releases at the very end can still appear in the report.

### Options

**(changed)** Each option has an `m_set*` function and an `m_get*` function that returns the current value. The original only had C++ functions that returned a `bool &`.

| Functions | Default | Effect |
| --- | --- | --- |
| `m_setStopOnErrors(bool set)`, `m_getStopOnErrors()` **(new)** | `true` | Stops the program with an assert on each error. With `false`, mmgr only writes the error to `memory.log` and the program goes on. |
| `m_setAlwaysValidateAll(bool set)`, `m_getAlwaysValidateAll()` | `false` | Checks every block each time mmgr allocates, reallocates or releases one. It is slow, but it finds a corruption soon after it happens. |
| `m_setAlwaysLogAll(bool set)`, `m_getAlwaysLogAll()` | `false` | Writes every allocation, reallocation and release to `memory.log`. |
| `m_setAlwaysWipeAll(bool set)`, `m_getAlwaysWipeAll()` | `true` | Fills new blocks with `0xfeedface` and released blocks with `0xdeadbeef`, so that code that uses uninitialized or released memory fails sooner. |
| `m_setRandomWipe(bool set)`, `m_getRandomWipe()` | `false` | Fills the blocks with a random pattern instead, for stress tests. mmgr never calls `srand`, so the patterns are the same in each run. |

If `STRESS_TEST` is defined when `mmgr.cpp` is compiled, the options of `m_setAlwaysValidateAll`, `m_setAlwaysLogAll` and `m_setRandomWipe` start turned on, and the padding on each side of a block is 4 KB instead of 16 bytes.

### Breakpoints

A breakpoint stops the program with an assert, even with `m_setStopOnErrors(false)`. Run the program in a debugger to see the call stack.

**(changed)** As with the options, each breakpoint has an `m_set*` function and an `m_get*` function.

| Functions | Stops the program when |
| --- | --- |
| `m_setBreakOnAllocation(uint64_t number)`, `m_getBreakOnAllocation()` | The allocation with this number happens. The leak report shows the number of each block, so in the next run you can stop where a leaked block is allocated, as long as the program allocates in the same order. `0` turns it off. |
| `m_setBreakOnRealloc(const void *block, bool set)`, `m_getBreakOnRealloc(const void *block)` | `block` is reallocated. |
| `m_setBreakOnDealloc(const void *block, bool set)`, `m_getBreakOnDealloc(const void *block)` | `block` is released. |

Setting a breakpoint on a block that mmgr does not track is an error, and the `m_get*` functions return `false` for it.

### Failure injection

| Functions | Effect |
| --- | --- |
| `m_setFailOnAllocation(uint64_t n)`, `m_getFailOnAllocation()` **(new)** | Only the n-th allocation request from now fails. `0` turns it off. Setting it also resets the request count to zero. |
| `m_getAllocationRequestCount()` **(new)** | Returns the number of allocation requests since the last call to `m_setFailOnAllocation()`. |

See [Testing the code that handles a failed allocation](#testing-the-code-that-handles-a-failed-allocation).

### Checks

| Function | Returns |
| --- | --- |
| `m_validateAddress(const void *block)` | `true` if mmgr tracks `block`. |
| `m_validateAllAllocUnits()` | Checks the padding of every block and the hash table, and writes each error to `memory.log`. Note that it returns **`true` when it finds errors**. |

mmgr checks a block by itself when the block is reallocated or released.

### Statistics

`m_getMemoryStatistics()` returns a copy of the statistics in a `sMStats`:

| Fields | Contents |
| --- | --- |
| `totalReportedMemory`, `totalActualMemory` | Memory in use now: the size that the program asked for, and the size with the padding of mmgr. |
| `peakReportedMemory`, `peakActualMemory` | The highest values of the two fields above. |
| `accumulatedReportedMemory`, `accumulatedActualMemory` | All the memory allocated since the program started. |
| `totalAllocUnitCount`, `peakAllocUnitCount`, `accumulatedAllocUnitCount` | The same, for the number of blocks. |

All the fields are `uint64_t`, also on 32-bit platforms.

`m_calcAllUnused()` returns the bytes of all the live blocks that still hold `0xfeedface`, that is, memory that was allocated but never written. It needs `m_setAlwaysWipeAll(true)` and `m_setRandomWipe(false)`.

### Logs and reports

| Function | Effect |
| --- | --- |
| `m_setLogFunction(m_logFunction function)` **(new)** | Sets the function that receives each line that mmgr writes. `NULL` turns it off. |
| `m_logToStdout(m_logKind kind, const char *line)` **(new)** | The default log function. It prints the line with `printf`. |
| `m_setLogFiles(const char *memoryLog, const char *leakReport)` **(new)** | Sets the names of `memory.log` and `memleaks.log`. `NULL` writes no file for that log. |
| `m_dumpLeakReport()` **(new)** | Writes the leak report now. |
| `m_dumpMemoryReport(const char *file, bool overwrite)` | Writes a report of the statistics and of every live block. **(changed)** With `NULL`, the report only goes to the log function. In C++, the default arguments are `"memreport.log"` and `true`. |

See [Logs](#logs).

### Functions for the debugger

`m_validateAllocUnit()`, `m_calcUnused()` and `m_dumpAllocUnit()` take a `sAllocUnit`, which describes one block. The program never gets one from mmgr, but when an assert of mmgr stops the program, the current function usually has one in a local variable. Call `m_dumpAllocUnit(allocUnit, "")` from the debugger to write its details to `memory.log`.

### Macros and headers

| Header or define | Effect |
| --- | --- |
| `mmgr.h` | Redefines `new`, `delete`, `malloc`, `calloc`, `realloc` and `free`, and **(new)** `strdup`, `_strdup` and the aligned functions: `aligned_alloc`, `memalign` and `posix_memalign`, or the `_aligned_*` functions on Windows. Then each call records its file, line and function. |
| `nommgr.h` | Undefines all of these macros. Including `mmgr.h` again defines them again. |
| `nonew.h` **(new)** | Undefines only `new` and `delete`. |
| `nomalloc.h` **(new)** | Undefines only the macros of the allocation functions. |
| `MMGR_NO_DELETE_MACRO` **(new)** | Keeps `delete` as it is; see [Conflicts with the `new` and `delete` macros](#conflicts-with-the-new-and-delete-macros). |

The macros call `m_setOwner`, `m_allocator`, `m_reallocator`, `m_deallocator`, and **(new)** `m_callocator`, `m_allocatorCpy` and the aligned versions (`m_alignedAllocator`, `m_posixMemalign`, `m_alignedReallocator`, `m_alignedRecallocator` and `m_alignedSize`). You can also call them directly to record a different file and line, for example the caller of your own allocation function.

## What mmgr cannot detect

- **Use after free.** mmgr releases each block at once, so it cannot see a later write to it. The `0xdeadbeef` fill of `m_setAlwaysWipeAll` only makes reads of a released block easier to notice, until the memory is used again.
- **Reads of uninitialized memory.** The `0xfeedface` fill makes them easier to see in a debugger, and `m_setRandomWipe(true)` can change the behavior of the program.
- **Writes far from the block.** mmgr only sees a write that changes the padding just before or just after a block, and only when it checks the block.
- **Blocks that a library allocated with its own `malloc`.** Some functions return memory that the caller must release, for example `getline`, `asprintf` or `realpath`. mmgr never saw these blocks, so calling `free` on them after including `mmgr.h` is reported as an error, and mmgr does not release them. Release them in code compiled without `mmgr.h`, or after `#include "nommgr.h"`. `new` in code compiled without `mmgr.h` is still tracked, but without the file and line.
- **Classes with their own `operator new` and `operator delete`.** Their blocks do not go through mmgr.

## Tested platforms

GitHub builds mmgr and runs the tests in `tests/` on each push, in Debug and Release:

| System | Architecture | Compilers | Status |
| --- | --- | --- | --- |
| Windows | x64, x86 (Win32), ARM64 | MSVC, clang-cl (x64) | [![Windows](https://github.com/Darky-Lucera/mmgr/actions/workflows/windows.yml/badge.svg)](https://github.com/Darky-Lucera/mmgr/actions/workflows/windows.yml) |
| Windows | x64, x86 | MinGW-w64 GCC (MSYS2) | [![MinGW](https://github.com/Darky-Lucera/mmgr/actions/workflows/mingw.yml/badge.svg)](https://github.com/Darky-Lucera/mmgr/actions/workflows/mingw.yml) |
| Linux | x64, ARM64 | GCC, Clang | [![Linux](https://github.com/Darky-Lucera/mmgr/actions/workflows/linux.yml/badge.svg)](https://github.com/Darky-Lucera/mmgr/actions/workflows/linux.yml) |
| macOS | x64, ARM64 | Apple Clang | [![macOS](https://github.com/Darky-Lucera/mmgr/actions/workflows/macos.yml/badge.svg)](https://github.com/Darky-Lucera/mmgr/actions/workflows/macos.yml) |
| Web | wasm32 | Emscripten (run with Node.js) | [![Web](https://github.com/Darky-Lucera/mmgr/actions/workflows/web.yml/badge.svg)](https://github.com/Darky-Lucera/mmgr/actions/workflows/web.yml) |

Also tested by hand, without warnings:

| System | Architecture | Compilers | Result |
| --- | --- | --- | --- |
| Windows 11 | x64 | MSVC 19.44 (Visual Studio 2022), MinGW-w64 GCC 15.2 | ✅ Debug and Release |
| Windows 11 | x86 (Win32) | MSVC 19.44, clang-cl 19.1 | ✅ Debug and Release |
| Linux (Ubuntu 26.04 on WSL2) | x64 | GCC 15.2, Clang 21.1, with AddressSanitizer and ThreadSanitizer | ✅ Debug and Release |
| macOS 12.7 (Monterey) | x64 | Apple Clang 14.0, with and without `-dead_strip` | ✅ Debug and Release |
| macOS 14.8 (Sonoma), Apple M1 Pro | ARM64 | Apple Clang 14.0, 15.0, 16.0, Clang 22.1, 23.1 (Homebrew), with and without `-dead_strip` | ✅ Debug and Release |
| iOS 16.2 (Simulator, Xcode 14.2) | x86_64 | Apple Clang 14.0, with and without `-dead_strip` | ✅ Debug and Release |
| iOS 16.4, 17.2, 18.3 (Simulator, Xcode 14.3.1, 15.2, 16.2) | ARM64 | Apple Clang 14.0, 15.0, 16.0, with and without `-dead_strip` | ✅ Debug and Release |
| Web (Node.js 22) | wasm32 | Emscripten 6.0.9 | ✅ Debug and Release |
| DOS (DOSBox-X) | x86 | DJGPP GCC 12.2 | ✅ Debug and Release |
| Android 17 (emulator) | x86_64 | NDK 27 | ✅ Debug and Release |
| Android 14 (OnePlus 9) | arm64-v8a, armeabi-v7a | NDK 27 | ✅ Debug and Release |
| Android 10 (Huawei P20 Pro) | arm64-v8a, armeabi-v7a | NDK 27 | ✅ Debug and Release |

## Changes

Compared with the original, this version:

- works in C, and with several threads;
- tracks `strdup`, the aligned allocation functions, and the sized, aligned and `nothrow` forms of `operator new` and `operator delete`;
- can log errors without stopping the program, make a chosen allocation fail, and send its logs to your own function;
- has an `m_set*` and an `m_get*` function for each option and breakpoint, also in C;
- needs no C++ exceptions, builds with CMake, and has self-checking tests;
- fixes a buffer overrun on platforms where `long` is 8 bytes, wrong results with blocks larger than 2 GB, and the `calloc` macro, which took one argument.

[CHANGELOG.md](CHANGELOG.md) has the full list, including what you must change in your code when you update.

## History

Paul Nettle (<https://www.paulnettle.com/>) published mmgr in 2000, with the flipcode article [Presenting A Memory Manager](https://www.flipcode.com/archives/Presenting_A_Memory_Manager.shtml). His last version is from 2010-12-31. In 2025 he re-released it under the MIT License at <https://github.com/nettlep/mmgr>. His original `readme.txt` is in the `src` folder, and the README of his repository describes mmgr like this:

> MMGR is an industrial-strength memory manager for almost any project.
>
> Not only will MMGR probably point out some bugs you never knew you had, but it is also a great development tool. Among other standard tracking tools (overruns/underruns, deallocation mismatches, etc.), MMGR also provides a rich featureset of developer tools. For example, it allows you to set a breakpoint on an arbitrary allocation, generate a snapshot report of memory currently in use, etc. You can even ask MMGR to track how much of your allocated RAM is actually in use (nothing worse than allocating 64MB of memory just to find that you only needed 2K. :)

At **Lucera** we used mmgr in our game engine, **MindShake**, from 2009, and it helped us find many memory problems: leaks, overruns and underruns, among others. This version starts from Paul Nettle's last version, adds the changes that we made for MindShake, and brings the rest up to date.

## License

This project is released under the MIT License. See [LICENSE](LICENSE).

The original code was written by Paul Nettle and first published on flipcode in 2000 under a custom license. In 2025 the author re-released it under the MIT License at <https://github.com/nettlep/mmgr>. The original copyright notices are kept in the source files.

The changes in this repository are released under the same MIT License.
