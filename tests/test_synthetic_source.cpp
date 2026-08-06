#include <cmath>
#include <cstring>

#include "doctest/doctest.h"
#include "source/synthetic_source.hpp"

using namespace emu;

TEST_CASE("synthetic source produces well-formed packets") {
  SyntheticSource src(1);
  PointPacketData pkt;
  for (int iter = 0; iter < 50; ++iter) {
    src.fill(pkt);
    CHECK(pkt.dot_num == 96);
    for (unsigned i = 0; i < kPointsPerPacket; ++i) {
      PointCartHigh pt;
      std::memcpy(&pt, pkt.bytes.data() + i * sizeof(pt), sizeof(pt));
      const double r =
          std::sqrt(double(pt.x) * pt.x + double(pt.y) * pt.y + double(pt.z) * pt.z);
      CHECK(r > 1000.0);   // > 1 m
      CHECK(r < 3500.0);   // < 3.5 m
      CHECK(pt.tag == 0);
      CHECK(pt.reflectivity >= 110);
      CHECK(pt.reflectivity < 150);
    }
  }
}

TEST_CASE("synthetic source is deterministic per seed") {
  SyntheticSource a(7), b(7), c(8);
  PointPacketData pa, pb, pc;
  a.fill(pa);
  b.fill(pb);
  c.fill(pc);
  CHECK(std::memcmp(pa.bytes.data(), pb.bytes.data(), pa.bytes.size()) == 0);
  // Different seed changes reflectivity noise only; geometry phase matches.
  CHECK(std::memcmp(pa.bytes.data(), pc.bytes.data(), pa.bytes.size()) != 0);
}
