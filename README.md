# MMGR - The Memory Manager

This is a modernized version of the classic MMGR from **Paul Nettle** ( https://www.paulnettle.com/ ). First appeared in flipcode article **Presenting A Memory Manager** (12 September 2000) ( https://www.flipcode.com/archives/Presenting_A_Memory_Manager.shtml ).

As we used it a lot at **Lucera** in our game engine **MindShake** (since 2009) we want to contribute with a modernized version of it. It has helped us to find several problems with memory: leaks, overflows, underflows, ...

I re-do this version from scratch from the last 'official version' (2010-12-31) adding the changes that we did for MindShake. At this point, this version, is not tested.

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

## Changes

 - Multi-thread support (call EnableMultithreadInMMGR()).
 - C support (not only c++).
 - strdup no-allocation detecttion.
 - Access to some extra functions:
   - m_breakOnAllocation(unsigned int count)
   - m_alwaysValidateAll(bool set)
   - m_alwaysLogAll(bool set)
   - m_alwaysWipeAll(bool set)
   - m_randomeWipe(bool set)

## Brief from the original version

MMGR is an industrial-strength memory manager for almost any project.

Not only will MMGR probably point out some bugs you never knew you had, but it is also a great development tool.
Among other standard tracking tools (overruns/underruns, deallocation mismatches, etc.), MMGR also provides a rich featureset of developer tools.
For example, it allows you to set a breakpoint on an arbitrary allocation, generate a snapshot report of memory currently in use, etc.
You can even ask MMGR to track how much of your allocated RAM is actually in use (nothing worse than allocating 64MB of memory just to find that you only needed 2K. :)

## Output

**test.cpp**:

```
 ----------------------------------------------------------------------------------------------------------------------------------
|                                          Memory leak report for:  09/19/2022 20:16:24                                            |
 ----------------------------------------------------------------------------------------------------------------------------------


12 memory leaks found:

Alloc.   Addr       Size       Addr       Size                        BreakOn BreakOn
Number Reported   Reported    Actual     Actual     Unused    Method  Dealloc Realloc Allocated by
------ ---------- ---------- ---------- ---------- ---------- -------- ------- ------- ---------------------------------------------------
000001 0x55D2FF4801B0 0x00000004 0x55D2FF480190 0x00000044 0x00000000 new         N       N    test.cpp(   11)::main
000002 0x55D2FF480200 0x00000014 0x55D2FF4801E0 0x00000054 0x00000008 new[]       N       N    test.cpp(   12)::main
000003 0x55D2FF480260 0x00000004 0x55D2FF480240 0x00000044 0x00000000 malloc      N       N    test.cpp(   13)::main
000004 0x55D2FF4802B0 0x00000005 0x55D2FF480290 0x00000045 0x00000000 malloc      N       N    test.cpp(   14)::main
000007 0x55D2FF480300 0x00000004 0x55D2FF4802E0 0x00000044 0x00000000 new         N       N    test.cpp(   34)::main
000008 0x55D2FF480350 0x00000014 0x55D2FF480330 0x00000054 0x00000010 new[]       N       N    test.cpp(   37)::main
000009 0x55D2FF4803B0 0x00000004 0x55D2FF480390 0x00000044 0x00000000 malloc      N       N    test.cpp(   42)::main
000010 0x55D2FF480400 0x00000004 0x55D2FF4803E0 0x00000044 0x00000000 malloc      N       N    test.cpp(   45)::main
000011 0x55D2FF480450 0x00000004 0x55D2FF480430 0x00000044 0x00000000 new         N       N    test.cpp(   48)::main
000012 0x55D2FF4804A0 0x00000014 0x55D2FF480480 0x00000054 0x00000010 new[]       N       N    test.cpp(   51)::main
000013 0x55D2FF480500 0x00000014 0x55D2FF4804E0 0x00000054 0x00000010 new[]       N       N    test.cpp(   56)::main
000014 0x55D2FF480560 0x00000014 0x55D2FF480540 0x00000054 0x00000010 new[]       N       N    test.cpp(   62)::main
```

**testCc**:

```
 ----------------------------------------------------------------------------------------------------------------------------------
|                                          Memory leak report for:  09/19/2022 20:16:24                                            |
 ----------------------------------------------------------------------------------------------------------------------------------


4 memory leaks found:

Alloc.   Addr       Size       Addr       Size                        BreakOn BreakOn
Number Reported   Reported    Actual     Actual     Unused    Method  Dealloc Realloc Allocated by
------ ---------- ---------- ---------- ---------- ---------- -------- ------- ------- ---------------------------------------------------
000001 0x564F523D51B0 0x00000004 0x564F523D5190 0x00000044 0x00000000 malloc      N       N    testC.c(   12)::main
000002 0x564F523D5200 0x00000005 0x564F523D51E0 0x00000045 0x00000000 malloc      N       N    testC.c(   13)::main
000004 0x564F523D52A0 0x00000014 0x564F523D5280 0x00000054 0x00000010 malloc      N       N    testC.c(   25)::main
000005 0x564F523D5300 0x00000014 0x564F523D52E0 0x00000054 0x00000010 malloc      N       N    testC.c(   31)::main
```
