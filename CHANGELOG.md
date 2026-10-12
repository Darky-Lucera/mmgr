# Changelog

All notable changes to this project are documented in this file. The versions before 1.0.0 had no number, so they are named by their date.

## [Unreleased]

## [1.0.1]

This version fixes mmgr on macOS and iOS when the program is linked with `-dead_strip`, which Xcode uses by default for iOS. It also fixes the leak report written at exit, which came out mixed, cut or empty when other threads still released blocks.

### Fixed

- **macOS and iOS with `-dead_strip`**: this linker option removes the code that nothing in the program uses. Xcode uses it by default for iOS, and a macOS project can turn it on. A program written in C never calls `operator new` or `operator delete`, so ld64 removed every `operator new` of mmgr but kept `operator delete(void *)`. The system libraries then allocated blocks with their own `new` and released them with the `delete` of mmgr. mmgr did not know those blocks, so it stopped the program with "Request to deallocate RAM that was never allocated". On the iOS Simulator, this happened before `main`, in a static initializer of `libLLVMContainer.dylib`, which UIKit loads. On macOS, it happened in SkyLight, when the program connected to the window server. The replacement operators now have `__attribute__((used))`, which on Mach-O also stops `-dead_strip` from removing them. The tests now also pass on the iOS Simulator, and on macOS when they are linked with `-dead_strip`.
- **Leak report at exit with several threads**: after the static destructor of mmgr writes the leak report, every release writes it again, so that a block released by a later static destructor no longer appears in it. Other threads did the same: on macOS, AppKit and Metal keep releasing blocks from libdispatch threads while the program ends. The static destructor did not take the mutex, so its report and the report of another thread were written to the same file at the same time and came out mixed. Also, the process ends without waiting for its threads, so a thread could be stopped halfway through the report and leave it cut or empty. In a program that uses MiniFB on macOS, about half of the reports were broken. Now the static destructor takes the mutex, and only the thread that runs the static destructors writes the report again. Releases in other threads still update the statistics. The new test `tests/exitthreads.cpp` ends while four threads release blocks, and then checks the report.

## [1.0.0]

This version updates mmgr for C++11 to C++23. It tracks the aligned allocation functions and every form of `operator new` and `operator delete`, and adds failure injection, a log function, configurable log files and an option to log errors without stopping the program. It works correctly with several threads, needs no C++ exceptions, and runs on DOS (DJGPP) and the Web (Emscripten). Self-checking tests now run with CTest. It also fixes a buffer overrun on platforms where `long` is 8 bytes, and the `calloc` macro, which took one argument.

### Added

