#pragma once

#include <array>
#include <cstdint>

#include "protocol/frames.hpp"

namespace emu {

// One wire packet's worth of Cartesian-high points, already in wire byte order.
struct PointPacketData {
  uint16_t dot_num = kPointsPerPacket;
  std::array<uint8_t, kPointPayloadBytes> bytes{};
};

// Pluggable point supplier (replay / synthetic / future live transducer).
// fill() is called from the point-streamer thread only and must always produce
// a full uniform packet of kPointsPerPacket points.
class PointSource {
 public:
  virtual ~PointSource() = default;
  virtual void fill(PointPacketData& out) = 0;
};

}  // namespace emu
