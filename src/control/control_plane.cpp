#include "control/control_plane.hpp"

#include <poll.h>
#include <time.h>

#include <cstring>

#include "util/log.hpp"

namespace emu {
namespace {

uint64_t mono_ns() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ull + ts.tv_nsec;
}

void ip_to_bytes(const std::string& s, std::array<uint8_t, 4>& out) {
  const uint32_t be = UdpSocket::parse_ip(s);
  std::memcpy(out.data(), &be, 4);
}

std::string ip_to_str(uint32_t ip_be) {
  uint8_t b[4];
  std::memcpy(b, &ip_be, 4);
  char buf[20];
  std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
  return buf;
}

}  // namespace

ControlPlane::ControlPlane(const Config& cfg, ControlPlaneCallbacks cbs)
    : cfg_(cfg), cbs_(std::move(cbs)) {
  st_.set_sn(cfg.sn);
  ip_to_bytes(cfg.lidar_ip, st_.lidar_ip);
  ip_to_bytes(cfg.netmask, st_.netmask);
  ip_to_bytes(cfg.gateway, st_.gateway);
}

bool ControlPlane::init(std::string& err) {
  // Discovery must bind the wildcard address: datagrams addressed to
  // 255.255.255.255 are not delivered to a socket bound to a unicast IP.
  if (!disc_.open("0.0.0.0", kPortDetection, /*broadcast=*/true, /*nonblocking=*/true, err))
    return false;
  if (!cmd_.open(cfg_.lidar_ip, kPortCmd, false, true, err)) return false;
  if (!push_.open(cfg_.lidar_ip, kPortPush, true, true, err)) return false;
  LOG_INFO("control plane up: discovery 0.0.0.0:%u, cmd %s:%u, push %s:%u",
           kPortDetection, cfg_.lidar_ip.c_str(), kPortCmd, cfg_.lidar_ip.c_str(),
           kPortPush);
  return true;
}

void ControlPlane::run(const std::atomic<bool>& stop) {
  pollfd fds[3] = {
      {disc_.fd(), POLLIN, 0}, {cmd_.fd(), POLLIN, 0}, {push_.fd(), POLLIN, 0}};
  UdpSocket* socks[3] = {&disc_, &cmd_, &push_};
  uint64_t next_push_ns = mono_ns() + 1'000'000'000ull;

  uint8_t buf[2048];
  while (!stop.load(std::memory_order_relaxed)) {
    (void)::poll(fds, 3, 250);
    for (int i = 0; i < 3; ++i) {
      if (!(fds[i].revents & POLLIN)) continue;
      for (;;) {
        uint32_t sip;
        uint16_t sport;
        const long n = socks[i]->recv_from(buf, sizeof(buf), sip, sport);
        if (n <= 0) break;
        handle_datagram_(*socks[i], buf, static_cast<size_t>(n), sip, sport);
      }
      fds[i].revents = 0;
    }
    const uint64_t now = mono_ns();
    if (now >= next_push_ns) {
      next_push_ns = now + 1'000'000'000ull;
      if (cfg_.state_push && st_.state_ep.valid) push_state_info_();
    }
  }
}

void ControlPlane::handle_datagram_(UdpSocket& sock, const uint8_t* buf, size_t len,
                                    uint32_t src_ip_be, uint16_t src_port) {
  auto f = parse_ctrl(buf, len);
  if (!f) {
    LOG_DEBUG("dropped malformed control frame (%zu bytes) from %s:%u", len,
              ip_to_str(src_ip_be).c_str(), src_port);
    return;
  }
  if (f->cmd_type != kCmdTypeReq) return;  // we only serve requests

  switch (f->cmd_id) {
    case kCmdDiscovery:
      on_discovery_(sock, *f, src_ip_be, src_port);
      break;
    case kCmdParamWrite:
      on_param_write_(sock, *f, src_ip_be, src_port);
      break;
    case kCmdParamRead:
      on_param_read_(sock, *f, src_ip_be, src_port);
      break;
    case kCmdRebootDevice:
    case kCmdResetDevice:
      LOG_INFO("acking cmd 0x%04X (reboot/reset is a no-op for the emulator)", f->cmd_id);
      reply_(sock, *f, {0x00}, src_ip_be, src_port);
      break;
    default:
      LOG_INFO("unhandled cmd 0x%04X from %s:%u -> generic ack", f->cmd_id,
               ip_to_str(src_ip_be).c_str(), src_port);
      reply_(sock, *f, {0x00}, src_ip_be, src_port);
      break;
  }
}

