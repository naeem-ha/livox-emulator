#pragma once

#include <cstdint>

#include "source/point_source.hpp"

namespace emu {

// Debug point source: a deterministic sweep over a wavy sphere around the
// sensor (~2 m range, Mid-360-ish FOV -7..52 deg elevation). Not the true
// non-repetitive rosette pattern — just recognizable, well-formed geometry.
class SyntheticSource : public PointSource {
 public:
  explicit SyntheticSource(uint32_t seed = 1);
  void fill(PointPacketData& out) override;

 private:
  double az_ = 0.0;
  double el_phase_ = 0.0;
  uint32_t rng_;
};

}  // namespace emu