- **License**: a `LICENSE` file with the MIT License. In 2025 Paul Nettle re-released the original code under the MIT License, and the changes in this repository use the same license. The source files keep the original copyright notices.
- **Aligned allocation functions**: `aligned_alloc`, `memalign` and `posix_memalign` on POSIX systems, and `_aligned_malloc`, `_aligned_realloc`, `_aligned_recalloc`, `_aligned_msize` and `_aligned_free` on Windows, are tracked like `malloc`. mmgr reports a block from an `_aligned_*` function released with `free` or `realloc` (and the other way around), and an alignment that is not a power of two. The `_aligned_offset_*` variants are not tracked.
- **Sized `operator delete`** (C++14): mmgr checks that the size passed to `delete` is the size that was allocated. This catches deleting an object through a pointer to a base class without a virtual destructor.
- **Aligned `operator new` and `operator delete`** (C++17): objects of types with a large alignment, such as `alignas(64)`, are tracked and checked like any other allocation.
- **`nothrow` `operator new` and `operator delete`**: some runtimes have their own versions, which do not call the normal ones (AddressSanitizer, or libstdc++ before GCC 9), and mmgr did not see those blocks.
- **`std::` names**: `std::malloc`, `std::calloc`, `std::realloc`, `std::free` and `std::aligned_alloc` work after including `mmgr.h`.
- **`m_setStopOnErrors(bool set)`** and `m_getStopOnErrors()`: with `false`, mmgr writes each error to `memory.log` and the program goes on, instead of stopping on an assert. The breakpoints still stop the program.
- **Failure injection**: `m_setFailOnAllocation(n)` makes only the n-th allocation request fail, to test the code that handles a failed allocation. `m_getFailOnAllocation()` returns that number, and `m_getAllocationRequestCount()` returns the number of requests since the last call to `m_setFailOnAllocation()`. mmgr writes each failure that it causes to `memory.log` with `[F]`.
- **Logs**: `m_setLogFunction` sets a function that receives every line that mmgr writes, with its kind (`m_logKind`). The default one, `m_logToStdout`, prints the line. `m_setLogFiles` changes the names of `memory.log` and `memleaks.log`, or turns either file off. `m_dumpLeakReport()` writes the leak report at once, for platforms where the program may never end normally, such as the Web, Android and iOS.
- **`MMGR_NO_DELETE_MACRO`**: a define, and a CMake option, that keeps `delete` as it is, so that `= delete` works after including `mmgr.h`. The logs then do not show the file and line of each `delete`.
- **CMake**: the static library target `mmgr::mmgr`, for `FetchContent` and `add_subdirectory`, and the options `MMGR_BUILD_TESTS` and `MMGR_NO_DELETE_MACRO`. The tests are only built when mmgr is the main project.
- **Tests**: self-checking programs in `tests/`, run with `ctest`. They cover leaks, every misuse that mmgr detects (each allocation function with each release function, releasing a block twice, unknown blocks, overruns and underruns), the aligned functions, failure injection, logs, options, threads, and a static object that allocates before mmgr starts and releases after the leak report. In debug builds, they also check that errors and breakpoints stop the program.
- **Platforms**: tested on Windows (x64 and Win32), Linux, macOS, the Web (Emscripten, run with Node.js), DOS (DJGPP, run in DOSBox-X) and Android (NDK, on a phone with arm64 and 32-bit ARM, and in the emulator). The README lists the systems and compiler versions.
- **GitHub Actions**: workflows that build mmgr and run the tests on Windows (MSVC, clang-cl and MinGW, x64, x86 and ARM64), Linux (x64 and ARM64), macOS (x64 and ARM64) and the Web, with a status badge for each one in the README.
- `.editorconfig` and `.gitattributes`: UTF-8, LF line endings, a final newline and no trailing spaces.
- README sections on adding mmgr to a project, C++ exceptions, logs, conflicts with the `new` and `delete` macros, failure injection, the API, and what mmgr cannot detect.

### Changed

- **`EnableMultithreadInMMGR()` is now `m_enableMultithread()`**, like the other public functions. The old name no longer exists, so code that calls it must change.
- **Options and breakpoints have `m_set*` and `m_get*` functions**, in C and C++: `m_setAlwaysValidateAll`, `m_setAlwaysLogAll`, `m_setAlwaysWipeAll`, `m_setRandomWipe`, `m_setBreakOnAllocation`, `m_setBreakOnRealloc` and `m_setBreakOnDealloc`, each with its `m_get*` function. The getters return the value, instead of the C++ functions that returned a `bool &`, and `m_randomeWipe` is now `m_setRandomWipe`. The old names no longer exist, so code that calls them must change. A breakpoint on reallocation can also be set on a block from `_aligned_malloc` or `_aligned_realloc`.
- **Integer types with a fixed size**: the statistics (`sMStats`), the allocation numbers, `m_setBreakOnAllocation`, the failure injection counters and `m_calcAllUnused` use `uint64_t`, so they do not wrap after 4 GB or 4,294,967,296 allocations, and they behave the same on 32-bit and 64-bit platforms. Sizes use `size_t`, `m_calcUnused` included, and line numbers and allocation types use `uint32_t`. mmgr no longer uses `long` anywhere, whose size differs between Windows and Linux.
- **Threads**: each thread keeps its own owner (file, line and function), so blocks that several threads allocate at the same time get the right owner. The options, the checks, the statistics, the reports and the breakpoints now take the mutex, so they can be used while other threads allocate. ThreadSanitizer finds no data races in the threads test.
- **No C++ exceptions inside mmgr**: it builds with `-fno-exceptions`, and Emscripten needs no extra options. `new` still throws `std::bad_alloc` when it cannot allocate and there is no `new_handler`, or calls `abort()` in code built without exceptions. `new (std::nothrow)` returns `nullptr`.
- **Mismatches are logged**: a block released with the wrong function (for example a block from `new` released with `free`) is written to `memory.log`, not only asserted.
- A log file that cannot be opened no longer stops the program. The line still goes to the log function.
- `m_dumpMemoryReport(NULL, ...)` sends the report only to the log function.
- `strdup` returns `NULL` when its allocation fails, instead of crashing.
- The `new` and `delete` macros are only defined in C++, so C code can use `new` and `delete` as identifiers.
- `nommgr.h` and `nomalloc.h` also undefine `strdup`, `_strdup` and the aligned functions.
- `operator delete` is `noexcept`, as the standard declares it.
- **C++ standard**: `mmgr.cpp` needs C++11 or later. CMake builds it as C++17 (it used C++14), so that it also replaces the sized and aligned operators. The code that links mmgr can use any standard.
- **CMake**: it needs CMake 3.16 (it needed 3.10). The warning options, and the MSVC options `/permissive-`, `/utf-8`, `/sdl` and `/Zc:__cplusplus`, only apply to mmgr and its tests. mmgr links the system thread library (`Threads::Threads`), except on DJGPP and Emscripten. The tests use `include(CTest)`, so `ctest -T test`, which IDEs such as VS Code run, no longer warns about a missing `DartConfiguration.tcl`. With MinGW, the target links libstdc++ statically, because the operator new and delete inside libstdc++-6.dll cannot be replaced.
- mmgr builds without warnings with `/W4` on MSVC and `-Wall -Wextra` on GCC and Clang.
- The tests in `tests/` replace the programs `test/test.cpp` and `test/testC.c`.
- All files use LF line endings.
- **Code style**: the whole code base has a new format, with braces at the end of the line, return types on their own line, one parameter per line in long lists, and aligned declarations. A diff with the original version is no longer useful.

