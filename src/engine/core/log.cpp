#include "engine/core/log.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <raylib.h>

namespace eng::log {
namespace {
std::atomic<int> gErrors{0};
std::atomic<int> gWarnings{0};
bool gVerbose = false;

void vlog(const char* tag, const char* fmt, va_list ap) {
    std::fprintf(stderr, "[%s] ", tag);
    std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr);
}

bool isFailure(const char* text) {
    // raylib reports missing files/failed loads as warnings; treat those as errors.
    static const char* kPatterns[] = {"Failed to", "failed to", "Unable to", "not found", "could not", "Could not", "Error"};
    for (const char* p : kPatterns)
        if (std::strstr(text, p)) return true;
    return false;
}

void raylibCallback(int level, const char* text, va_list args) {
    char buf[1024];
    std::vsnprintf(buf, sizeof buf, text, args);
    if (level >= LOG_ERROR || (level == LOG_WARNING && isFailure(buf))) {
        // Benign raylib warnings we know about.
        if (std::strstr(buf, "GLFW: Error: 65544") || std::strstr(buf, "Failed to initialize joystick") ||
            std::strstr(buf, "AUDIO: Failed to initialize playback device")) {
            std::fprintf(stderr, "[raylib] %s\n", buf);
            return;
        }
        ++gErrors;
        std::fprintf(stderr, "[raylib ERROR] %s\n", buf);
    } else if (level == LOG_WARNING) {
        ++gWarnings;
        std::fprintf(stderr, "[raylib WARN] %s\n", buf);
    } else if (gVerbose) {
        std::fprintf(stderr, "[raylib] %s\n", buf);
    }
}
}  // namespace

void info(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog("info", fmt, ap);
    va_end(ap);
}
void warn(const char* fmt, ...) {
    ++gWarnings;
    va_list ap;
    va_start(ap, fmt);
    vlog("warn", fmt, ap);
    va_end(ap);
}
void error(const char* fmt, ...) {
    ++gErrors;
    va_list ap;
    va_start(ap, fmt);
    vlog("ERROR", fmt, ap);
    va_end(ap);
}
int errorCount() { return gErrors.load(); }
int warningCount() { return gWarnings.load(); }
void resetCounts() {
    gErrors = 0;
    gWarnings = 0;
}

void installRaylibHook(bool verbose) {
    gVerbose = verbose;
    SetTraceLogLevel(verbose ? LOG_INFO : LOG_WARNING);
    SetTraceLogCallback(raylibCallback);
}

}  // namespace eng::log
