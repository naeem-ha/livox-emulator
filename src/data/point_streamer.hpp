#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "source/point_source.hpp"

namespace emu {

// Streams Cartesian-high point packets from lidar_ip:56300 to the configured
// host endpoint at 2083.3 packets/s (96 pts each = 200k pts/s). The sender
// thread starts on the first set_target(); retargeting is lock-free.
class PointStreamer {
 public:
  PointStreamer(PointSource& source, std::string bind_ip, unsigned batch);
  ~PointStreamer();

  void set_target(uint32_t ip_be, uint16_t port);
  void stop();

 private:
  void run_();

  PointSource& source_;
  std::string bind_ip_;
  unsigned batch_;
  std::atomic<uint64_t> target_{0};  // ip_be << 16 | port; 0 = unset
  std::atomic<bool> stop_{false};
  std::thread thread_;
  std::atomic<bool> started_{false};
};

}  // namespace emu
