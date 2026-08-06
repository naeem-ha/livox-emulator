#pragma once

#include <cstdint>
#include <ctime>

namespace emu {

// Absolute-deadline scheduler on CLOCK_MONOTONIC. Deadlines advance by a fixed
// period so send cadence does not accumulate drift; if we fall behind by more
// than 8 periods (scheduler stall, TCG slowness) the deadline resyncs to now
// and the event is counted rather than replayed as a burst.
class Pacer {
 public:
  void start(uint64_t period_ns);
  void wait_next();
  uint64_t resyncs() const { return resyncs_; }

 private:
  uint64_t period_ns_ = 0;
  timespec deadline_{};
  uint64_t resyncs_ = 0;
};

}  // namespace emu