void ControlPlane::on_discovery_(UdpSocket& sock, const CtrlFrame& req,
                                 uint32_t src_ip_be, uint16_t src_port) {
  DetectionAckData ack{};
  ack.ret_code = 0;
  ack.dev_type = kDevTypeMid360;
  std::memcpy(ack.sn, st_.sn.data(), 16);
  std::memcpy(ack.lidar_ip, st_.lidar_ip.data(), 4);
  ack.cmd_port = kPortCmd;
  std::vector<uint8_t> payload(sizeof(ack));
  std::memcpy(payload.data(), &ack, sizeof(ack));
  LOG_DEBUG("discovery from %s:%u -> ack (seq %u)", ip_to_str(src_ip_be).c_str(),
            src_port, req.seq_num);
  reply_(sock, req, std::move(payload), src_ip_be, src_port);
}

void ControlPlane::on_param_write_(UdpSocket& sock, const CtrlFrame& req,
                                   uint32_t src_ip_be, uint16_t src_port) {
  auto kvs = parse_kv_list(req.payload.data(), req.payload.size());
  if (!kvs) {
    LOG_WARN("0x0100 with unparseable KV list (%zu bytes) -> ack anyway",
             req.payload.size());
    reply_(sock, req, build_write_ack(), src_ip_be, src_port);
    return;
  }
  for (const auto& kv : *kvs) {
    switch (kv.key) {
      case kKeyStateInfoHostCfg:
      case kKeyPointHostCfg:
      case kKeyImuHostCfg: {
        if (kv.value.size() != sizeof(HostEndpointCfg)) break;
        HostEndpointCfg c{};
        std::memcpy(&c, kv.value.data(), sizeof(c));
        Endpoint ep;
        std::memcpy(&ep.ip_be, c.ip, 4);
        ep.port = c.host_port;
        ep.valid = true;
        if (kv.key == kKeyStateInfoHostCfg) st_.state_ep = ep;
        if (kv.key == kKeyPointHostCfg) st_.point_ep = ep;
        if (kv.key == kKeyImuHostCfg) st_.imu_ep = ep;
        LOG_INFO("host endpoint 0x%04X -> %s:%u", kv.key, ip_to_str(ep.ip_be).c_str(),
                 ep.port);
        break;
      }
      case kKeyPclDataType:
        if (!kv.value.empty()) st_.pcl_data_type = kv.value[0];
        LOG_INFO("pcl_data_type = %u", st_.pcl_data_type);
        break;
      case kKeyPatternMode:
        if (!kv.value.empty()) st_.pattern_mode = kv.value[0];
        LOG_INFO("pattern_mode = %u", st_.pattern_mode);
        break;
      case kKeyWorkMode:
        if (!kv.value.empty()) st_.work_mode = kv.value[0];
        LOG_INFO("work_mode = %u", st_.work_mode);
        break;
      case kKeyImuDataEn:
        if (!kv.value.empty()) st_.imu_data_en = kv.value[0];
        LOG_INFO("imu_data_en = %u", st_.imu_data_en);
        break;
      case kKeyInstallAttitude:
        if (!kv.value.empty()) {
          std::memcpy(st_.install_attitude.data(), kv.value.data(),
                      kv.value.size() > 24 ? 24 : kv.value.size());
        }
        LOG_INFO("install_attitude stored (%zu bytes)", kv.value.size());
        break;
      default:
        LOG_INFO("write of unhandled key 0x%04X (%zu bytes) accepted", kv.key,
                 kv.value.size());
        break;
    }
  }
  // Ack before kicking the streamers so the host never sees data racing the ack.
  reply_(sock, req, build_write_ack(), src_ip_be, src_port);
  if (st_.point_ep.valid && cbs_.on_point_target)
    cbs_.on_point_target(st_.point_ep.ip_be, st_.point_ep.port);
  if (st_.imu_ep.valid && st_.imu_data_en && cbs_.on_imu_target)
    cbs_.on_imu_target(st_.imu_ep.ip_be, st_.imu_ep.port);
}

void ControlPlane::on_param_read_(UdpSocket& sock, const CtrlFrame& req,
                                  uint32_t src_ip_be, uint16_t src_port) {
  auto keys = parse_key_query(req.payload.data(), req.payload.size());
  std::vector<KV> kvs;
  if (keys) {
    for (uint16_t k : *keys) {
      auto v = read_key_(k);
      if (v) {
        kvs.push_back({k, std::move(*v)});
      } else {
        LOG_WARN("read of unknown key 0x%04X -> 1-byte zero", k);
        kvs.push_back({k, {0x00}});
      }
    }
  }
  reply_(sock, req, build_read_ack(kvs), src_ip_be, src_port);
}

