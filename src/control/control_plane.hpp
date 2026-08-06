#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "control/device_state.hpp"
#include "net/udp_socket.hpp"
#include "protocol/frame_codec.hpp"
#include "protocol/kv.hpp"
#include "util/config.hpp"

namespace emu {

struct ControlPlaneCallbacks {
  // Fired when the host configures where streams should go (keys 0x0006/0x0007).
  std::function<void(uint32_t ip_be, uint16_t port)> on_point_target;
  std::function<void(uint32_t ip_be, uint16_t port)> on_imu_target;
};

// Implements the Mid-360 control plane: discovery responder (0.0.0.0:56000),
// command server (lidar_ip:56100) and 1 Hz state push (lidar_ip:56200).
class ControlPlane {
 public:
  ControlPlane(const Config& cfg, ControlPlaneCallbacks cbs);

  bool init(std::string& err);
  // Blocks until |stop| becomes true. Poll loop + 1 s push tick.
  void run(const std::atomic<bool>& stop);

 private:
  void handle_datagram_(UdpSocket& sock, const uint8_t* buf, size_t len,
                        uint32_t src_ip_be, uint16_t src_port);
  void on_discovery_(UdpSocket& sock, const CtrlFrame& req, uint32_t src_ip_be,
                     uint16_t src_port);
  void on_param_write_(UdpSocket& sock, const CtrlFrame& req, uint32_t src_ip_be,
                       uint16_t src_port);
  void on_param_read_(UdpSocket& sock, const CtrlFrame& req, uint32_t src_ip_be,
                      uint16_t src_port);
  void push_state_info_();

  std::optional<std::vector<uint8_t>> read_key_(uint16_t key) const;
  std::vector<KV> state_info_kvs_() const;
  void reply_(UdpSocket& sock, const CtrlFrame& req, std::vector<uint8_t> payload,
              uint32_t dst_ip_be, uint16_t dst_port);

  Config cfg_;
  ControlPlaneCallbacks cbs_;
  DeviceState st_;
  UdpSocket disc_, cmd_, push_;
  uint32_t push_seq_ = 0;
};

}  // namespace emu
