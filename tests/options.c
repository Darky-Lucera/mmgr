#include "check.h"

static int
memoryLogLinesStartingWith(const char *prefix) {
    char   line[512];
    int    count = 0;
    FILE * file  = fopen("memory.log", "rb");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            ++count;
        }
    }

    fclose(file);
    return count;
}

// The options and the breakpoint flags, with their m_set* and m_get* functions.
int
main(void) {
    CHECK(m_getStopOnErrors());
    CHECK(!m_getAlwaysValidateAll());
    CHECK(!m_getAlwaysLogAll());
    CHECK(m_getAlwaysWipeAll());
    CHECK(!m_getRandomWipe());
    CHECK(m_getBreakOnAllocation() == 0);

    m_setStopOnErrors(false);
    CHECK(!m_getStopOnErrors());

    // A breakpoint that is set and cleared again never stops the program
    m_setBreakOnAllocation(1000000);
    CHECK(m_getBreakOnAllocation() == 1000000);
    m_setBreakOnAllocation(0);

    void * flagged = malloc(16);
    m_setBreakOnRealloc(flagged, true);
    m_setBreakOnDealloc(flagged, true);
    CHECK(m_getBreakOnRealloc(flagged));
    CHECK(m_getBreakOnDealloc(flagged));
    m_setBreakOnRealloc(flagged, false);
    m_setBreakOnDealloc(flagged, false);
    CHECK(!m_getBreakOnRealloc(flagged));
    CHECK(!m_getBreakOnDealloc(flagged));
    free(flagged);

    // A block that mmgr does not know has no flags, and setting one is an error
    int local = 0;
    CHECK(!m_getBreakOnRealloc(&local));
    CHECK(!m_getBreakOnDealloc(&local));
    int errorsBefore = loggedErrors();
    m_setBreakOnRealloc(&local, true);
    m_setBreakOnDealloc(&local, true);
    CHECK(loggedErrors() == errorsBefore + 2);

    // m_setAlwaysLogAll() writes every allocation, reallocation and release to memory.log
    m_setAlwaysLogAll(true);
    CHECK(m_getAlwaysLogAll());
    void * block = malloc(16);
    block        = realloc(block, 32);
    free(block);
    m_setAlwaysLogAll(false);
    CHECK(memoryLogLinesStartingWith("[+]") > 0);
    CHECK(memoryLogLinesStartingWith("[~]") > 0);
    CHECK(memoryLogLinesStartingWith("[-]") > 0);

    int allocationLines = memoryLogLinesStartingWith("[+]");
    block               = malloc(16);
    free(block);
    CHECK(memoryLogLinesStartingWith("[+]") == allocationLines);

    // m_setAlwaysValidateAll() checks every block on each allocation, so it finds a corruption before the block is released
    unsigned char * volatile corrupt = (unsigned char *)malloc(16);
    corrupt[16]                      = 0x55;
    int errors                       = loggedErrors();
    m_setAlwaysValidateAll(true);
    block = malloc(16);
    m_setAlwaysValidateAll(false);
    CHECK(loggedErrors() > errors);
    free(block);

    errors = loggedErrors();
    free(corrupt);
    CHECK(loggedErrors() == errors + 1);

    // New blocks are filled with 0xfeedface, or with a random pattern with m_setRandomWipe()
    uint32_t * filled = (uint32_t *)malloc(16);
    CHECK(filled[0] == 0xfeedface);
    free(filled);

    m_setRandomWipe(true);
    CHECK(m_getRandomWipe());
    filled = (uint32_t *)malloc(16);
    CHECK(filled[0] != 0xfeedface);
    free(filled);
    m_setRandomWipe(false);

    CHECK(liveBlocks() == 0);
    return checkFailures;
}