### Fixed

- **Buffer overrun where `long` is 8 bytes**, such as 64-bit Linux and macOS: the fill patterns were `long`, so filling a block wrote past its end. They are now `uint32_t`.
- **`calloc(count, size)`**: the macro took one argument, so a normal call to `calloc` did not compile after including `mmgr.h`. It now takes both arguments, and returns `NULL` when their product does not fit in a `size_t`, as `calloc` does.
- **Blocks larger than 2 GB**: they were not filled with the pattern, and reallocating a block larger than 4 GB wrote the pattern over part of its old contents. The statistics and the logs also cut every size to 32 bits.
- Some log lines passed a `size_t` to `%X` or `%d`, and printed addresses with `%010p`, which C does not define. They now use `%zX`, `%zu`, `PRIu32` and `PRIu64`.
- **Static objects of other files with threads enabled**: the order in which the statics of different files are built and destroyed is not fixed, so such an object could use the mutex of mmgr before it was built, or release a block after the mutex was destroyed. The mutex is now built on first use and never destroyed.
- The table of blocks in the leak and memory reports only lined up with 32-bit addresses. An address now takes as many hex digits as a pointer, so the columns line up on 64-bit platforms too.
- A failed allocation lost one of the allocation units that mmgr uses to track the blocks.
- A breakpoint on reallocation or release of an address that mmgr does not track dereferenced a null pointer in release builds, where the assert is empty. mmgr now writes it to `memory.log` as an error.

## 2022-09-20

The first version in this repository: the last version by Paul Nettle (2010-12-31), with the changes made for the MindShake engine at Lucera.

### Added

- **`strdup` and `_strdup`** are tracked like `malloc`.
- **Multi-thread support**: `EnableMultithreadInMMGR()` protects mmgr with a recursive mutex.
- **C support**: `mmgr.h` works in C files. The option and breakpoint functions have C versions: `m_breakOnAllocation`, `m_breakOnRealloc`, `m_breakOnDealloc`, `m_alwaysValidateAll`, `m_alwaysLogAll`, `m_alwaysWipeAll` and `m_randomeWipe`.
- `nonew.h` and `nomalloc.h`, which undefine only the `new` and `delete` macros, or only the macros of the allocation functions.
- A CMake project with two test programs, `test/test.cpp` and `test/testC.c`.
- `README.md`.

### Changed

- mmgr builds with MSVC v140 to v143, clang-cl, MinGW 11.2 and GCC 11.2. The include of `stdafx.h` is gone.
- mmgr builds as C++11 and C++14.

## 2010-12-31

The last version released by Paul Nettle, available at <https://github.com/nettlep/mmgr>.
