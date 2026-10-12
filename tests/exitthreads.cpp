#include <chrono>
#include <thread>
#include "check.h"

static const uint32_t leakedBlocks = 100;
static const uint32_t threadCount  = 4;

// Keeps releasing blocks for a while after main returns, as the threads of system libraries do (libdispatch on macOS)
static void
release() {
    for (uint32_t i = 0; i < 300; ++i) {
        // volatile, because the compiler may remove a new whose result is never used
        int * volatile value = new int(static_cast<int>(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        delete value;
    }
}

// Leaves some blocks alive and ends while other threads still release blocks. If those releases also rewrote the leak
// report, the writes would mix, and the end of the process could stop a thread halfway through the report.
static int
run() {
    m_enableMultithread();
    for (uint32_t i = 0; i < leakedBlocks; ++i) {
        int * volatile leaked = new int(static_cast<int>(i));
        (void) leaked;
    }

    for (uint32_t i = 0; i < threadCount; ++i) {
        std::thread(release).detach();
    }

    // Gives the threads time to start, so that they are still releasing blocks when the static destructors run
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return 0;
}

// Checks the report that "run" left: one header, and as many complete rows as the count in the report
static int
check() {
    char     line[512];
    uint32_t headers  = 0;
    uint32_t rows     = 0;
    uint32_t rowsRun  = 0;
    uint32_t reported = 0;
    FILE *   file     = fopen("memleaks.log", "rb");
    CHECK(file != NULL);
    if (file == NULL) {
        return checkFailures;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, "Memory leak report for") != NULL) {
            ++headers;
        }
        else if (strstr(line, "memory leaks found") != NULL || strstr(line, "memory leak found") != NULL) {
            reported = static_cast<uint32_t>(strtoul(line, NULL, 10));
        }
        else if (strlen(line) > 7 && strncmp(line + 6, " 0x", 3) == 0) {
            ++rows;
            if (strstr(line, "::run") != NULL) {
                ++rowsRun;
            }
            // The last column ends with the function name, so a row that was cut has no "::" or no line end
            CHECK(strstr(line, "::") != NULL);
            CHECK(strchr(line, '\n') != NULL);
        }
    }

    fclose(file);

    CHECK(headers == 1);
    // The report can also list the blocks that the threads and std::thread still own
    CHECK(rowsRun == leakedBlocks);
    CHECK(rows == reported);
    return checkFailures;
}

int
main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "run") == 0) {
        return run();
    }

    if (argc == 2 && strcmp(argv[1], "check") == 0) {
        return check();
    }

    printf("usage: exitthreads run|check\n");
    return 1;
}
