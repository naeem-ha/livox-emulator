#pragma once

#include <cstdint>
#include <string>

struct mmsghdr;

namespace emu {

// Thin RAII wrapper over a Linux UDP socket. Every emulator socket binds an
// explicit local port because Livox-SDK2 filters incoming packets by SOURCE
// port (56000/56100/56200/56300/56400) and silently drops mismatches.
class UdpSocket {
 public:
  UdpSocket() = default;
  ~UdpSocket();
  UdpSocket(const UdpSocket&) = delete;
  UdpSocket& operator=(const UdpSocket&) = delete;

  // bind_ip empty or "0.0.0.0" binds INADDR_ANY. Sets SO_REUSEADDR; enables
  // SO_BROADCAST when |broadcast| (needed to *receive* limited broadcasts too
  // on some paths, harmless otherwise).
  bool open(const std::string& bind_ip, uint16_t port, bool broadcast,
            bool nonblocking, std::string& err);
  void close_fd();

  int fd() const { return fd_; }
  bool valid() const { return fd_ >= 0; }

  bool send_to(const void* data, size_t len, uint32_t dst_ip_be, uint16_t dst_port);
  // Returns bytes received, 0 on EAGAIN, -1 on error.
  long recv_from(void* buf, size_t cap, uint32_t& src_ip_be, uint16_t& src_port);
  int send_mmsg(struct mmsghdr* msgs, unsigned n);

  void set_sndbuf(int bytes);

  // "a.b.c.d" -> network-byte-order u32 (INADDR_NONE on parse failure).
  static uint32_t parse_ip(const std::string& s);

 private:
  int fd_ = -1;
};

}  // namespace emu
