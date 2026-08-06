#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include "protocol/frames.hpp"

namespace emu {

struct Endpoint {
  uint32_t ip_be = 0;
  uint16_t port = 0;
  bool valid = false;
};

// Mutable device parameters. Owned and mutated only by the control-plane
// thread; streamers receive updates through their own thread-safe setters.
struct DeviceState {
  std::array<char, 16> sn{};
  std::array<uint8_t, 4> lidar_ip{};
  std::array<uint8_t, 4> netmask{};
  std::array<uint8_t, 4> gateway{};

  uint8_t pcl_data_type = 1;
  uint8_t pattern_mode = 0;
  uint8_t work_mode = 1;    // 0x01 = sampling/normal
  uint8_t imu_data_en = 1;
  std::array<uint8_t, 24> install_attitude{};

  Endpoint state_ep;  // key 0x0005
  Endpoint point_ep;  // key 0x0006
  Endpoint imu_ep;    // key 0x0007

  void set_sn(const std::string& s) {
    sn.fill(0);
    std::memcpy(sn.data(), s.data(), s.size() > 16 ? 16 : s.size());
  }
};

}  // namespace emu
