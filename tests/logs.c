#include "check.h"

static int  linesOfKind[3];
static char lastLine[512];

static void
collectLine(m_logKind kind, const char *line) {
    ++linesOfKind[kind];
    strncpy(lastLine, line, sizeof(lastLine) - 1);
}

static int
countLinesWith(const char *fileName, const char *text) {
    char   line[512];
    int    count = 0;
    FILE * file  = fopen(fileName, "rb");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, text) != NULL) {
            ++count;
        }
    }

    fclose(file);
    return count;
}

// The log function and the names of the log files.
int
main(void) {
    m_setStopOnErrors(false);
    m_setLogFunction(collectLine);

    int local                  = 0;
    int * volatile notFromMmgr = &local;

    // An error goes to memory.log and to the log function. The header of memory.log only goes to the file.
    free(notFromMmgr);
    CHECK(linesOfKind[m_log_memory] == 1);
    CHECK(strncmp(lastLine, "[!]", 3) == 0);
    CHECK(countLinesWith("memory.log", "never allocated") == 1);
    CHECK(countLinesWith("memory.log", "This file contains a log") == 1);

    // Other names let several programs run in the same folder
    m_setLogFiles("other.log", "other_leaks.log");
    free(notFromMmgr);
    CHECK(linesOfKind[m_log_memory] == 2);
    CHECK(countLinesWith("other.log", "never allocated") == 1);
    CHECK(countLinesWith("memory.log", "never allocated") == 1);

    // The leak report can also be written before the end, for programs that do not end normally
    void * leak = malloc(16);
    m_dumpLeakReport();
    CHECK(linesOfKind[m_log_leaks] > 0);
    CHECK(countLinesWith("other_leaks.log", "1 memory leak found") == 1);
    free(leak);
    int leakLines = linesOfKind[m_log_leaks];

    // NULL writes no file, but the log function still gets the lines
    m_setLogFiles(NULL, NULL);
    free(notFromMmgr);
    CHECK(linesOfKind[m_log_memory] == 3);
    CHECK(countLinesWith("other.log", "never allocated") == 1);

    // Without a file name, the memory report only goes to the log function
    void * block = malloc(16);
    m_dumpMemoryReport(NULL, true);
    CHECK(linesOfKind[m_log_report] > 0);
    CHECK(linesOfKind[m_log_leaks] == leakLines);
    free(block);

    m_setLogFunction(NULL);
    free(notFromMmgr);
    CHECK(linesOfKind[m_log_memory] == 3);

    CHECK(liveBlocks() == 0);
    return checkFailures;
}
