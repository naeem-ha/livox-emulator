#pragma once

#include <cstdarg>
#include <cstdio>
#include <ctime>

namespace emu {

enum class LogLevel : int { kDebug = 0, kInfo = 1, kWarn = 2, kError = 3 };

inline LogLevel& log_level() {
  static LogLevel level = LogLevel::kInfo;
  return level;
}

inline void logf(LogLevel lvl, const char* tag, const char* fmt, ...) {
  if (lvl < log_level()) return;
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  std::fprintf(stderr, "[%7ld.%03ld] %s %s\n", static_cast<long>(ts.tv_sec),
               ts.tv_nsec / 1000000, tag, buf);
}

}  // namespace emu

#define LOG_DEBUG(...) ::emu::logf(::emu::LogLevel::kDebug, "D", __VA_ARGS__)
#define LOG_INFO(...) ::emu::logf(::emu::LogLevel::kInfo, "I", __VA_ARGS__)
#define LOG_WARN(...) ::emu::logf(::emu::LogLevel::kWarn, "W", __VA_ARGS__)
#define LOG_ERROR(...) ::emu::logf(::emu::LogLevel::kError, "E", __VA_ARGS__)
