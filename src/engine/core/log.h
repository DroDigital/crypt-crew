#pragma once
// Minimal logging that also counts errors (the smoke test fails on any error).
#include <cstdarg>

namespace eng::log {

void info(const char* fmt, ...);
void warn(const char* fmt, ...);
void error(const char* fmt, ...);
int errorCount();
int warningCount();
// Forget errors logged so far (e.g. a failed first window attempt that was retried successfully).
void resetCounts();
// Route raylib TraceLog through us so failed loads count as errors.
void installRaylibHook(bool verbose);

}  // namespace eng::log
