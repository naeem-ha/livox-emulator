#include "source/lvx2_source.hpp"

#include <cstring>

#include "util/log.hpp"

namespace emu {

bool Lvx2Source::open(const std::string& path, std::string& err) {
  if (!reader_.open(path, err)) return false;
  LOG_INFO("lvx2 open: %zu device(s), frame_duration %u ms", reader_.devices().size(),
           reader_.frame_duration_ms());
  for (const auto& d : reader_.devices()) {
    LOG_INFO("  device sn=%s lidar_id=%u type=%u", d.sn, d.lidar_id, d.device_type);
  }
  return true;
}

bool Lvx2Source::refill_() {
  bool rewound = false;
  while (fifo_.size() < kPointsPerPacket) {
    Lvx2Package pkg;
    if (!reader_.next_package(pkg)) {
      if (rewound) return false;  // empty or point-free file; give up this round
      reader_.rewind();
      ++loops_;
      rewound = true;
      LOG_INFO("lvx2 replay looped (x%llu)", static_cast<unsigned long long>(loops_));
      continue;
    }
    if (pkg.data_type == kDataTypeCartHigh) {
      const size_t n = pkg.payload_len / sizeof(PointCartHigh);
      for (size_t i = 0; i < n; ++i) {
        PointCartHigh pt;
        std::memcpy(&pt, pkg.payload + i * sizeof(pt), sizeof(pt));
        fifo_.push_back(pt);
      }
    } else if (pkg.data_type == kDataTypeCartLow) {
      const size_t n = pkg.payload_len / sizeof(PointCartLow);
      for (size_t i = 0; i < n; ++i) {
        PointCartLow lo;
        std::memcpy(&lo, pkg.payload + i * sizeof(lo), sizeof(lo));
        PointCartHigh pt{};
        pt.x = static_cast<int32_t>(lo.x) * 10;  // 10 mm units -> mm
        pt.y = static_cast<int32_t>(lo.y) * 10;
        pt.z = static_cast<int32_t>(lo.z) * 10;
        pt.reflectivity = lo.reflectivity;
        pt.tag = lo.tag;
        fifo_.push_back(pt);
      }
    }  // other data types (none defined in LVX2 v1.0) are skipped
  }
  return true;
}

void Lvx2Source::fill(PointPacketData& out) {
  out.dot_num = kPointsPerPacket;
  if (!refill_()) {
    if (!warned_empty_) {
      LOG_ERROR("lvx2 file yields no points; streaming zeros");
      warned_empty_ = true;
    }
    out.bytes.fill(0);
    return;
  }
  uint8_t* p = out.bytes.data();
  for (unsigned i = 0; i < kPointsPerPacket; ++i) {
    std::memcpy(p, &fifo_.front(), sizeof(PointCartHigh));
    fifo_.pop_front();
    p += sizeof(PointCartHigh);
  }
}

}  // namespace emu
