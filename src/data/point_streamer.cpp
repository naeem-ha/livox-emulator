#include "data/point_streamer.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>

#include <cstring>
#include <vector>

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

PointStreamer::PointStreamer(PointSource& source, std::string bind_ip, unsigned batch)
    : source_(source), bind_ip_(std::move(bind_ip)), batch_(batch) {}

PointStreamer::~PointStreamer() { stop(); }

void PointStreamer::set_target(uint32_t ip_be, uint16_t port) {
  const uint64_t packed = (static_cast<uint64_t>(ip_be) << 16) | port;
  const uint64_t prev = target_.exchange(packed, std::memory_order_release);
  if (!started_.exchange(true)) {
    thread_ = std::thread(&PointStreamer::run_, this);
  } else if (prev != packed) {
    LOG_INFO("point stream retargeted");
  }
}

void PointStreamer::stop() {
  stop_.store(true);
  if (thread_.joinable()) thread_.join();
}

void PointStreamer::run_() {
  UdpSocket sock;
  std::string err;
  if (!sock.open(bind_ip_, kPortPoint, false, false, err)) {
    LOG_ERROR("point streamer: %s", err.c_str());
    return;
  }
  sock.set_sndbuf(4 << 20);

  std::vector<std::array<uint8_t, kPointPacketBytes>> bufs(batch_);
  std::vector<mmsghdr> msgs(batch_);
  std::vector<iovec> iovs(batch_);
  sockaddr_in dst{};
  dst.sin_family = AF_INET;

  Pacer pacer;
  pacer.start(batch_ * kPointPeriodNs);

  PointPacketData pkt;
  uint16_t udp_cnt = 0;
  uint8_t frame_cnt = 0;
  uint64_t next_frame_ns = realtime_ns() + kFramePeriodNs;
  uint64_t sent = 0, last_report_sent = 0, last_resyncs = 0;
  uint64_t last_report_ns = realtime_ns();

  LOG_INFO("point streamer running (batch=%u, %.1f pkt/s)", batch_,
           1e9 / kPointPeriodNs);

  while (!stop_.load(std::memory_order_relaxed)) {
    const uint64_t packed = target_.load(std::memory_order_acquire);
    dst.sin_addr.s_addr = static_cast<uint32_t>(packed >> 16);
    dst.sin_port = htons(static_cast<uint16_t>(packed & 0xFFFF));

    for (unsigned i = 0; i < batch_; ++i) {
      source_.fill(pkt);
      const uint64_t ts = realtime_ns();
      if (ts >= next_frame_ns) {
        ++frame_cnt;
        next_frame_ns += kFramePeriodNs;
      }
      build_data_packet(bufs[i].data(), kDataTypeCartHigh, pkt.dot_num, udp_cnt++,
                        frame_cnt, ts, pkt.bytes.data(), kPointPayloadBytes,
                        kTimeIntervalPer96);
    }
    pacer.wait_next();
    if (batch_ == 1) {
      ::sendto(sock.fd(), bufs[0].data(), kPointPacketBytes, 0,
               reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    } else {
      for (unsigned i = 0; i < batch_; ++i) {
        iovs[i] = {bufs[i].data(), kPointPacketBytes};
        std::memset(&msgs[i], 0, sizeof(msgs[i]));
        msgs[i].msg_hdr.msg_name = &dst;
        msgs[i].msg_hdr.msg_namelen = sizeof(dst);
        msgs[i].msg_hdr.msg_iov = &iovs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
      }
      sock.send_mmsg(msgs.data(), batch_);
    }
    sent += batch_;

    const uint64_t now = realtime_ns();
    if (now - last_report_ns >= 1'000'000'000ull) {
      LOG_DEBUG("points: %.0f pkt/s, resyncs +%llu",
                (sent - last_report_sent) * 1e9 / (now - last_report_ns),
                static_cast<unsigned long long>(pacer.resyncs() - last_resyncs));
      last_report_ns = now;
      last_report_sent = sent;
      last_resyncs = pacer.resyncs();
    }
  }
}

}  // namespace emu
