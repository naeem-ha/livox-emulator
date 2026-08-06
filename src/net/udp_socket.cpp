#include "net/udp_socket.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace emu {

UdpSocket::~UdpSocket() { close_fd(); }

void UdpSocket::close_fd() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool UdpSocket::open(const std::string& bind_ip, uint16_t port, bool broadcast,
                     bool nonblocking, std::string& err) {
  close_fd();
  fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0) {
    err = std::string("socket(): ") + strerror(errno);
    return false;
  }
  int one = 1;
  ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  if (broadcast) ::setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
  if (nonblocking) ::fcntl(fd_, F_SETFL, ::fcntl(fd_, F_GETFL, 0) | O_NONBLOCK);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr =
      (bind_ip.empty() || bind_ip == "0.0.0.0") ? htonl(INADDR_ANY) : parse_ip(bind_ip);
  if (addr.sin_addr.s_addr == INADDR_NONE) {
    err = "bad bind IP: " + bind_ip;
    close_fd();
    return false;
  }
  if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    err = "bind(" + bind_ip + ":" + std::to_string(port) + "): " + strerror(errno);
    close_fd();
    return false;
  }
  return true;
}

bool UdpSocket::send_to(const void* data, size_t len, uint32_t dst_ip_be, uint16_t dst_port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(dst_port);
  addr.sin_addr.s_addr = dst_ip_be;
  const ssize_t n = ::sendto(fd_, data, len, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  return n == static_cast<ssize_t>(len);
}

long UdpSocket::recv_from(void* buf, size_t cap, uint32_t& src_ip_be, uint16_t& src_port) {
  sockaddr_in addr{};
  socklen_t alen = sizeof(addr);
  const ssize_t n =
      ::recvfrom(fd_, buf, cap, 0, reinterpret_cast<sockaddr*>(&addr), &alen);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return -1;
  }
  src_ip_be = addr.sin_addr.s_addr;
  src_port = ntohs(addr.sin_port);
  return static_cast<long>(n);
}

int UdpSocket::send_mmsg(struct mmsghdr* msgs, unsigned n) {
  return ::sendmmsg(fd_, msgs, n, 0);
}

void UdpSocket::set_sndbuf(int bytes) {
  ::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes));
}

uint32_t UdpSocket::parse_ip(const std::string& s) { return ::inet_addr(s.c_str()); }

}  // namespace emu
