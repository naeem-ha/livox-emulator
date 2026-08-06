#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace emu {

// Streams synthetic stationary IMU packets from lidar_ip:56400 at 200 Hz
// (acc ~ (0,0,1g), gyro ~ 0, small gaussian noise). Starts on first
// set_target().
class ImuStreamer {
 public:
  explicit ImuStreamer(std::string bind_ip);
  ~ImuStreamer();

  void set_target(uint32_t ip_be, uint16_t port);
  void stop();

 private:
  void run_();

  std::string bind_ip_;
  std::atomic<uint64_t> target_{0};
  std::atomic<bool> stop_{false};
  std::thread thread_;
  std::atomic<bool> started_{false};
};

}  // namespace emu
