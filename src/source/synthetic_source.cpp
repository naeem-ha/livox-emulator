#include "source/synthetic_source.hpp"

#include <cmath>
#include <cstring>

namespace emu {
namespace {
constexpr double kPi = 3.14159265358979323846;
}

SyntheticSource::SyntheticSource(uint32_t seed) : rng_(seed ? seed : 1) {}

void SyntheticSource::fill(PointPacketData& out) {
  out.dot_num = kPointsPerPacket;
  uint8_t* p = out.bytes.data();
  for (unsigned i = 0; i < kPointsPerPacket; ++i) {
    az_ += 0.0043633;  // ~0.25 deg per point
    if (az_ > 2 * kPi) az_ -= 2 * kPi;
    el_phase_ += 0.00091;
    if (el_phase_ > 2 * kPi) el_phase_ -= 2 * kPi;

    const double el = (22.5 + 29.5 * std::sin(el_phase_)) * kPi / 180.0;  // -7..52 deg
    const double r_m = 2.0 + 0.5 * std::sin(3.0 * az_) * std::cos(2.0 * el_phase_);

    PointCartHigh pt{};
    pt.x = static_cast<int32_t>(r_m * std::cos(el) * std::cos(az_) * 1000.0);
    pt.y = static_cast<int32_t>(r_m * std::cos(el) * std::sin(az_) * 1000.0);
    pt.z = static_cast<int32_t>(r_m * std::sin(el) * 1000.0);
    rng_ = rng_ * 1664525u + 1013904223u;  // LCG, deterministic per seed
    pt.reflectivity = static_cast<uint8_t>(110 + (rng_ >> 24) % 40);
    pt.tag = 0;
    std::memcpy(p, &pt, sizeof(pt));
    p += sizeof(pt);
  }
}

}  // namespace emu
