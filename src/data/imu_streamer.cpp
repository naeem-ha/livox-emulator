#include "data/imu_streamer.hpp"

#include <time.h>

#include <cstring>
#include <random>

#include "data/pacer.hpp"
#include "net/udp_socket.hpp"
#include "protocol/frame_codec.hpp"
#include "util/log.hpp"

namespace emu {
namespace {

uint64_t realtime_ns() {
  timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ull + ts.tv_nsec;
}

}  // namespace

ImuStreamer::ImuStreamer(std::string bind_ip) : bind_ip_(std::move(bind_ip)) {}

ImuStreamer::~ImuStreamer() { stop(); }

void ImuStreamer::set_target(uint32_t ip_be, uint16_t port) {
  target_.store((static_cast<uint64_t>(ip_be) << 16) | port, std::memory_order_release);
  if (!started_.exchange(true)) thread_ = std::thread(&ImuStreamer::run_, this);
}

void ImuStreamer::stop() {
  stop_.store(true);
  if (thread_.joinable()) thread_.join();
}

void ImuStreamer::run_() {
  UdpSocket sock;
  std::string err;
  if (!sock.open(bind_ip_, kPortImu, false, false, err)) {
    LOG_ERROR("imu streamer: %s", err.c_str());
    return;
  }

  std::mt19937 rng(0x1234567);
  std::normal_distribution<float> gyro_noise(0.0f, 0.002f);  // rad/s
  std::normal_distribution<float> acc_noise(0.0f, 0.003f);   // g

  Pacer pacer;
  pacer.start(kImuPeriodNs);
  uint8_t buf[sizeof(DataHeader) + sizeof(ImuSample)];
  uint16_t udp_cnt = 0;
  LOG_INFO("imu streamer running (200 Hz)");

  while (!stop_.load(std::memory_order_relaxed)) {
    const uint64_t packed = target_.load(std::memory_order_acquire);
    ImuSample s{};
    s.gyro_x = gyro_noise(rng);
    s.gyro_y = gyro_noise(rng);
    s.gyro_z = gyro_noise(rng);
    s.acc_x = acc_noise(rng);
    s.acc_y = acc_noise(rng);
    s.acc_z = 1.0f + acc_noise(rng);
    const size_t n =
        build_data_packet(buf, kDataTypeImu, /*dot_num=*/1, udp_cnt++, /*frame_cnt=*/0,
                          realtime_ns(), reinterpret_cast<uint8_t*>(&s), sizeof(s),
                          /*time_interval=*/0);
    pacer.wait_next();
    sock.send_to(buf, n, static_cast<uint32_t>(packed >> 16),
                 static_cast<uint16_t>(packed & 0xFFFF));
  }
}

}  // namespace emu
