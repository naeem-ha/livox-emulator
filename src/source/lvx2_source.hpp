#pragma once

#include <deque>
#include <string>

#include "source/lvx2_parser.hpp"
#include "source/point_source.hpp"

namespace emu {

// Replays a .lvx2 recording as a point source. Decodes package payloads into a
// point FIFO and re-chunks into uniform 96-point wire packets (packages and
// wire packets have the same payload layout, but re-chunking guarantees
// dot_num==96 across package boundaries and file loops). Loops at EOF.
class Lvx2Source : public PointSource {
 public:
  bool open(const std::string& path, std::string& err);
  void fill(PointPacketData& out) override;

  uint64_t loops() const { return loops_; }

 private:
  bool refill_();

  Lvx2Reader reader_;
  std::deque<PointCartHigh> fifo_;
  uint64_t loops_ = 0;
  bool warned_empty_ = false;
};

}  // namespace emu
