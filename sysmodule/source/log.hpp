#pragma once

namespace ispotifai {

// Truncates the log file and writes a header. Call once, after the SD card is
// mounted.
void logReset(void);

// Appends a line to sdmc:/switch/ispotifai/sysmodule.log. A sysmodule has no
// console, so this file is the only way to see what happened.
void logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

} // namespace ispotifai
