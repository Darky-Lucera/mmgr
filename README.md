# MMGR - The Memory Manager

This is a modernized version of the classic MMGR from **Paul Nettle** ( https://www.paulnettle.com/ ). First appeared in flipcode article **Presenting A Memory Manager** (12 September 2000) ( https://www.flipcode.com/archives/Presenting_A_Memory_Manager.shtml ).

The original source code, as released by the author, is available at <https://github.com/nettlep/mmgr>.

As we used it a lot at **Lucera** in our game engine **MindShake** (since 2009) we want to contribute with a modernized version of it. It has helped us to find several problems with memory: leaks, overflows, underflows, ...

I re-do this version from scratch from the last 'official version' (2010-12-31) adding the changes that we did for MindShake.

The [Changes](#changes) section sums up what is new compared with the original, [CHANGELOG.md](CHANGELOG.md) has the full list, and the [API](#api) section marks what is new or different.

The original readme.txt is included in the src folder.

## Use

Sometimes it is a little bit tricky to use it because it must be included after the common include files. In the case of MindhShake, we included it at the end of the precompiled headers:

```cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__cplusplus)
    #include <new>
    #include <vector>
    #include <set>
    #include <typeinfo>
    #include <string>
    #include <sstream>
    #include <algorithm>
#endif
#include <Core/mmgr/mmgr.h>
```

### Adding it to your project

You can copy the `src` folder into your project and compile `mmgr.cpp` with it. It needs C++11 or later. Compile it as C++17 to also track the sized `delete` (C++14) and the aligned `new` (C++17).

With CMake, you can also use the static library target `mmgr` (or `mmgr::mmgr`). Linking it adds `src` to the include path:

```cmake
include(FetchContent)
FetchContent_Declare(mmgr
    GIT_REPOSITORY https://github.com/Darky-Lucera/mmgr.git
    GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(mmgr)

target_link_libraries(my_program PRIVATE mmgr::mmgr)
```

`add_subdirectory(path/to/mmgr)` works too. To make `FetchContent` use a local copy instead of downloading it, pass `-DFETCHCONTENT_SOURCE_DIR_MMGR=path/to/mmgr`.

CMake options:

- `MMGR_BUILD_TESTS`: builds the tests in `tests/` and adds them to CTest, so that `ctest` runs them. It is `ON` only when mmgr is the main project.
- `MMGR_NO_DELETE_MACRO`: see the section about the `new` and `delete` macros.

With CMake, `mmgr.cpp` is always compiled as C++17, and the code that links mmgr can use any standard. A C project must also enable C++, for example with `project(my_project C CXX)`.

With MinGW, the program must link libstdc++ statically. `libstdc++-6.dll` has its own `operator new` and `operator delete`, which mmgr cannot replace, so the STL code inside it allocates blocks that mmgr does not know, and releasing them is reported as an error. The CMake target adds `-static-libstdc++` by itself; without CMake, add it to the link options.

### C++ exceptions

mmgr does not use C++ exceptions, so it also compiles with `-fno-exceptions`, and Emscripten needs no extra options.

The only exception is the one that the standard asks for: when `new` cannot allocate and there is no `new_handler`, it throws `std::bad_alloc`. If the code is compiled without exceptions, `new` calls `abort()` instead, as the standard library does. `new (std::nothrow)` returns `nullptr` in both cases.

### Logs

Each line that mmgr writes goes to its file and to a log function:

| Kind | File | Contents |
| --- | --- | --- |
| `m_log_memory` | `memory.log` | The errors, and every operation with `m_setAlwaysLogAll(true)` |
| `m_log_leaks` | `memleaks.log` | The leak report, written when the program ends and by `m_dumpLeakReport()` |
| `m_log_report` | the one given to `m_dumpMemoryReport()` | The memory report |

- `m_setLogFunction(function)` sets the log function, which receives the kind and the line. The default one, `m_logToStdout`, prints the line with `printf`. `NULL` turns it off.
- `m_setLogFiles(memoryLog, leakReport)` sets the names of the two files, with a path if needed. Then several programs can run in the same folder, or write their logs in a folder that they can write to (on Android and iOS, the current folder is read-only). `NULL` writes no file for that log.
- `m_dumpMemoryReport(NULL, ...)` only sends the report to the log function.
- `m_dumpLeakReport()` writes the leak report at once. Call it when the program closes on platforms where it may never end normally, such as the Web (Emscripten), Android and iOS. The report is written again if the program does end.

If a file cannot be opened, the line still goes to the log function. The header of `memory.log` describes the file, so it only goes to the file.

For example, to send the logs to logcat on Android:

```c
static void logToLogcat(m_logKind kind, const char *line) {
    __android_log_print(kind == m_log_memory ? ANDROID_LOG_WARN : ANDROID_LOG_INFO, "mmgr", "%s", line);
}

m_setLogFunction(logToLogcat);
m_setLogFiles(NULL, NULL);
```

### Conflicts with the `new` and `delete` macros

`mmgr.h` defines `new` and `delete` as macros, so any code compiled after it that uses these keywords in another way does not compile. The most common case is a C++11 deleted function:

```cpp
NonCopyable(const NonCopyable &) = delete;  // error after including mmgr.h
```

The same happens with `::new` and with classes that declare their own `operator new` or `operator delete`.

There are two ways to avoid it:

- Include `mmgr.h` after every header that uses these keywords. For code that you cannot reorder, surround it with `nommgr.h` and `mmgr.h`:

  ```cpp
  #include "nommgr.h"
  #include "header_with_deleted_functions.h"
  #include "mmgr.h"
  ```

- Define `MMGR_NO_DELETE_MACRO` for the whole project (with the CMake option of the same name, or with `-DMMGR_NO_DELETE_MACRO` for the compiler). Then `mmgr.h` does not redefine `delete`, and `= delete` works everywhere. The cost is that the logs no longer show the file and line of each `delete`. Leaks, double deletes and `new[]`/`delete` mismatches are still detected.

### Testing the code that handles a failed allocation

`m_setFailOnAllocation(n)` makes only the n-th allocation request after the call fail:

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
| macOS 12.7 (Monterey) | x64 | Apple Clang 14.0 | ✅ Debug and Release |
| macOS 14.8 (Sonoma), Apple M1 Pro | ARM64 | Apple Clang 14.0, 15.0, 16.0 | ✅ Debug and Release |
| Web (Node.js 22) | wasm32 | Emscripten 6.0.9 | ✅ Debug and Release |
| DOS (DOSBox-X) | x86 | DJGPP GCC 12.2 | ✅ Debug and Release |
| Android 17 (emulator) | x86_64 | NDK 27 | ✅ Debug and Release |
| Android 14 (OnePlus 9) | arm64-v8a, armeabi-v7a | NDK 27 | ✅ Debug and Release |
| Android 10 (Huawei P20 Pro) | arm64-v8a, armeabi-v7a | NDK 27 | ✅ Debug and Release |

## API

Everything is declared in `mmgr.h`, and every function can be called from C and from C++. **(new)** marks what the original mmgr does not have, and **(changed)** what works in a different way.

### Threads

`m_enableMultithread()` **(new)** protects mmgr with a mutex. Call it before a second thread uses mmgr. On DJGPP, which has no threads, it does nothing.

**(changed)** The functions of mmgr take the mutex, so they can be called while other threads allocate. Only the count of unused memory of `m_calcAllUnused()` and `m_dumpMemoryReport()` is approximate then, because it reads the contents of every block while the other threads write them.

### Options

**(changed)** Each option has an `m_set*` function and an `m_get*` function that returns the current value. The original only had C++ functions that returned a `bool &`.

| Functions | Default | Effect |
| --- | --- | --- |
| `m_setStopOnErrors(bool set)`, `m_getStopOnErrors()` **(new)** | `true` | Stops the program with an assert on each error. With `false`, mmgr only writes the error to `memory.log` and the program goes on. |
| `m_setAlwaysValidateAll(bool set)`, `m_getAlwaysValidateAll()` | `false` | Checks every block each time mmgr allocates, reallocates or releases one. It is slow, but it finds a corruption soon after it happens. |
| `m_setAlwaysLogAll(bool set)`, `m_getAlwaysLogAll()` | `false` | Writes every allocation, reallocation and release to `memory.log`. |
| `m_setAlwaysWipeAll(bool set)`, `m_getAlwaysWipeAll()` | `true` | Fills new blocks with `0xfeedface` and released blocks with `0xdeadbeef`, so that code that uses uninitialized or released memory fails sooner. |
| `m_setRandomWipe(bool set)`, `m_getRandomWipe()` | `false` | Fills the blocks with a random pattern instead, for stress tests. mmgr never calls `srand`, so the patterns are the same in each run. |

Compiling `mmgr.cpp` with `STRESS_TEST` defined turns on the options of `m_setAlwaysValidateAll`, `m_setAlwaysLogAll` and `m_setRandomWipe`, and makes the padding on each side of a block 4 KB instead of 16 bytes.

### Breakpoints

A breakpoint stops the program with an assert, even with `m_setStopOnErrors(false)`. Run the program in a debugger to see the call stack.

**(changed)** As with the options, each breakpoint has an `m_set*` function and an `m_get*` function.

| Functions | Stops the program when |
| --- | --- |
| `m_setBreakOnAllocation(uint64_t number)`, `m_getBreakOnAllocation()` | The allocation with this number happens. The leak report shows the number of each block, so you can stop on the allocation of a leak in the next run, if the program allocates in the same order. `0` turns it off. |
| `m_setBreakOnRealloc(const void *block, bool set)`, `m_getBreakOnRealloc(const void *block)` | `block` is reallocated. |
| `m_setBreakOnDealloc(const void *block, bool set)`, `m_getBreakOnDealloc(const void *block)` | `block` is released. |

Setting a breakpoint on a block that mmgr does not track is an error, and the `m_get*` functions return `false` for it.

### Failure injection

| Functions | Effect |
| --- | --- |
| `m_setFailOnAllocation(uint64_t n)`, `m_getFailOnAllocation()` **(new)** | Only the n-th allocation request from now fails. `0` turns it off. Setting it also starts counting the requests from zero. |
| `m_getAllocationRequestCount()` **(new)** | Returns the number of allocation requests since the last call to `m_setFailOnAllocation()`. |

See "Testing the code that handles a failed allocation" above.

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
| `totalReportedMemory`, `totalActualMemory` | Memory in use now: as requested, and with the padding of mmgr. |
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

See "Logs" above.

### Functions for the debugger

`m_validateAllocUnit()`, `m_calcUnused()` and `m_dumpAllocUnit()` take a `sAllocUnit`, which describes one block. mmgr does not give these to the program, but when an assert of mmgr stops the program, there is usually one in a local variable. Call `m_dumpAllocUnit(allocUnit, "")` from the debugger to write its details to `memory.log`.

### Macros and headers

- `mmgr.h` redefines `new`, `delete`, `malloc`, `calloc`, `realloc` and `free`, and **(new)** `strdup`, `_strdup` and the aligned functions: `aligned_alloc`, `memalign` and `posix_memalign`, or the `_aligned_*` functions on Windows. Then each call records its file, line and function.
- `nommgr.h` undefines all of these macros, **(new)** `nonew.h` only `new` and `delete`, and **(new)** `nomalloc.h` only the others. Including `mmgr.h` again defines them again.
- **(new)** `MMGR_NO_DELETE_MACRO` keeps `delete` as it is (see "Conflicts with the `new` and `delete` macros" above).
- The macros call `m_setOwner`, `m_allocator`, `m_reallocator`, `m_deallocator`, and **(new)** `m_callocator`, `m_allocatorCpy` and the aligned versions (`m_alignedAllocator`, `m_posixMemalign`, `m_alignedReallocator`, `m_alignedRecallocator` and `m_alignedSize`). You can call them directly to pass another file and line, for example from your own allocation function.

## What mmgr cannot detect

- **Use after free.** mmgr releases each block at once, so it cannot see a later write to it. The `0xdeadbeef` fill of `m_setAlwaysWipeAll` only makes reads of a released block easier to notice, until the memory is used again.
- **Reads of uninitialized memory.** The `0xfeedface` fill makes them easier to see in a debugger, and `m_setRandomWipe(true)` can change the behavior of the program.
- **Writes far from the block.** mmgr only sees a write that changes the padding just before or just after a block, and only when it checks the block.
- **Blocks that a library allocated with its own `malloc`.** Some functions return memory that the caller must release, for example `getline`, `asprintf` or `realpath`. mmgr did not track these blocks, so calling `free` on them after including `mmgr.h` is an error, and mmgr does not release them. Release them in code compiled without `mmgr.h`, or after `#include "nommgr.h"`. `new` in code compiled without `mmgr.h` is still tracked, but without the file and line.
- **Classes with their own `operator new` and `operator delete`.** Their blocks do not go through mmgr.

## Changes

Compared with the original, this version:

- works in C, and with several threads;
- tracks `strdup`, the aligned allocation functions, and the sized, aligned and `nothrow` forms of `operator new` and `operator delete`;
- can log errors without stopping the program, make a chosen allocation fail, and send its logs to your own function;
- has an `m_set*` and an `m_get*` function for each option and breakpoint, also in C;
- needs no C++ exceptions, builds with CMake, and has self-checking tests;
- fixes a buffer overrun on platforms where `long` is 8 bytes, wrong results with blocks larger than 2 GB, and the `calloc` macro, which took one argument.

[CHANGELOG.md](CHANGELOG.md) has the full list, including what you must change in your code when you update.

## Brief from the original version

MMGR is an industrial-strength memory manager for almost any project.

Not only will MMGR probably point out some bugs you never knew you had, but it is also a great development tool.
Among other standard tracking tools (overruns/underruns, deallocation mismatches, etc.), MMGR also provides a rich featureset of developer tools.
For example, it allows you to set a breakpoint on an arbitrary allocation, generate a snapshot report of memory currently in use, etc.
You can even ask MMGR to track how much of your allocated RAM is actually in use (nothing worse than allocating 64MB of memory just to find that you only needed 2K. :)

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

## License

This project is released under the MIT License. See [LICENSE](LICENSE).

The original code was written by Paul Nettle and first published on flipcode in 2000 under a custom license. In 2025 the author re-released it under the MIT License at <https://github.com/nettlep/mmgr>. The original copyright notices are kept in the source files.

The changes in this repository are released under the same MIT License.
