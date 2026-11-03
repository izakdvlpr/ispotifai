#include "log.hpp"

#include <switch.h>

#include <sys/stat.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <ispotifai/version.hpp>

namespace ispotifai {

namespace {

const char *const kDir     = "sdmc:/switch/ispotifai";
const char *const kPath    = "sdmc:/switch/ispotifai/sysmodule.log";
const char *const kPathOld = "sdmc:/switch/ispotifai/sysmodule.log.1";

// A retry loop can produce the same line every 100ms for days, so the file is
// capped and rotated instead of growing until the card is full.
constexpr off_t kMaxBytes = 64 * 1024;

char g_last[256]  = "";
unsigned g_repeat = 0;

void rotate(void) {
    struct stat st;
    if (stat(kPath, &st) != 0 || st.st_size < kMaxBytes)
        return;

    remove(kPathOld);
    rename(kPath, kPathOld);
}

// Every write is open/append/close: a crash then still leaves the last line on
// the card, which is the whole point of this file.
void writeLine(const char *mode, const char *text) {
    if (mode[0] == 'a')
        rotate();

    FILE *f = fopen(kPath, mode);
    if (!f)
        return;

    fputs(text, f);
    fputc('\n', f);
    fclose(f);
}

// Emits the pending "repeated" count, if any, before a different line goes out.
void flushRepeat(void) {
    if (g_repeat == 0)
        return;

    char line[64];
    snprintf(line, sizeof(line), "last message repeated %u times", g_repeat);
    g_repeat = 0;

    writeLine("a", line);
}

} // namespace

void logReset(void) {
    mkdir(kDir, 0777);

    g_last[0] = '\0';
    g_repeat  = 0;

    char line[128];
    snprintf(line, sizeof(line), "--- %s %s sysmodule ---", kName, kVersion);
    writeLine("w", line);
}

void logf(const char *fmt, ...) {
    char line[256];

    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    // Collapse runs of an identical line rather than writing each one.
    if (strcmp(line, g_last) == 0) {
        g_repeat++;
        return;
    }

    flushRepeat();

    snprintf(g_last, sizeof(g_last), "%s", line);
    writeLine("a", line);
}

} // namespace ispotifai
