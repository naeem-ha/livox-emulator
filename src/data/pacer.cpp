#include "data/pacer.hpp"

namespace emu {
namespace {

uint64_t to_ns(const timespec& ts) {
  return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ull + ts.tv_nsec;
}

timespec from_ns(uint64_t ns) {
  timespec ts;
  ts.tv_sec = static_cast<time_t>(ns / 1'000'000'000ull);
  ts.tv_nsec = static_cast<long>(ns % 1'000'000'000ull);
  return ts;
}

}  // namespace

void Pacer::start(uint64_t period_ns) {
  period_ns_ = period_ns;
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  deadline_ = from_ns(to_ns(now) + period_ns_);
}

void Pacer::wait_next() {
  while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline_, nullptr) != 0) {
  }
  uint64_t next = to_ns(deadline_) + period_ns_;
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  if (to_ns(now) > next + 8 * period_ns_) {
    next = to_ns(now) + period_ns_;
    ++resyncs_;
  }
  deadline_ = from_ns(next);
}

}  // namespace emu