std::optional<std::vector<uint8_t>> ControlPlane::read_key_(uint16_t key) const {
  auto bytes = [](const void* p, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    return std::vector<uint8_t>(b, b + n);
  };
  switch (key) {
    case kKeyFwType:
      return std::vector<uint8_t>{0x01};  // app firmware; 0 would mean bootloader
    case kKeyPclDataType:
      return std::vector<uint8_t>{st_.pcl_data_type};
    case kKeyPatternMode:
      return std::vector<uint8_t>{st_.pattern_mode};
    case kKeyWorkMode:
    case kKeyCurWorkState:
      return std::vector<uint8_t>{st_.work_mode};
    case kKeyImuDataEn:
      return std::vector<uint8_t>{st_.imu_data_en};
    case kKeyInstallAttitude:
      return bytes(st_.install_attitude.data(), st_.install_attitude.size());
    case kKeySn:
      return bytes(st_.sn.data(), 16);
    case kKeyProductInfo: {
      std::vector<uint8_t> v(64, 0);
      static const char kInfo[] = "Livox Mid-360 Emulator";
      std::memcpy(v.data(), kInfo, sizeof(kInfo) - 1);
      return v;
    }
    case kKeyVersionApp:
      return std::vector<uint8_t>{13, 2, 0, 0};
    case kKeyVersionLoader:
      return std::vector<uint8_t>{7, 1, 0, 0};
    case kKeyVersionHardware:
      return std::vector<uint8_t>{1, 0, 0, 0};
    case kKeyMac:
      // Locally administered MAC derived from the lidar IP.
      return std::vector<uint8_t>{0x02, 0x4C, 0x56, st_.lidar_ip[1], st_.lidar_ip[2],
                                  st_.lidar_ip[3]};
    case kKeyCoreTemp: {
      const int32_t t = 4250;  // 42.50 C
      return bytes(&t, 4);
    }
    case kKeyLidarIpCfg: {
      std::vector<uint8_t> v(12);
      std::memcpy(v.data(), st_.lidar_ip.data(), 4);
      std::memcpy(v.data() + 4, st_.netmask.data(), 4);
      std::memcpy(v.data() + 8, st_.gateway.data(), 4);
      return v;
    }
    case kKeyStateInfoHostCfg:
    case kKeyPointHostCfg:
    case kKeyImuHostCfg: {
      const Endpoint& ep = key == kKeyStateInfoHostCfg ? st_.state_ep
                           : key == kKeyPointHostCfg   ? st_.point_ep
                                                       : st_.imu_ep;
      HostEndpointCfg c{};
      std::memcpy(c.ip, &ep.ip_be, 4);
      c.host_port = ep.port;
      c.lidar_port = key == kKeyStateInfoHostCfg ? kPortPush
                     : key == kKeyPointHostCfg   ? kPortPoint
                                                 : kPortImu;
      return bytes(&c, sizeof(c));
    }
    default:
      return std::nullopt;
  }
}

std::vector<KV> ControlPlane::state_info_kvs_() const {
  std::vector<KV> kvs;
  for (uint16_t key : {static_cast<uint16_t>(kKeyPclDataType),
                       static_cast<uint16_t>(kKeyPatternMode),
                       static_cast<uint16_t>(kKeyLidarIpCfg),
                       static_cast<uint16_t>(kKeyStateInfoHostCfg),
                       static_cast<uint16_t>(kKeyPointHostCfg),
                       static_cast<uint16_t>(kKeyImuHostCfg),
                       static_cast<uint16_t>(kKeyInstallAttitude),
                       static_cast<uint16_t>(kKeyWorkMode),
                       static_cast<uint16_t>(kKeyImuDataEn),
                       static_cast<uint16_t>(kKeySn),
                       static_cast<uint16_t>(kKeyProductInfo),
                       static_cast<uint16_t>(kKeyVersionApp),
                       static_cast<uint16_t>(kKeyMac),
                       static_cast<uint16_t>(kKeyCurWorkState),
                       static_cast<uint16_t>(kKeyCoreTemp)}) {
    if (auto v = read_key_(key)) kvs.push_back({key, std::move(*v)});
  }
  return kvs;
}

void ControlPlane::push_state_info_() {
  CtrlFrame f;
  f.seq_num = push_seq_++;
  f.cmd_id = kCmdPushStateInfo;
  f.cmd_type = kCmdTypeReq;
  f.sender_type = kSenderLidar;
  f.payload = build_kv_push(state_info_kvs_());
  const auto frame = pack_ctrl(f);
  push_.send_to(frame.data(), frame.size(), st_.state_ep.ip_be, st_.state_ep.port);
}

void ControlPlane::reply_(UdpSocket& sock, const CtrlFrame& req,
                          std::vector<uint8_t> payload, uint32_t dst_ip_be,
                          uint16_t dst_port) {
  CtrlFrame f;
  f.seq_num = req.seq_num;  // ACK must echo the request's seq
  f.cmd_id = req.cmd_id;
  f.cmd_type = kCmdTypeAck;
  f.sender_type = kSenderLidar;
  f.payload = std::move(payload);
  const auto frame = pack_ctrl(f);
  if (!sock.send_to(frame.data(), frame.size(), dst_ip_be, dst_port)) {
    LOG_WARN("failed to send ack for cmd 0x%04X to %s:%u", req.cmd_id,
             ip_to_str(dst_ip_be).c_str(), dst_port);
  }
}

}  // namespace emu
