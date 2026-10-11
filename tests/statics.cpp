#include "check.h"

static bool
leakReportIsClean() {
    char   line[512];
    bool   clean = false;
    FILE * file  = fopen("memleaks.log", "rb");
    if (file == NULL) {
        return false;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, "No memory leaks found") != NULL) {
            clean = true;
        }
    }

    fclose(file);
    return clean;
}

// A static object that is built before the statics of mmgr.cpp and destroyed after them, with threads enabled: it
// allocates before mmgr has built anything, and releases after mmgr has written the leak report.
struct EarlyAndLate {
    int * value;

    EarlyAndLate() {
        m_enableMultithread();
        value = new int(1);
    }

    ~EarlyAndLate() {
        delete value;

        // main has already returned, so a failure here can only end the program with an error
        if (liveBlocks() != 0 || loggedErrors() != 0 || !leakReportIsClean()) {
            printf("the release after the leak report failed\n");
            _Exit(1);
        }
    }
};

#if defined(_MSC_VER)
    // MSVC warns that this area is meant for libraries, but it is what builds this object before the statics of mmgr
    #pragma warning(disable : 4073)
    #pragma init_seg(lib)
static EarlyAndLate earlyAndLate;
#elif defined(__APPLE__)
// Mach-O has no init priorities, so the order is not forced here and the test only checks what it can
static EarlyAndLate earlyAndLate;
#else
static EarlyAndLate earlyAndLate __attribute__((init_priority(101)));
#endif

int
main() {
    CHECK(liveBlocks() == 1);
    CHECK(m_validateAddress(earlyAndLate.value));
    return checkFailures;
}
